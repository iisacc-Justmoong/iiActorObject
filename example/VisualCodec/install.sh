#!/bin/sh
set -eu
if [ "$#" -ne 2 ]; then echo "usage: $0 <build-directory> <install-prefix>" >&2; exit 2; fi
exec cmake --install "$1" --prefix "$2"
