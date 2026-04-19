#include <chrono>
#include <sstream>
#include <string>

#include "argparse/argparse.hpp"
#include "camera_config.hpp"
#include "depthai/depthai.hpp"
#include "opencv2/opencv.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"

namespace navfield {

std::pair<uint32_t, uint32_t> rgb_resolution_dims(const std::string& s) {
  if (s == "THE_1080_P") return {1920, 1080};
  if (s == "THE_4_K")    return {3840, 2160};
  throw std::invalid_argument("Unknown rgb_resolution: " + s);
}

void run(const CameraConfig& cfg) {
  auto device = std::make_shared<dai::Device>();

  auto* logger = quill::Frontend::get_logger("view_rgb");
  std::ostringstream usb_ss;
  usb_ss << device->getUsbSpeed();
  LOG_INFO(logger, "Device name: {}, Product: {}, MxId: {}, USB: {}, IMU: {}",
           device->getDeviceName(), device->getProductName(),
           device->getMxId(), usb_ss.str(), device->getConnectedIMU());

  dai::Pipeline pipeline(device);

  const auto [w, h] = rgb_resolution_dims(cfg.rgb_resolution);

  auto cam = pipeline.create<dai::node::Camera>();
  cam->build(dai::CameraBoardSocket::CAM_A);
  auto* out = cam->requestOutput({w, h}, dai::ImgFrame::Type::NV12,
                                 dai::ImgResizeMode::CROP,
                                 static_cast<float>(cfg.rgb_fps));

  auto queue = out->createOutputQueue(8, false);
  pipeline.start();

  auto t_prev = std::chrono::steady_clock::now();
  int n = 0;
  double fps = 0.0;

  while (true) {
    auto msg = queue->get<dai::ImgFrame>();
    cv::Mat frame = msg->getCvFrame();

    ++n;
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - t_prev).count();
    if (dt >= 1.0) {
      fps = n / dt;
      n = 0;
      t_prev = now;
    }

    if (cfg.show_fps) {
      cv::putText(frame, "FPS: " + std::to_string(static_cast<int>(fps)),
                  {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 0}, 2);
    }

    cv::imshow("OAK-D Lite — RGB", frame);
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
      "view_rgb",
      quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"));

  argparse::ArgumentParser program("view_rgb", "1.0.0");
  program.add_argument("--config")
      .default_value(std::string{"config/camera.json"})
      .help("Path to camera.json");
  program.parse_args(argc, argv);

  navfield::run(
      navfield::load_camera_config(program.get<std::string>("--config")));

  quill::Backend::stop();
  return 0;
}
