# Compiler
CXX := g++

# Source and Target
SRC := sub_server.cpp crowd_detector.cpp
TARGET := sub_server

# ONNX Runtime
ONNX_INCLUDE := /home/veda/onnxruntime-linux-aarch64-1.17.0/include
ONNX_LIB := /home/veda/onnxruntime-linux-aarch64-1.17.0/lib
ONNX_INCLUDE_FLAGS := -I$(ONNX_INCLUDE)
ONNX_LINK_FLAGS := -L$(ONNX_LIB) -lonnxruntime -Wl,-rpath=$(ONNX_LIB)

# GStreamer, RTSP, and GLib
RTSP_PKGS := gstreamer-1.0 gstreamer-rtsp-server-1.0 gio-2.0 gobject-2.0 glib-2.0
RTSP_CFLAGS := $(shell pkg-config --cflags $(RTSP_PKGS))
RTSP_LIBS   := $(shell pkg-config --libs $(RTSP_PKGS))

# OpenCV
OPENCV_FLAGS := $(shell pkg-config --cflags --libs opencv4)

# MQTT
MQTT_LIBS := -lpaho-mqttpp3 -lpaho-mqtt3as

# Compiler and Linker Flags
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic $(OPENCV_FLAGS) $(RTSP_CFLAGS) $(ONNX_INCLUDE_FLAGS)
LDFLAGS  := $(RTSP_LIBS) $(MQTT_LIBS) $(ONNX_LINK_FLAGS)

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

# Execution and Debugging
run: $(TARGET)
	LD_LIBRARY_PATH=$(ONNX_LIB) ./$(TARGET)

gdb: $(TARGET)
	LD_LIBRARY_PATH=$(ONNX_LIB) gdb ./$(TARGET)

valgrind: $(TARGET)
	LD_LIBRARY_PATH=$(ONNX_LIB) valgrind --leak-check=full --show-leak-kinds=all \
		--track-origins=yes --log-file=valgrind.log --gen-suppressions=all ./$(TARGET)

# Cleanup
clean:
	rm -f $(TARGET) valgrind.log