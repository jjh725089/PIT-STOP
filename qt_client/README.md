# Qt Client

## Overview

This program is a real-time fall detection and crowd density monitoring system built using **Qt**, **GStreamer**, **OpenCV**, and **MQTT** technologies. It integrates video streaming from multiple RTSP cameras, audio streaming via microphone, user authentication with SQLite, and event handling through MQTT messaging.

Key features include:

- RTSP video streaming for up to 4 cameras
- Fall detection alerting and event image logging
- Crowd count monitoring from multiple sources
- Encrypted audio transmission over TCP
- Secure MQTT client communication with TLS
- User registration and login system with camera coordinates

## Author

Hyun-ji Lee

Jeong-hyeon Cho

## Project Structure

- `main.cpp`: Application entry point.
- `mainwindow.{h,cpp}`: Login UI and user entry point.
- `signupwindow.{h,cpp}`: User registration dialog.
- `monitorwindow.{h,cpp}`: Main monitoring window with RTSP/MQTT integration.
- `rtspclientcamera1.{h,cpp}`: RTSP client with OpenCV (Indoor camera).
- `rtspclienttlsinteraction.{h,cpp}`: Custom TLS interaction for RTSP with Glib.
- `rtspclient.{h,cpp}`: Generic RTSP client (Gate cameras).
- `mqtt.{h,cpp}`: MQTT client handling subscriptions and events
- `databasemanager.{h,cpp}`: User and event DB access (SQLite)
- `microphone.{h,cpp}`: High-level coordinator for microphone input and network streaming.
- `microphone_socket.{h,cpp}`: Manages TCP connection and sends audio/metadata packets to the server.
- `microphone_input.{h,cpp}`: Captures raw audio and converts it from stereo float to mono int16.
- `audio_settings.{h,cpp}`: Defines the default audio format and handles JSON.

## Installation & Dependencies

### Required Libraries

- C++17 or later
- Qt 6.2 or later
- OpenCV >= 4.6 (with OpenCL support recommended)
- SQLite (provided by Qt)
- OpenSSL (for MQTT TLS and RTSPs)
- GStreamer 1.18+ with plugins:
  - `rtspsrc`, `appsink`, `videoconvert`, `decodebin`, `h264parse`, etc.

### Build Instructions

1. Clone this repository
2. Open in **Qt Creator** or use **CMake** with proper Qt path
3. Ensure GStreamer and OpenCV paths are configured
4. Build & Run

## Key Components

### GUI

- `MainWindow`: User login and MQTT trigger
- `SignupWindow`: User registration with camera coordinates
- `MonitorWindow`: Main dashboard for video, logs, alarms, and controls

### RTSP Streaming

- `RtspClientCamera1`: OpenCV pipeline for camera 1 with distortion correction
- `RtspClient`: General RTSP client for other cameras using `GstVideoOverlay`
- `RtspClientTlsInteraction`: Handles self-signed or custom CA TLS certificates

### Microphone & Audio

- `Microphone`: Audio streaming coordinator
- `MicrophoneInput`: Captures raw audio (stereo) and converts to mono
- `MicrophoneSocket`: TCP socket sender with metadata, PCM, and keep-alive

### MQTT Integration

- `Mqtt`: Secure MQTT client with topic subscriptions:
  - `pi/data/fall`: Fall detection
  - `main/result/log/`, `image/`: Event data and image
  - `main/data/Count`, `pop/#`: Crowd counts

- Publishes user-specific camera coordinates on login to `qt/data/exits`

### Database

- `DatabaseManager`: SQLite wrapper for:
  - User credentials and camera coordinates
  - Fall events with image path and raw JSON
  - User log history

## Notes

- Only indoor camera supports OpenCV image processing and distortion correction.
- The application uses **resource-embedded TLS certificates**, avoid system path dependency.
- GStreamer freeze detection and reconnection is enabled for all video streams.
- All sensitive signals (e.g., fall alerts) trigger visual alarms and DB logging in real-time.
- The system currently supports **single-user login** per session.

- Crowd count and fall event data can be simulated via MQTT tools (`mosquitto_pub`).
