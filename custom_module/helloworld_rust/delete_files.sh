#!/bin/bash

# Delete all files except *.rs, Makefile, and this script
find . -maxdepth 1 -type f ! -name "*.rs" ! -name "Makefile" ! -name "$(basename "$0")" -exec rm -f {} +

echo "Cleaned up files, keeping only .rs files, the Makefile, and this script."
