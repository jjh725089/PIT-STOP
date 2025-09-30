# Compiler
CXX := g++

# Source and Target
SRC := speaker_main.cpp speaker.cpp speaker_socket.cpp playback_worker.cpp audio_settings.cpp
TARGET := speaker_app

# System Libraries
SYS_LIBS := -lasound -lpthread

# Compiler and Linker Flags
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
LDFLAGS  := $(SYS_LIBS)

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

# Execution and Debugging
run: $(TARGET)
	./$(TARGET)

gdb: $(TARGET)
	gdb ./$(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all \
		--track-origins=yes --log-file=valgrind.log --gen-suppressions=all ./$(TARGET)

# Cleanup
clean:
	rm -f $(TARGET) valgrind.log