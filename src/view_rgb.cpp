#include <chrono>
#include <mutex>
#include <sstream>
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

  auto encoder = pipeline.create<dai::node::VideoEncoder>();
  encoder->setDefaultProfilePreset(static_cast<float>(cfg.rgb_fps),
                                   dai::VideoEncoderProperties::Profile::MJPEG);
  encoder->setQuality(80);
  out->link(encoder->input);

  auto queue = encoder->out.createOutputQueue(8, false);
  pipeline.start();

  cv::Mat shared_frame;
  double shared_cap_fps = 0.0;
  bool fresh = false;
  std::mutex frame_mtx;

  std::jthread cap_thread([&](std::stop_token st) {
    auto t_prev = std::chrono::steady_clock::now();
    int n = 0;

    while (!st.stop_requested()) {
      auto msg = queue->tryGet<dai::EncodedFrame>();
      if (!msg) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      const auto& raw = msg->getData();
      cv::Mat buf(1, static_cast<int>(raw.size()), CV_8UC1,
                  const_cast<uint8_t*>(raw.data()));
      cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);
      if (frame.empty()) continue;
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
        shared_frame = std::move(frame);
        fresh = true;
        if (new_fps >= 0.0) shared_cap_fps = new_fps;
      }
    }
  });

  auto t_prev = std::chrono::steady_clock::now();
  int disp_n = 0;
  double disp_fps = 0.0;

  while (true) {
    cv::Mat frame;
    double cap_fps = 0.0;
    bool got = false;
    {
      std::lock_guard lk(frame_mtx);
      if (fresh) {
        frame = std::move(shared_frame);
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

      if (cfg.show_fps) {
        cv::putText(frame, "CAP:  " + std::to_string(static_cast<int>(cap_fps)),
                    {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 0}, 2);
        cv::putText(frame,
                    "DISP: " + std::to_string(static_cast<int>(disp_fps)),
                    {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 200, 0}, 2);
      }

      cv::imshow("OAK-D Lite — RGB", frame);
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
