#include <chrono>
#include <string>

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

std::pair<uint32_t, uint32_t> rgb_resolution_dims(const std::string& s) {
  if (s == "THE_1080_P") return {1920, 1080};
  if (s == "THE_4_K")    return {3840, 2160};
  if (s == "THE_12_MP")  return {4056, 3040};
  throw std::invalid_argument("Unknown rgb_resolution: " + s);
}

// ---------------------------------------------------------------------------
// Display loop
// ---------------------------------------------------------------------------

void run(const CameraConfig& cfg, quill::Logger* logger) {
  LOG_INFO(logger, "Starting RGB viewer: {}fps  resolution={}", cfg.fps,
           cfg.rgb_resolution);
  fprintf(stderr, "[diag] logger created\n"); fflush(stderr);

  dai::Pipeline pipeline;
  fprintf(stderr, "[diag] pipeline created\n"); fflush(stderr);

  auto cam = pipeline.create<dai::node::Camera>();
  cam->build(dai::CameraBoardSocket::CAM_A);
  fprintf(stderr, "[diag] camera built\n"); fflush(stderr);

  const auto [w, h] = rgb_resolution_dims(cfg.rgb_resolution);
  auto* out = cam->requestOutput(
      {w, h},
      dai::ImgFrame::Type::BGR888p,
      dai::ImgResizeMode::CROP,
      static_cast<float>(cfg.fps));
  fprintf(stderr, "[diag] output requested, connecting to device...\n"); fflush(stderr);

  pipeline.start();
  fprintf(stderr, "[diag] device connected, starting loop\n"); fflush(stderr);

  auto queue = out->createOutputQueue(/*maxSize=*/4, /*blocking=*/false);

  auto t_prev = std::chrono::steady_clock::now();
  int frame_count = 0;
  double fps_display = 0.0;

  LOG_INFO(logger, "RGB viewer running — press 'q' to quit.");

  int no_frame_count = 0;
  while (true) {
    auto frame_msg = queue->tryGet<dai::ImgFrame>();
    if (!frame_msg) {
      if (++no_frame_count % 1000 == 0)
        LOG_WARNING(logger, "No frame received yet ({} misses)", no_frame_count);
    }
    if (frame_msg) {
      cv::Mat frame = frame_msg->getCvFrame();
      ++frame_count;

      const auto now = std::chrono::steady_clock::now();
      const double elapsed =
          std::chrono::duration<double>(now - t_prev).count();
      if (elapsed >= 1.0) {
        fps_display = frame_count / elapsed;
        frame_count = 0;
        t_prev = now;
      }

      if (cfg.show_fps) {
        cv::putText(frame,
                    "FPS: " + std::to_string(static_cast<int>(fps_display)),
                    {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 0}, 2);
      }

      cv::imshow("OAK-D Lite — RGB", frame);
    }

    if (cv::waitKey(1) == 'q') break;
  }

  cv::destroyAllWindows();
}

}  // namespace navfield

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  quill::Backend::start();
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
