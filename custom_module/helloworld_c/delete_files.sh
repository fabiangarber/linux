#!/bin/bash

# Delete all files except *.c (exactly .c), Makefile, and this script
find . -maxdepth 1 -type f ! -name "*.c" ! -name "Makefile" ! -name "$(basename "$0")" -exec rm -f {} +

# Delete files with .c followed by additional characters
find . -maxdepth 1 -type f -name "*.c*" ! -name "*.c" -exec rm -f {} +

echo "Cleaned up files, keeping only exact .c files, the Makefile, and this script."
