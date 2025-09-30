# Compiler
CXX := g++

# Source and Target
SRC := test_visual.cpp fall_detector.cpp crowd_detector.cpp \
       congestion_analyzer.cpp path_finder.cpp renderer.cpp
TARGET := test

# ONNX Runtime
ONNX_INCLUDE := /home/veda/onnxruntime-linux-aarch64-1.17.0/include
ONNX_LIB := /home/veda/onnxruntime-linux-aarch64-1.17.0/lib
ONNX_INCLUDE_FLAGS := -I$(ONNX_INCLUDE)
ONNX_LINK_FLAGS := -L$(ONNX_LIB) -lonnxruntime -Wl,-rpath=$(ONNX_LIB)

# OpenCV
OPENCV_FLAGS := $(shell pkg-config --cflags --libs opencv4)

# Compiler and Linker Flags
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic $(OPENCV_FLAGS) $(ONNX_INCLUDE_FLAGS)
LDFLAGS  := $(ONNX_LINK_FLAGS)

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