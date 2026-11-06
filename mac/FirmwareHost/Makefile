# FirmwareHost — compile real MotoNav sources on macOS under HOST_SIM=1
#
# brew install sdl2 cmake
# mkdir build && cd build && cmake .. && cmake --build .
# ./FirmwareHost
#
# Headless (no SDL):
#   cmake -DHOST_HAS_SDL=OFF ..

.PHONY: all configure build run clean

BUILD_DIR ?= build
CMAKE_FLAGS ?=

all: build

configure:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) ..

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/FirmwareHost

clean:
	rm -rf $(BUILD_DIR)
