# Package Installation Guide

## Table of Contents

1. [Basic Development Tools]
2. [OpenCV 4.11.0]
3. [ONNX Runtime 1.17.0]
4. [GStreamer]
5. [OpenSSL]
6. [MQTT (Eclipse Paho)]
7. [Qt (via Qt Creator & Maintenance Tool)]
8. [Post-install Settings]

## Basic Development Tools

```bash
sudo apt update
sudo apt install -y \\
    vim build-essential cmake valgrind \\
    libasound2-dev nlohmann-json3-dev \\
    libx264-dev libxvidcore-dev
```
    
## OpenCV 4.11.0

### Memory Swap Configuration

Before installing OpenCV, increase swap memory:

```bash
sudo vi /sbin/dphys-swapfile   # Set CONF_MAXSWAP=4096
sudo vi /etc/dphys-swapfile    # Set CONF_SWAPSIZE=4096
sudo vi /boot/firmware/config.txt  # Set gpu_mem=128
sudo reboot
```

### Installation Script

```bash
wget https://github.com/Qengineering/Install-OpenCV-Raspberry-Pi-64-bits/raw/main/OpenCV-4-11-0.sh
chmod 755 OpenCV-4-11-0.sh
./OpenCV-4-11-0.sh
```

Note: Edit the script to ensure GStreamer support (-D WITH_GSTREAMER=ON), and set additional flags as required by your system.

## ONNX Runtime

```bash
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-aarch64-1.17.0.tgz
tar -xzvf onnxruntime-linux-aarch64-1.17.0.tgz
```

Make sure to:

- Point Makefile's -I and -L flags to this directory
- Export LD_LIBRARY_PATH if needed

## GStreamer

GStreamer is required for RTSP streaming and OpenCV capture.

```bash
sudo apt install -y \\
    libgstreamer1.0-dev gstreamer1.0-gtk3 \\
    libgstreamer-plugins-base1.0-dev gstreamer1.0-gl \\
    gstreamer1.0-tools gstreamer1.0-libav \\
    gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
```
    
To enable libcamera pipeline (on newer Raspberry Pi OS Bookworm):

```bash
sudo apt install -y gstreamer1.0-libcamera
```

## OpenSSL

Required for secure MQTT and RTSP (TLS support):

```bash
sudo apt install -y libssl-dev openssl
```

Certificates for RTSP server must be located at:

```bash
/opt/rtsp/server.cert.pem
/opt/rtsp/server.key.pem
```

Generate a self-signed certificate (if needed):

```bash
sudo mkdir -p /opt/rtsp
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \\
    -keyout /opt/rtsp/server.key.pem \\
    -out /opt/rtsp/server.cert.pem
```

## MQTT (Eclipse Paho)

Install Eclipse Paho C++ Client:

```bash
sudo apt install -y libpaho-mqttpp3-dev
```

If you need to build from source:

```bash
sudo apt install -y libssl-dev libcppunit-dev
```

### C client
```bash
git clone <https://github.com/eclipse/paho.mqtt.c.git>
cd paho.mqtt.c
cmake -Bbuild -H. -DPAHO_WITH_SSL=TRUE
sudo cmake --build build/ --target install
```

### C++ client
```bash
git clone <https://github.com/eclipse/paho.mqtt.cpp>
cd paho.mqtt.cpp
cmake -Bbuild -H. -DPAHO_MQTT_C_PATH=/usr/local -DPAHO_BUILD_STATIC=TRUE
sudo cmake --build build/ --target install
```

## Qt (via Qt Creator & Maintenance Tool)

This project uses Qt 6.x and CMake. You can install the Qt environment using the official Qt Maintenance Tool.

### Select Qt Version

Recommended: Qt 6.2.x or later
Windows: MSVC 2022 64-bit

### Required Modules

Install the following components in the installer:

- Qt > Qt 6.x > GCC 64-bit
- Qt Widgets
- Qt Multimedia
- Qt MultimediaWidgets
- Qt WebEngine
- Qt Sql
- Qt Charts
- Qt MQTT (under Developer and Designer Tools)

## Post-install Settings

```bash
sudo vi /sbin/dphys-swapfile   # Set CONF_MAXSWAP=2048
sudo vi /etc/dphys-swapfile    # Set CONF_SWAPSIZE=100
sudo reboot
```