#!/bin/bash
# Quick demo: Capture some frames, then decode them

echo "=== MHAB Data Storage Demo ==="
echo ""

# Step 1: Compile the main system
echo "Step 1: Compiling capture system..."
gcc -c -O2 third_party/lz4/lz4.c -o lz4.o
gcc -c -O2 third_party/sqlite/sqlite3.c -o sqlite3.o

g++ -std=c++17 -O2 -Wall -Wextra \
    -DUSE_SQLITE=1 \
    -o capture_system \
    initialisation.cpp \
    capture.cpp \
    writer.cpp \
    db.cpp \
    lz4.o \
    sqlite3.o \
    -lpthread -ldl

if [ $? -ne 0 ]; then
    echo "✗ Compilation failed!"
    rm -f lz4.o sqlite3.o
    exit 1
fi

echo "✓ Capture system compiled!"
rm -f lz4.o sqlite3.o

# Step 2: Run capture for a few seconds
echo ""
echo "Step 2: Running capture system for 5 seconds..."
echo "(This will capture ~10 frames at 2 FPS)"
echo ""

# Run in background and kill after 5 seconds
timeout 5 ./capture_system &
CAPTURE_PID=$!

# Wait for it to finish
wait $CAPTURE_PID 2>/dev/null

echo ""
echo "✓ Capture complete!"

# Step 3: Show what was captured
echo ""
echo "Step 3: Checking captured files..."
if [ -d "archive" ]; then
    FRAME_COUNT=$(ls -1 archive/*.lz4 2>/dev/null | wc -l)
    echo "✓ Found $FRAME_COUNT compressed frames in archive/"
    ls -lh archive/*.lz4 2>/dev/null | head -5
    
    if [ -f "archive/frames.sqlite3" ]; then
        echo "✓ Database created: archive/frames.sqlite3"
    fi
else
    echo "✗ No archive directory found"
    exit 1
fi

# Step 4: Decode the frames
echo ""
echo "Step 4: Decoding frames..."
./decoder archive/frames.sqlite3 decoded_frames

echo ""
echo "Step 5: Checking decoded files..."
if [ -d "decoded_frames" ]; then
    DECODED_COUNT=$(ls -1 decoded_frames/*.raw16 2>/dev/null | wc -l)
    echo "✓ Decoded $DECODED_COUNT frames to decoded_frames/"
    ls -lh decoded_frames/*.raw16 2>/dev/null | head -5
else
    echo "✗ Decoding failed"
    exit 1
fi

echo ""
echo "=== Demo Complete! ==="
echo ""
echo "Summary:"
echo "  - Captured frames: $FRAME_COUNT"
echo "  - Decoded frames: $DECODED_COUNT"
echo "  - Original size per frame: ~4.7 MB (1936x1216x2 bytes)"
echo "  - Compressed size: varies (see archive/*.lz4)"
echo ""
echo "Try decoding as FITS:"
echo "  ./decoder archive/frames.sqlite3 decoded_fits --fits"
