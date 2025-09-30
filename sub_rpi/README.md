# Sub Server

## Overview

This program is designed to run on a sub Raspberry Pi unit. It provides the following features:

- Launches a TLS-enabled RTSP streaming server using GStreamer.
- Captures and processes camera frames in real time via OpenCV.
- Performs crowd detection using a YOLO-based ONNX model.
- Responds to MQTT messages for real-time or periodic image capture.
- Publishes the detected people count to the main Raspberry Pi via MQTT.
- Controls an external LED device through GPIO.
- Automatically adjusts camera white balance (AWB) based on frame brightness.

## Author

KyungMin Mok

## Project Structure

All functionality is implemented in a single source file: `sub_rtsp_server.cpp`, which includes:

- GStreamer RTSP server setup with TLS support
- Frame capture using a GStreamer pad probe
- Auto White Balance (AWB) control logic
- ONNX-based people detection
- MQTT connectivity and message handling
- LED control via GPIO interface

## Installation & Dependencies

This program requires the following dependencies:

- OpenCV ≥ 4.6
- C++17 or later
- ONNX Runtime ≥ 1.17.0
- Eclipse Paho MQTT C++ client
- GStreamer with TLS support (`rtsp-server`, `libcamerasrc`, etc.)
- GLib and GObject
- TLS certificate and key:
  - `/opt/rtsp/server.cert.pem`
  - `/opt/rtsp/server.key.pem`

Ensure the ONNX Runtime path is correctly referenced in the `Makefile`.

## Build & Run Instructions

### Build

Use the provided Makefile to compile:

```make
make
```

This will produce an executable file.

### Run

To launch the server, specify the sub_id as a command-line argument:


```make
sudo ./sub_rtsp_server <sub_id>
```

This determines MQTT topics and LED device path for this unit.

### Clean

To remove the executable:

```make
make clean
```

## Key Components

### `main()`

Initializes all core components:

- GStreamer RTSP server and TLS configuration
- RTSP media pipeline setup
- MQTT connection thread
- LED path and topic routing based on `sub_id`

### `mqtt_thread_func()`

Manages the MQTT connection to the main Raspberry Pi. Subscribes to:

- `main/data/cap` — real-time image capture request
- `main/data/periodic` — periodic capture trigger
- `sub/led/on/<id>` — turn on LED
- `qt/off` — turn off LED

Processes all messages using an internal callback.

### `callback::message_arrived()`

Handles MQTT messages as follows:

- On capture requests (`main/data/cap`, `main/data/periodic`), captures a frame and triggers detection.
- On LED control messages, toggles `/dev/gpioled<id>` accordingly.
- Publishes results via `sub/capture/<id>` or `pop/<id>`.

### `process_frame_and_publish()`

Processes the most recent captured frame:

- Detects people using `CrowdDetector::detect()`.
- Publishes the count to the appropriate MQTT topic.

### `buffer_probe_cb()`

GStreamer pad probe callback:

- Intercepts frames in NV21 format.
- Converts to BGR using OpenCV.
- Stores a resized copy of the latest frame.
- Calls `check_and_apply_awb()` for AWB evaluation.

### `check_and_apply_awb()`

Analyzes the brightness ratio of each frame:
- Turns AWB **ON** if bright pixels make up 5–40% of the image.
- Turns AWB **OFF** if bright pixels are below 2%.
- Cooldown interval: 2 seconds between adjustments.

### `media_prepared_cb()`

Callback when RTSP media is prepared:

- Attaches `buffer_probe_cb()` to the `capture_queue` sink pad.
- Captures the camera source element for AWB control.

### `turn_on_led()` / `turn_off_led()`

Writes `"1"` or `"0"` to `/dev/gpioled<sub_id>` to toggle the LED state.

### `intr_handler()`

Handles graceful termination (SIGINT), shuts down the GStreamer main loop and MQTT thread.

## Notes

- **RTSP Stream URL:** `rtsps://<pi-ip>:8555/stream`
- Requires a TLS-capable RTSP client (e.g., VLC, ffmpeg)
- TLS certificate and key must be present in `/opt/rtsp/`
- The camera must support `libcamerasrc`. If not available, a software fallback pipeline is used.
- Pass `sub_id` as a command-line argument to configure MQTT topics and LED path.
- LED device path: `/dev/gpioled<sub_id>` (e.g., `/dev/gpioled1`)
- Input frame format from the camera must be `NV21`.
- Graceful shutdown is handled via `Ctrl+C` (SIGINT).