# Post-Flight Decoder Guide

## Overview

After the balloon flight, you'll have:

- Compressed `.lz4` files in the `archive/` directory
- A SQLite database `archive/frames.sqlite3` with metadata

The decoder utility allows you to decompress these frames back to their original RAW16 format or convert them to FITS format for analysis.

---

## Quick Start

```bash
# Navigate to data storage directory
cd "data storage"

# Decode all frames to raw binary
./decoder archive/frames.sqlite3 decoded_frames

# Or decode to FITS format
./decoder archive/frames.sqlite3 decoded_frames --fits
```

---

## What the Decoder Does

1. **Opens the SQLite database** and reads frame metadata
2. **For each frame:**
   - Reads the compressed `.lz4` file
   - Parses the 32-byte header with frame information
   - Decompresses using LZ4
   - Saves as `.raw16` or `.fits` format
3. **Outputs** all decoded frames to the specified directory

---

## Output Formats

### RAW16 Binary (`.raw16`)

- Pure binary data: width × height × 2 bytes
- No header, just pixel data
- 16-bit grayscale values
- Total size: 1936 × 1216 × 2 = 4,709,632 bytes per frame

**To read in Python:**

```python
import numpy as np

# Read RAW16 file
width, height = 1936, 1216
data = np.fromfile('frame_123456789.raw16', dtype=np.uint16)
image = data.reshape((height, width))

# Display or process
import matplotlib.pyplot as plt
plt.imshow(image, cmap='gray')
plt.show()
```

### FITS Format (`.fits`)

- Industry-standard astronomy format
- Includes metadata header (2880 bytes)
- Contains width, height, timestamp in header
- Can be opened with astronomy software (DS9, FITS Liberator, etc.)

**To read in Python:**

```python
from astropy.io import fits
import matplotlib.pyplot as plt

# Open FITS file
with fits.open('frame_123456789.fits') as hdul:
    image_data = hdul[0].data
    header = hdul[0].header

print(f"Width: {header['NAXIS1']}")
print(f"Height: {header['NAXIS2']}")
print(f"Timestamp: {header['TIMESTMP']}")

plt.imshow(image_data, cmap='gray')
plt.show()
```

---

## Batch Processing

### Decode Only Specific Frames

Modify `decode_main.cpp` to filter by timestamp or frame number, then recompile.

### Convert to Other Formats

Use the decoded `.raw16` files with ImageMagick, GIMP, or custom scripts:

```bash
# Example: Convert to PNG with ImageMagick (after installing it)
convert -size 1936x1216 -depth 16 gray:frame_123456789.raw16 frame.png
```

### Create Video from Frames

```bash
# Using FFmpeg (install first)
# Convert all raw16 to pngs first, then:
ffmpeg -framerate 2 -i frame_%d.png -c:v libx264 output.mp4
```

---

## Troubleshooting

### "Failed to open database"

- Check the path to `frames.sqlite3`
- Ensure database wasn't corrupted during flight

### "Invalid magic number"

- The `.lz4` file may be corrupted
- Check file size (should be > 32 bytes)
- Verify file wasn't truncated

### "LZ4 decompression failed"

- Compressed data may be corrupted
- Try skipping this frame and continue with others

### "Size mismatch"

- Header says one size, but decompressed to different size
- File corruption or compression error during capture

---

## Database Queries

You can query the database directly to inspect metadata:

```bash
sqlite3 archive/frames.sqlite3
```

**Useful queries:**

```sql
-- Count total frames
SELECT COUNT(*) FROM frames;

-- Show first 10 frames
SELECT frame_id, ts_ns, width, height, raw_bytes, compressed_bytes, filepath
FROM frames
ORDER BY ts_ns
LIMIT 10;

-- Calculate compression ratio
SELECT
    AVG(CAST(raw_bytes AS REAL) / compressed_bytes) as avg_compression_ratio,
    MIN(CAST(raw_bytes AS REAL) / compressed_bytes) as min_ratio,
    MAX(CAST(raw_bytes AS REAL) / compressed_bytes) as max_ratio
FROM frames;

-- Find frames with best compression
SELECT ts_ns, raw_bytes, compressed_bytes,
       CAST(raw_bytes AS REAL) / compressed_bytes as ratio
FROM frames
ORDER BY ratio DESC
LIMIT 10;

-- Check for missing files
SELECT filepath FROM frames WHERE filepath NOT IN (
    SELECT filepath FROM frames
    WHERE EXISTS (SELECT 1)  -- Add actual file check here
);
```

---

## Performance Notes

- Decoding is much faster than encoding (LZ4 advantage)
- Expect ~100-200 MB/s decode speed on modern laptop
- For 1000 frames (~4.7 GB raw): ~30-60 seconds total decode time
- Disk I/O is usually the bottleneck, not CPU

---

## Integration with Analysis Software

### MATLAB

```matlab
% Read RAW16
fid = fopen('frame_123456789.raw16', 'r');
img = fread(fid, [1936, 1216], 'uint16');
fclose(fid);
img = img';  % Transpose to correct orientation
imshow(img, []);
```

### Python (NumPy)

```python
import numpy as np
img = np.fromfile('frame_123456789.raw16', dtype=np.uint16).reshape((1216, 1936))
```

### ImageJ / Fiji

1. File → Import → Raw
2. Image type: 16-bit Unsigned
3. Width: 1936
4. Height: 1216
5. Little-endian byte order

---

## Data Archival

After successful decode and verification:

1. **Keep originals**: Archive the `.lz4` files and database
2. **Backup decoded**: Store decoded frames separately
3. **Verify integrity**: Check random samples visually
4. **Document metadata**: Note any anomalies or corrupted frames
5. **Create thumbnails**: For quick browsing of dataset

---

## Questions?

Contact the MHAB Data Storage Team.

Happy analyzing! 🚀
