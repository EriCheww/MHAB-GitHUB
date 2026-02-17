#!/bin/bash
# Compile the decoder utility for post-flight decoding

echo "=== Compiling Decoder Utility ==="

# Compile C files separately
gcc -c -O2 third_party/lz4/lz4.c -o lz4.o
gcc -c -O2 third_party/sqlite/sqlite3.c -o sqlite3.o

# Compile C++ files and link
g++ -std=c++17 -O2 -Wall -Wextra \
    -o decoder \
    decode_main.cpp \
    decoder.cpp \
    lz4.o \
    sqlite3.o \
    -lpthread -ldl

if [ $? -eq 0 ]; then
    echo "✓ Decoder compiled successfully!"
    echo ""
    echo "Usage:"
    echo "  ./decoder <db_path> <output_dir> [--fits]"
    echo ""
    echo "Example:"
    echo "  ./decoder archive/frames.sqlite3 decoded_frames"
    echo "  ./decoder archive/frames.sqlite3 decoded_frames --fits"
    
    # Clean up object files
    rm -f lz4.o sqlite3.o
else
    echo "✗ Compilation failed!"
    rm -f lz4.o sqlite3.o
    exit 1
fi
