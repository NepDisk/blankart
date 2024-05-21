#!/usr/bin/env bash

# Makes a fused macOS Universal app bundle in the arm64 release preset dir
# Only works if in master branch or in source tarball

set -e

rm -rf "build/ninja-x64_osx_vcpkg-release"
rm -rf "build/ninja-arm64_osx_vcpkg-release"
cmake --preset ninja-x64_osx_vcpkg-release -DVCPKG_OVERLAY_TRIPLETS=scripts/ -DVCPKG_TARGET_TRIPLET=x64-osx-1015
cmake --build --preset ninja-x64_osx_vcpkg-release
cmake --preset ninja-arm64_osx_vcpkg-release -DVCPKG_OVERLAY_TRIPLETS=scripts/ -DVCPKG_TARGET_TRIPLET=arm64-osx-1015
cmake --build --preset ninja-arm64_osx_vcpkg-release

mkdir -p build/dist
rm -rf "build/dist/Dr. Robotnik's Ring Racers.app" "build/dist/srb3kart.app"

cp -r build/ninja-arm64_osx_vcpkg-release/bin/srb3kart.app build/dist/

lipo -create \
	-output "build/dist/srb3kart.app/Contents/MacOS/srb3kart" \
	build/ninja-x64_osx_vcpkg-release/bin/srb3kart.app/Contents/MacOS/srb3kart \
	build/ninja-arm64_osx_vcpkg-release/bin/srb3kart.app/Contents/MacOS/srb3kart

mv build/dist/srb3kart.app "build/dist/SRB3Kart.app"
