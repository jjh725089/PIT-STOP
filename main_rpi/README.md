# Main Server

## Overview

This program is the main event-driven controller that orchestrates the full system pipeline: fall detection, crowd counting, congestion analysis, optimal exit pathfinding, visualization, logging, MQTT communication, and audio playback. It responds to various MQTT events, manages asynchronous image detection via `inotify`, and triggers inference and rendering steps accordingly.
This file serves as the runtime entry point and integrates all modules, handling both real-time fall incidents and periodic crowd assessments.

## Author

KyungMin Mok

## Project Structure

All logic is implemented within a single source file. It contains the following logics:

- Event loop & MQTT handler
- Image watch & processing threads
- Fall and crowd detection logic
- Congestion heatmap analysis
- A* pathfinding with congestion score
- Visualization & result rendering
- Log saving & MQTT reporting
- Audio playback via speaker module

## Installation & Dependencies

- OpenCV >= 4.6
- C++17 or later
- ONNX Runtime >= 1.17.0
- ALSA development libraries
- Nlohmann Json
- POSIX APIs (`inotify`, `poll`, `unistd`)
- Eclipse Paho MQTT C++ client

Make sure the ONNX Runtime directory is correctly referenced in the `Makefile`.

## Build & Run Instructions

### Build

Use the provided Makefile to compile the project. It includes all necessary flags for ONNX Runtime, OpenCV, MQTT, and ALSA.

```make
make
```
This will generate an executable file.

### Run

To run the program:

```make
make run
```

### Debug (optional)

To launch with GDB:

```make
make gdb
```

To check for memory leaks using Valgrind:

```make
make valgrind
```

### Clean

To remove the build output and log files:

```make
make clean
```

## Key Components

### `main()`

Initializes fall and crowd detectors, MQTT client, and speaker server. Subscribes to MQTT topics and starts background threads for event processing and LED auto-reset logic.

### `MainCallback::message_arrived()`

MQTT callback handler that processes messages from various topics:

- `pi/data/fall`: triggers fall image capture and detection
- `qt/data/exits`: updates dynamic exit points for pathfinding
- `sub/capture/#`: receives crowd count from sub-cameras and evaluates escape routes
- `pi/data/Count`: triggers periodic people counting from CH1
- `qt/off`: cancels current emergency and resets all state

### `waitAndProcessNew1jpg()`

Monitors the `./cap_repo/` directory for new fall-related image files using `inotify`. Once an image is found:

- Loads the image
- Extracts fall timestamp
- Runs fall detection via `findFallOnCH1()`
- Saves the image into a time-stamped folder

### `findFallOnCH1()`

Runs the fall detector and processes its results:

- Determines fall center coordinates
- Annotates fall and safe person positions
- Generates a visualized image and saves as `result.jpg`

### `crowdCountingSub()`

Receives crowd count from sub Raspberry Pi units. Once all sub-cameras have responded, it triggers `controlGateLed()` to determine the optimal gate.

### `waitAndProcessPeriodicjpg()`

Handles periodic mode crowd image arrival using `inotify`, and performs processing via `crowdCountingPeriodic()`.

### `crowdCountingPeriodic()`

Runs crowd detection using the fall detector (as a proxy for person count), then publishes the result to `main/data/Count`.

### `controlGateLed()`

Uses the following to determine which exit gate to activate:

- Crowd positions
- Congestion heatmap (`CongestionAnalyzer`)
- Exit list (MQTT or fallback)
- Path score computation (`Pathfinder`)

Renders congestion and path result to `path.jpg` and publishes the selected gate over MQTT.

### `saveFallLog()`

Saves metadata and inference results to a JSON log. Also publishes:

- The log file to `main/result/log/`
- The path image to `main/result/image/`
- Any errors to `main/result/error/`

### `safeMoveImage()` / `safeDeleteImage()`

Handles file I/O with locking to move or delete image files safely in a multithreaded context.

### `millis()`

Returns the current time in milliseconds (used for measuring event duration).

### `periodicPublishThread()`

Publishes an empty heartbeat message to `main/data/periodic` every 10 seconds.

## Notes

- A speaker client must connect to the socket to play alert audio.
- All MQTT communication is assumed to be secured via TLS (port 8883).
- Image artifacts are saved to:
  - `./prev_cap_repo/<timestamp>/result.jpg`
  - `./prev_cap_repo/<timestamp>/path.jpg`
- Event metadata is stored in `./log/<timestamp>.json`
- LED control assumes sub Pi units respond to MQTT LED activation topics.
- System automatically resets after 60 seconds or when `qt/off` is received.