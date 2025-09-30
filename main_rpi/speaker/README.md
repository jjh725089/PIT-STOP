# Speaker

## Overview

This module implements a TCP-based audio playback server designed for Linux environments. The server receives audio playback settings and raw audio frames over a network socket, parses and applies metadata (such as sample rate, format, and channels), and plays back the audio using the ALSA (Advanced Linux Sound Architecture) API.
It is capable of real-time streaming and features timestamp-based frame validation, thread-safe audio queuing, and graceful shutdown handling.

## Author

Jooho Hwang

## Project Structure

- `speaker.{h,cpp}`: High-level controller that manages socket handling and audio playback.
- `speaker_socket.{h,cpp}`: TCP socket server for client connections and data reception.
- `playback_worker.{h,cpp}`: Worker thread that handles ALSA playback and manages audio buffering.
- `audio_settings.{h,cpp}`: Parses JSON-based audio settings and maps to ALSA formats.

## Installation & Dependencies

- Linux system (ALSA supported)
- C++17 or later
- ALSA development libraries
- Nlohmann Json libraries

## Key Components

### Speaker class

The central orchestrator that initializes the socket server, parses packets, sets up playback via PlaybackWorker, and handles graceful shutdowns.

### SpeakerSocket class

Implements a non-blocking TCP socket server that accepts clients and receives packets containing an 8-byte header and a variable-size payload. Supports metadata and audio frame types.

### PlaybackWorker class

Handles the playback logic using a dedicated thread. Audio frames are queued with timestamps, and outdated frames (over 5ms delay) are discarded. Uses snd_pcm_writei for output.

### AudioSettings class

Parses audio configuration parameters (e.g., "sample_rate": 48000, "format": "int16") from a JSON string and converts the format to the ALSA-compatible enum.

## Notes

- Frames are timestamped upon reception.
- Playback loop ensures low-latency operation by skipping frames delayed over 5ms.
- Set your speaker output to maximum volume by alsamixer.
- Should allow port 8888.
- For integration into your own application, you may run it as a background process or within a dedicated thread.