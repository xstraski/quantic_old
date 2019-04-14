#!/bin/bash

# ---------------------------------------------------------------------------------------------------------------------------------------------
# Note:
#  This build script is temporarily hardcoded and configured to build a linux-x64 executable with debug code and debug information included.
#  In a future we should rewrite it so it can be executed with parameters like target cpu architecture, internal/public build, etc...
# ---------------------------------------------------------------------------------------------------------------------------------------------

# General project name, must not contain spaces and deprecated symbols, no extension.
# In a nutshell, the target game executable will be named as %OutputName%,
# the target game entities will be named as %OutputName%_ents.so.
OutputName="qtest"

# Common compiler flags for all projects:
# -march=native                 - optimize for native architecture.
# -std=c++17                    - use C++17 standard.
# -g                            - [debug] add debug information for GCC.
# -fPIC                         - Position-Independent-Code.
# -DLINUX=1                     - signals we are compiling for GNU/Linux.
# -DINTERNAL=1					- [debug] signals we are compiling an internal build, not for public release.
# -DSLOWCODE=1					- [debug] signals we are compiling a paranoid build with slow code enabled.
CommonOptions="-march=native -std=c++17 -g -fPIC -DLINUX=1 -DINTERNAL=1 -DSLOWCODE=1"

# Stop at errors
set -e

# Create 'build' directory if not created yet
mkdir -p build

# Compile 'game'.
# Used external libraries:
# 'pthread'                     - POSIX thread library.
# 'm'                           - standard mathematics.
# 'X11'                         - X11 interface.
# 'Xext'                        - X extensions interface.
# 'Xrandr'                      - X RandR interface.
g++ game.cpp -o ./build/$OutputName $CommonOptions -pthread -lm -lX11 -lXext -lXrandr
