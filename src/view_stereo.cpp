#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "argparse/argparse.hpp"
#include "camera_config.hpp"
#include "depthai/depthai.hpp"
#include "opencv2/opencv.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"

namespace navfield {

std::pair<uint32_t, uint32_t> mono_resolution_dims(const std::string& s) {
  if (s == "THE_480_P") return {640, 480};
  throw std::invalid_argument("Unknown stereo_resolution: " + s);
}

void run(const CameraConfig& cfg) {
  auto device = std::make_shared<dai::Device>();

  auto* logger = quill::Frontend::get_logger("view_stereo");
  std::ostringstream usb_ss;
  usb_ss << device->getUsbSpeed();
  LOG_INFO(logger, "Device name: {}, Product: {}, MxId: {}, USB: {}, IMU: {}",
           device->getDeviceName(), device->getProductName(),
           device->getMxId(), usb_ss.str(), device->getConnectedIMU());

  dai::Pipeline pipeline(device);

  const auto [w, h] = mono_resolution_dims(cfg.stereo_resolution);
  const float fps = static_cast<float>(cfg.stereo_fps);

  auto left_cam = pipeline.create<dai::node::Camera>();
  left_cam->build(dai::CameraBoardSocket::CAM_B);
  auto* left_out = left_cam->requestOutput({w, h}, dai::ImgFrame::Type::GRAY8,
                                           dai::ImgResizeMode::CROP, fps);

  auto right_cam = pipeline.create<dai::node::Camera>();
  right_cam->build(dai::CameraBoardSocket::CAM_C);
  auto* right_out = right_cam->requestOutput({w, h}, dai::ImgFrame::Type::GRAY8,
                                             dai::ImgResizeMode::CROP, fps);

  auto q_left = left_out->createOutputQueue(8, false);
  auto q_right = right_out->createOutputQueue(8, false);
  pipeline.start();

  cv::Mat shared_left, shared_right;
  double shared_cap_fps = 0.0;
  bool fresh = false;
  std::mutex frame_mtx;

  std::jthread cap_thread([&](std::stop_token st) {
    auto t_prev = std::chrono::steady_clock::now();
    int n = 0;
    std::shared_ptr<dai::ImgFrame> lbuf, rbuf;

    while (!st.stop_requested()) {
      if (!lbuf) lbuf = q_left->tryGet<dai::ImgFrame>();
      if (!rbuf) rbuf = q_right->tryGet<dai::ImgFrame>();
      if (!lbuf || !rbuf) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      cv::Mat lf = lbuf->getCvFrame().clone();
      cv::Mat rf = rbuf->getCvFrame().clone();
      lbuf.reset();
      rbuf.reset();
      ++n;

      const auto now = std::chrono::steady_clock::now();
      const double dt = std::chrono::duration<double>(now - t_prev).count();
      double new_fps = -1.0;
      if (dt >= 1.0) {
        new_fps = n / dt;
        n = 0;
        t_prev = now;
      }

      {
        std::lock_guard lk(frame_mtx);
        shared_left = std::move(lf);
        shared_right = std::move(rf);
        fresh = true;
        if (new_fps >= 0.0) shared_cap_fps = new_fps;
      }
    }
  });

  auto t_prev = std::chrono::steady_clock::now();
  int disp_n = 0;
  double disp_fps = 0.0;

  while (true) {
    cv::Mat left, right;
    double cap_fps = 0.0;
    bool got = false;
    {
      std::lock_guard lk(frame_mtx);
      if (fresh) {
        left = std::move(shared_left);
        right = std::move(shared_right);
        cap_fps = shared_cap_fps;
        fresh = false;
        got = true;
      }
    }

    if (got) {
      ++disp_n;
      const auto now = std::chrono::steady_clock::now();
      const double dt = std::chrono::duration<double>(now - t_prev).count();
      if (dt >= 1.0) {
        disp_fps = disp_n / dt;
        disp_n = 0;
        t_prev = now;
      }

      cv::Mat left_bgr, right_bgr;
      cv::cvtColor(left, left_bgr, cv::COLOR_GRAY2BGR);
      cv::cvtColor(right, right_bgr, cv::COLOR_GRAY2BGR);

      cv::Mat combined;
      cv::hconcat(left_bgr, right_bgr, combined);

      const int ch = left_bgr.rows;
      const int cw = left_bgr.cols;
      cv::putText(combined, "LEFT", {10, ch - 10}, cv::FONT_HERSHEY_SIMPLEX,
                  0.7, {200, 200, 200}, 1);
      cv::putText(combined, "RIGHT", {cw + 10, ch - 10},
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, {200, 200, 200}, 1);

      if (cfg.show_fps) {
        cv::putText(
            combined, "CAP:  " + std::to_string(static_cast<int>(cap_fps)),
            {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 2);
        cv::putText(
            combined, "DISP: " + std::to_string(static_cast<int>(disp_fps)),
            {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {200, 200, 200}, 2);
      }

      cv::imshow("OAK-D Lite — Stereo (Left | Right)", combined);
    }

    if (cv::waitKey(1) == 'q') break;
  }

  cv::destroyAllWindows();
}

}  // namespace navfield

int main(int argc, char* argv[]) {
  quill::BackendOptions backend_opts;
  backend_opts.check_backend_singleton_instance = false;
  quill::Backend::start(backend_opts);

  quill::Frontend::create_or_get_logger(
      "view_stereo",
      quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"));

  argparse::ArgumentParser program("view_stereo", "1.0.0");
  program.add_argument("--config")
      .default_value(std::string{"config/camera.json"})
      .help("Path to camera.json");
  program.parse_args(argc, argv);

  navfield::run(
      navfield::load_camera_config(program.get<std::string>("--config")));

  quill::Backend::stop();
  return 0;
}
