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

std::pair<uint32_t, uint32_t> mono_resolution_dims(const std::string& s) {
  if (s == "THE_400_P") return {640,  400};
  if (s == "THE_480_P") return {640,  480};
  if (s == "THE_720_P") return {1280, 720};
  if (s == "THE_800_P") return {1280, 800};
  throw std::invalid_argument("Unknown stereo_resolution: " + s);
}

// ---------------------------------------------------------------------------
// Display loop
// ---------------------------------------------------------------------------

void run(const CameraConfig& cfg, quill::Logger* logger) {
  LOG_INFO(logger, "Starting stereo viewer: {}fps  resolution={}", cfg.fps,
           cfg.stereo_resolution);

  dai::Pipeline pipeline;

  const auto [w, h] = mono_resolution_dims(cfg.stereo_resolution);
  const float fps = static_cast<float>(cfg.fps);

  auto left_cam = pipeline.create<dai::node::Camera>();
  left_cam->build(dai::CameraBoardSocket::CAM_B);
  auto* left_out = left_cam->requestOutput(
      {w, h}, dai::ImgFrame::Type::GRAY8, dai::ImgResizeMode::CROP, fps);

  auto right_cam = pipeline.create<dai::node::Camera>();
  right_cam->build(dai::CameraBoardSocket::CAM_C);
  auto* right_out = right_cam->requestOutput(
      {w, h}, dai::ImgFrame::Type::GRAY8, dai::ImgResizeMode::CROP, fps);

  pipeline.start();

  auto q_left  = left_out->createOutputQueue(/*maxSize=*/4, /*blocking=*/false);
  auto q_right = right_out->createOutputQueue(/*maxSize=*/4, /*blocking=*/false);

  auto t_prev = std::chrono::steady_clock::now();
  int frame_count = 0;
  double fps_display = 0.0;

  LOG_INFO(logger, "Stereo viewer running — press 'q' to quit.");

  while (true) {
    auto left_msg  = q_left->tryGet<dai::ImgFrame>();
    auto right_msg = q_right->tryGet<dai::ImgFrame>();

    if (left_msg && right_msg) {
      cv::Mat left_frame  = left_msg->getCvFrame();
      cv::Mat right_frame = right_msg->getCvFrame();
      ++frame_count;

      const auto now = std::chrono::steady_clock::now();
      const double elapsed =
          std::chrono::duration<double>(now - t_prev).count();
      if (elapsed >= 1.0) {
        fps_display = frame_count / elapsed;
        frame_count = 0;
        t_prev = now;
      }

      const std::string fps_label =
          "FPS: " + std::to_string(static_cast<int>(fps_display));

      if (cfg.show_fps) {
        cv::putText(left_frame,  fps_label, {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2);
        cv::putText(right_frame, fps_label, {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2);
      }

      cv::putText(left_frame,  "LEFT",  {10, left_frame.rows  - 10},
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, {200, 200, 200}, 1);
      cv::putText(right_frame, "RIGHT", {10, right_frame.rows - 10},
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, {200, 200, 200}, 1);

      cv::Mat left_bgr, right_bgr;
      cv::cvtColor(left_frame,  left_bgr,  cv::COLOR_GRAY2BGR);
      cv::cvtColor(right_frame, right_bgr, cv::COLOR_GRAY2BGR);

      cv::Mat combined;
      cv::hconcat(left_bgr, right_bgr, combined);
      cv::imshow("OAK-D Lite — Stereo (Left | Right)", combined);
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
