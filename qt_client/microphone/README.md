# Microphone

## Overview

This module implements capturing live microphone input using Qt's multimedia framework and streams the processed audio data to a remote server via TCP. It converts stereo float audio into mono int16 format for efficient transmission and includes a Qt-based UI for user control.
The system is designed for low-latency, real-time audio streaming, and includes support for sending audio format metadata and maintaining the connection using keep-alive silent packets.

## Author

Jooho Hwang

## Project Structure

- `microphone_widget.{h,cpp}`: Provides a Qt-based user interface to control streaming (IP, port, start/stop).
- `microphone.{h,cpp}`: High-level coordinator for microphone input and network streaming.
- `microphone_socket.{h,cpp}`: Manages TCP connection and sends audio/metadata packets to the server.
- `microphone_input.{h,cpp}`: Captures raw audio and converts it from stereo float to mono int16.
- `audio_settings.{h,cpp}`: Defines the default audio format and handles JSON.

## Installation & Dependencies

### Requirements

- C++17 or later
- Qt 6.2 or later
- Nlohmann Json libraries

## Key Components

### MicrophoneWidget class

Provides a graphical interface for entering the server IP and port, and for starting/stopping the microphone streaming session.

### Microphone class

Acts as a controller, coordinating the audio input and socket output. Handles full lifecycle of capture and transmission.

### MicrophoneSocket class

Handles TCP connection lifecycle, transmits audio and JSON metadata to the server, and maintains a heartbeat via periodic silent packets.

### MicrophoneInput class

Captures real-time audio using QAudioSource, downmixes stereo float audio to mono int16 PCM, and emits the result via a Qt signal.

### AudioSettings class

Provides a default audio format (48kHz, stereo, float) and utility functions to serialize/deserialize audio settings using JSON.

## Notes

- All audio is transmitted in mono int16 PCM format at 48 kHz, regardless of the original capture format.
- A custom binary protocol is used to distinguish between audio data and metadata packets via protocol headers.
- To maintain a persistent connection, the system periodically sends silent packets as a keep-alive mechanism when no audio input is present.
- The current implementation does not include features such as authentication, encryption, or advanced error recovery, aside from basic connection retries.
- `MicrophoneWidget` provides a minimal reference implementation of the user interface. For production use, it can be extended or integrated into a larger Qt application by modifying its exposed methods and member variables.