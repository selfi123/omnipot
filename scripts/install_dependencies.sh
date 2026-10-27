#!/usr/bin/env bash
set -euo pipefail

echo "Installing PotPlayer Native Linux build dependencies on Fedora 44..."
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    qt6-qtbase-devel \
    mpv-devel

echo "Dependencies successfully installed."
