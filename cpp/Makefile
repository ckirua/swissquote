.PHONY: configure build test install check-toolchain

BUILD_DIR := build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build $(BUILD_DIR) -j

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: build
	cmake --install $(BUILD_DIR)

check-toolchain:
	bash scripts/check_toolchain.sh
