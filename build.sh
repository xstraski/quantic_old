#!/bin/bash

# ---------------------------------------------------------------------------------------------------------------------------------------------
# Note:
#  This build script is temporarily hardcoded and configured to build a linux-x64 executable with debug code and debug information included.
#  In a future we should rewrite it so it can be executed with parameters like target cpu architecture, internal/public build, etc...
# ---------------------------------------------------------------------------------------------------------------------------------------------

mkdir -p build
g++ game.cpp -o ./build/qtest -march=native -DLINUX=1 -DPARANOID=1 -DSLOWCODE=1 -lm -lX11 -lXext -lXrandr
