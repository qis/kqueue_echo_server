MAKEFLAGS += --no-print-directory

CC	:= clang
CXX	:= clang++
CMAKE	:= CC=$(CC) CXX=$(CXX) cmake -GNinja
PROJECT	!= grep "^project" CMakeLists.txt | cut -c9- | cut -d " " -f1 | tr "[:upper:]" "[:lower:]"
SOURCES	!= find src -type f -name '*.h' -or -name '*.cpp'

all: debug

run: debug
	@sudo build/debug/$(PROJECT)

dbg: debug
	@lldb -o run build/debug/$(PROJECT)

format:
	@clang-format -i $(SOURCES)

install: release
	@cmake --build build/release --target install

debug: build/debug/CMakeCache.txt $(SOURCES)
	@cmake --build build/debug

release: build/release/CMakeCache.txt $(SOURCES)
	@cmake --build build/release

build/debug/CMakeCache.txt: CMakeLists.txt build/debug
	@cd build/debug && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX:PATH=$(PWD) $(PWD)

build/release/CMakeCache.txt: CMakeLists.txt build/release
	@cd build/release && $(CMAKE) -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX:PATH=$(PWD) $(PWD)

build/debug:
	@mkdir -p build/debug

build/release:
	@mkdir -p build/release

clean:
	@rm -rf build/debug build/release

.PHONY: all run dbg format install debug release clean
