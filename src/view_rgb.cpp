#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "argparse/argparse.hpp"
#include "camera_config.hpp"
#include "depthai/depthai.hpp"
#include "depthai/pipeline/datatype/EncodedFrame.hpp"
#include "opencv2/opencv.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

namespace navfield {

// ---------------------------------------------------------------------------
// Resolution string → pixel dimensions
// ---------------------------------------------------------------------------

std::pair<uint32_t, uint32_t> rgb_resolution_dims(const std::string& s) {
  if (s == "THE_1080_P") return {1920, 1080};
  if (s == "THE_4_K")    return {3840, 2160};
  if (s == "THE_12_MP")  return {4056, 3040};
  throw std::invalid_argument("Unknown rgb_resolution: " + s);
}

// ---------------------------------------------------------------------------
// Display loop — capture thread decoupled from display thread
// ---------------------------------------------------------------------------

void run(const CameraConfig& cfg, quill::Logger* logger) {
  LOG_INFO(logger, "Starting RGB viewer: {}fps  resolution={}",
           cfg.rgb_fps, cfg.rgb_resolution);

  dai::Pipeline pipeline;

  auto cam = pipeline.create<dai::node::Camera>();
  cam->build(dai::CameraBoardSocket::CAM_A);

  const auto [w, h] = rgb_resolution_dims(cfg.rgb_resolution);

  // NV12 is the ISP's native output (1.5 bytes/pixel).  Feed it into an on-device
  // MJPEG encoder so each frame is ~150–300 KB over USB instead of ~3 MB —
  // essential on USB 2 (~43 MB/s ceiling).
  // NOTE: the OAK-D Lite VPU MJPEG encoder tops out at 1080p; 4K is not supported.
  // Use encoder->out (EncodedFrame), NOT encoder->bitstream (ImgFrame legacy API
  // that never produces frames in this depthai version).
  auto* out = cam->requestOutput(
      {w, h},
      dai::ImgFrame::Type::NV12,
      dai::ImgResizeMode::CROP,
      static_cast<float>(cfg.rgb_fps));

  auto encoder = pipeline.create<dai::node::VideoEncoder>();
  encoder->setDefaultProfilePreset(
      static_cast<float>(cfg.rgb_fps),
      dai::VideoEncoderProperties::Profile::MJPEG);
  // Quality 80: ~1 MB/frame at 4K, ~200 KB at 1080p — well within USB-2 ceiling.
  encoder->setQuality(80);
  out->link(encoder->input);

  // Queue must be registered BEFORE pipeline.start() so the graph is wired.
  // encoder->out (EncodedFrame) and encoder->bitstream (ImgFrame) are mutually
  // exclusive — use encoder->out.
  auto queue = encoder->out.createOutputQueue(/*maxSize=*/8, /*blocking=*/false);

  pipeline.start();
  LOG_INFO(logger, "RGB viewer running — press 'q' to quit.");

  // Shared state between capture and display threads.
  cv::Mat  shared_frame;
  double   shared_cap_fps = 0.0;
  bool     fresh          = false;
  std::mutex frame_mtx;

  // Capture thread: drains the device queue as fast as the camera delivers
  // frames; measures capture FPS independently of display.
  std::jthread cap_thread([&](std::stop_token st) {
    auto t_prev = std::chrono::steady_clock::now();
    int  n    = 0;
    int  miss = 0;
    while (!st.stop_requested()) {
      auto msg = queue->tryGet<dai::EncodedFrame>();
      if (!msg) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (++miss % 1000 == 0)
          LOG_WARNING(logger, "No RGB frame ({} misses)", miss);
        continue;
      }
      // MJPEG bitstream → BGR.  imdecode allocates the output independently.
      const auto& raw = msg->getData();
      cv::Mat buf(1, static_cast<int>(raw.size()), CV_8UC1,
                  const_cast<uint8_t*>(raw.data()));
      cv::Mat decoded = cv::imdecode(buf, cv::IMREAD_COLOR);
      if (decoded.empty()) {
        LOG_WARNING(logger, "MJPEG decode failed, skipping frame");
        continue;
      }
      ++n;

      const auto   now = std::chrono::steady_clock::now();
      const double dt  =
          std::chrono::duration<double>(now - t_prev).count();
      double new_fps = -1.0;
      if (dt >= 1.0) {
        new_fps = n / dt;
        n       = 0;
        t_prev  = now;
      }

      {
        std::lock_guard lk(frame_mtx);
        shared_frame = std::move(decoded);
        fresh        = true;
        if (new_fps >= 0.0) shared_cap_fps = new_fps;
      }
    }
  });

  // Display loop: picks up the latest captured frame, overlays both
  // capture FPS and display FPS, then renders.
  auto t_prev = std::chrono::steady_clock::now();
  int  disp_n = 0;
  double disp_fps = 0.0;

  while (true) {
    cv::Mat frame;
    double  cap_fps = 0.0;
    bool    got     = false;
    {
      std::lock_guard lk(frame_mtx);
      if (fresh) {
        frame   = std::move(shared_frame);
        cap_fps = shared_cap_fps;
        fresh   = false;
        got     = true;
      }
    }

    if (got) {
      ++disp_n;
      const auto   now = std::chrono::steady_clock::now();
      const double dt  =
          std::chrono::duration<double>(now - t_prev).count();
      if (dt >= 1.0) {
        disp_fps = disp_n / dt;
        disp_n   = 0;
        t_prev   = now;
      }

      if (cfg.show_fps) {
        cv::putText(
            frame,
            "CAP:  " + std::to_string(static_cast<int>(cap_fps)),
            {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 0}, 2);
        cv::putText(
            frame,
            "DISP: " + std::to_string(static_cast<int>(disp_fps)),
            {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 200, 0}, 2);
      }

      cv::imshow("OAK-D Lite — RGB", frame);
    }

    if (cv::waitKey(1) == 'q') break;
  }

  // jthread destructor: calls request_stop() then join() — capture thread
  // exits within ~1 ms (sleep granularity) before any shared state is torn
  // down.
  cv::destroyAllWindows();
}

}  // namespace navfield

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  // Disable BackendWorkerLock (POSIX named semaphore) — it crashes on macOS
  // when depthai-core is loaded, because depthai's static initializers
  // (spdlog, dcl) corrupt the semaphore state before main() runs.
  quill::BackendOptions backend_opts;
  backend_opts.check_backend_singleton_instance = false;
  quill::Backend::start(backend_opts);

  auto sink =
      quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
  auto* logger =
      quill::Frontend::create_or_get_logger("view_rgb", std::move(sink));

  argparse::ArgumentParser program("view_rgb", "1.0.0");
  program.add_argument("--config")
      .default_value(std::string{"config/camera.json"})
      .help("Path to camera.json");
  program.parse_args(argc, argv);

  const navfield::CameraConfig cfg =
      navfield::load_camera_config(program.get<std::string>("--config"));

  navfield::run(cfg, logger);

  quill::Backend::stop();
  return 0;
}
