# Compiler
CXX := g++

# Source and Target
SRC := main_server.cpp fall_detector.cpp crowd_detector.cpp congestion_analyzer.cpp \
       path_finder.cpp renderer.cpp speaker.cpp speaker_socket.cpp \
       playback_worker.cpp audio_settings.cpp
TARGET := main_server

# ONNX Runtime
ONNX_INCLUDE := /home/veda/onnxruntime-linux-aarch64-1.17.0/include
ONNX_LIB := /home/veda/onnxruntime-linux-aarch64-1.17.0/lib
ONNX_INCLUDE_FLAGS := -I$(ONNX_INCLUDE)
ONNX_LINK_FLAGS := -L$(ONNX_LIB) -lonnxruntime -Wl,-rpath=$(ONNX_LIB)

# OpenCV
OPENCV_FLAGS := $(shell pkg-config --cflags --libs opencv4)

# MQTT
MQTT_LIBS := -lpaho-mqttpp3 -lpaho-mqtt3as

# System Libraries
SYS_LIBS := -lpthread -lasound

# Compiler and Linker Flags
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic $(OPENCV_FLAGS) $(ONNX_INCLUDE_FLAGS)
LDFLAGS  := $(MQTT_LIBS) $(ONNX_LINK_FLAGS) $(SYS_LIBS)

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