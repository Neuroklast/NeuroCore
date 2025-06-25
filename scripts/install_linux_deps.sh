#!/usr/bin/env bash
set -e

sudo apt-get update
sudo apt-get install -y \
    libx11-dev \
    libxrandr-dev \
    libgl1-mesa-dev \
    libgtk-3-dev \
    libwebkit2gtk-4.1-dev

