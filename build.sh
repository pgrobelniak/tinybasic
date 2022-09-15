#!/bin/bash
make
rm -rf AppDir
mkdir -p AppDir/usr/bin
mv basic AppDir/usr/bin
appimage-builder --recipe AppImageBuilder.yml
