#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "argparse/argparse.hpp"
#include "camera_config.hpp"
#include "depthai/depthai.hpp"
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

std::pair<uint32_t, uint32_t> mono_resolution_dims(const std::string& s) {
  if (s == "THE_400_P") return {640,  400};
  if (s == "THE_480_P") return {640,  480};
  if (s == "THE_720_P") return {1280, 720};
  if (s == "THE_800_P") return {1280, 800};
  throw std::invalid_argument("Unknown stereo_resolution: " + s);
}

// ---------------------------------------------------------------------------
// Display loop — capture thread decoupled from display thread
// ---------------------------------------------------------------------------

void run(const CameraConfig& cfg, quill::Logger* logger) {
  LOG_INFO(logger, "Starting stereo viewer: {}fps  resolution={}",
           cfg.stereo_fps, cfg.stereo_resolution);

  dai::Pipeline pipeline;

  const auto [w, h] = mono_resolution_dims(cfg.stereo_resolution);
  const float fps   = static_cast<float>(cfg.stereo_fps);

  auto left_cam = pipeline.create<dai::node::Camera>();
  left_cam->build(dai::CameraBoardSocket::CAM_B);
  auto* left_out = left_cam->requestOutput(
      {w, h}, dai::ImgFrame::Type::GRAY8,
      dai::ImgResizeMode::CROP, fps);

  auto right_cam = pipeline.create<dai::node::Camera>();
  right_cam->build(dai::CameraBoardSocket::CAM_C);
  auto* right_out = right_cam->requestOutput(
      {w, h}, dai::ImgFrame::Type::GRAY8,
      dai::ImgResizeMode::CROP, fps);

  // Queues must be registered BEFORE pipeline.start() so the graph is
  // wired correctly.
  auto q_left =
      left_out->createOutputQueue(/*maxSize=*/8, /*blocking=*/false);
  auto q_right =
      right_out->createOutputQueue(/*maxSize=*/8, /*blocking=*/false);

  pipeline.start();
  LOG_INFO(logger, "Stereo viewer running — press 'q' to quit.");

  // Shared state between capture and display threads.
  cv::Mat  shared_left;
  cv::Mat  shared_right;
  double   shared_cap_fps = 0.0;
  bool     fresh          = false;
  std::mutex frame_mtx;

  // Capture thread: drains both mono queues, measures capture FPS.
  //
  // lbuf/rbuf persist across iterations so that a dequeued frame from one
  // camera is never thrown away while waiting for the other.  Without this,
  // the tight poll loop can wake between the two hardware-synced arrivals
  // (~µs apart), dequeue left, find right not yet present, drop left and
  // sleep — then dequeue right, find left gone, drop right — wasting the
  // entire pair and halving effective FPS.
  std::jthread cap_thread([&](std::stop_token st) {
    auto t_prev = std::chrono::steady_clock::now();
    int  n      = 0;
    std::shared_ptr<dai::ImgFrame> lbuf;
    std::shared_ptr<dai::ImgFrame> rbuf;

    while (!st.stop_requested()) {
      if (!lbuf) lbuf = q_left->tryGet<dai::ImgFrame>();
      if (!rbuf) rbuf = q_right->tryGet<dai::ImgFrame>();

      if (!lbuf || !rbuf) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      // GRAY8 getCvFrame() wraps the ImgFrame's buffer without copying.
      // .clone() makes the Mat own its data so it is safe to hold after
      // lbuf/rbuf are released below.
      cv::Mat lf = lbuf->getCvFrame().clone();
      cv::Mat rf = rbuf->getCvFrame().clone();
      lbuf.reset();
      rbuf.reset();
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
        shared_left  = std::move(lf);
        shared_right = std::move(rf);
        fresh        = true;
        if (new_fps >= 0.0) shared_cap_fps = new_fps;
      }
    }
  });

  // Display loop: converts GRAY8→BGR, hconcat, overlays labels + FPS.
  auto t_prev = std::chrono::steady_clock::now();
  int  disp_n = 0;
  double disp_fps = 0.0;

  while (true) {
    cv::Mat left;
    cv::Mat right;
    double  cap_fps = 0.0;
    bool    got     = false;
    {
      std::lock_guard lk(frame_mtx);
      if (fresh) {
        left    = std::move(shared_left);
        right   = std::move(shared_right);
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

      cv::Mat left_bgr;
      cv::Mat right_bgr;
      cv::cvtColor(left,  left_bgr,  cv::COLOR_GRAY2BGR);
      cv::cvtColor(right, right_bgr, cv::COLOR_GRAY2BGR);

      cv::Mat combined;
      cv::hconcat(left_bgr, right_bgr, combined);

      // LEFT / RIGHT labels at bottom of each half.
      const int ch = left_bgr.rows;
      const int cw = left_bgr.cols;
      cv::putText(combined, "LEFT",  {10,      ch - 10},
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, {200, 200, 200}, 1);
      cv::putText(combined, "RIGHT", {cw + 10, ch - 10},
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, {200, 200, 200}, 1);

      if (cfg.show_fps) {
        cv::putText(
            combined,
            "CAP:  " + std::to_string(static_cast<int>(cap_fps)),
            {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2);
        cv::putText(
            combined,
            "DISP: " + std::to_string(static_cast<int>(disp_fps)),
            {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {200, 200, 200}, 2);
      }

      cv::imshow("OAK-D Lite — Stereo (Left | Right)", combined);
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
  quill::BackendOptions backend_opts;
  backend_opts.check_backend_singleton_instance = false;
  quill::Backend::start(backend_opts);

  auto sink =
      quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
  auto* logger =
      quill::Frontend::create_or_get_logger("view_stereo", std::move(sink));

  argparse::ArgumentParser program("view_stereo", "1.0.0");
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
