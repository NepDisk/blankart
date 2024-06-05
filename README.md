# blankart

This is a project which has the main idea to recreate v1 of a certain kart game which was well beloved but was changed despite that.

# Development

## Building from Source

This is built using a compatible C++ toolchain (GCC, MinGW, Clang and Apple Clang as of this writing), CMake, and Microsoft vcpkg. The compiler and runtime libraries must support the ISO C++17 standard and ISO C11 standard.

On Linux platforms, you will need the following libraries available on the system.

- libcurl
- zlib
- libpng
- libogg
- libvorbis
- libvpx
- libyuv
- SDL2

On Windows and macOS, you will need to install [vcpkg] instead to build these dependencies alongside the game.

[vcpkg]: https://vcpkg.io/en/

To configure and build the game, there are [CMake presets] (declared in `CMakePresets.json`). These presets require the ninja build script tool in addition to cmake and your C++ toolchain. Here is a non-exhaustive list of them:

- ninja-debug: non-optimized, assertions enabled
- ninja-develop: optimized, assertions enabled
- ninja-release: optimized
- ninja-x86_mingw_static_vcpkg-debug
- ninja-x86_mingw_static_vcpkg-develop
- ninja-x86_mingw_static_vcpkg-release
- ninja-x64_osx_vcpkg-debug
- ninja-x64_osx_vcpkg-develop
- ninja-x64_osx_vcpkg-release
- ninja-arm64_osx_vcpkg-debug
- ninja-arm64_osx_vcpkg-develop
- ninja-arm64_osx_vcpkg-release

[CMake presets]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

These presets depend on the `VCPKG_ROOT` environment variable being specified before the first run of the `cmake` command. Their build directories are pre-configured as subdirectories of `build/`.

After all prerequisites are set-up, configure and build using the following commands, adjusting according to your target system:

    cmake --preset ninja-x86_mingw_static_vcpkg-develop
    cmake --build --preset ninja-x86_mingw_static_vcpkg-develop
