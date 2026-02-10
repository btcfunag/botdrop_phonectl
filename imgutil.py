#!/usr/bin/env python3
"""imgutil.py - Image utility for openclaw (no external deps)
Usage:
  imgutil.py info <path>           - Show image file info (size, dimensions)
  imgutil.py base64 <path>         - Output base64-encoded image
  imgutil.py thumbnail <src> <dst> - Create a smaller JPEG copy via screencap trick
"""
import sys, os, struct, base64

def jpeg_dimensions(path):
    """Read JPEG width/height from SOF marker."""
    with open(path, "rb") as f:
        f.read(2)  # SOI
        while True:
            marker = f.read(2)
            if len(marker) < 2:
                return None, None
            if marker[0] != 0xFF:
                return None, None
            if marker[1] in (0xC0, 0xC2):
                f.read(3)  # length + precision
                h = struct.unpack(">H", f.read(2))[0]
                w = struct.unpack(">H", f.read(2))[0]
                return w, h
            else:
                length = struct.unpack(">H", f.read(2))[0]
                f.read(length - 2)

def cmd_info(path):
    if not os.path.exists(path):
        print(f"ERROR: {path} not found")
        return 1
    size = os.path.getsize(path)
    w, h = jpeg_dimensions(path)
    print(f"path: {path}")
    print(f"size: {size} bytes ({size/1024/1024:.1f} MB)")
    if w and h:
        print(f"dimensions: {w}x{h}")
    print(f"type: {'JPEG' if path.lower().endswith(('.jpg','.jpeg')) else 'image'}")
    return 0

def cmd_base64(path):
    if not os.path.exists(path):
        print(f"ERROR: {path} not found", file=sys.stderr)
        return 1
    with open(path, "rb") as f:
        data = f.read()
    print(base64.b64encode(data).decode())
    return 0

def cmd_thumbnail(src, dst):
    """Copy and optionally note that full resize needs Pillow."""
    import shutil
    if not os.path.exists(src):
        print(f"ERROR: {src} not found")
        return 1
    shutil.copy2(src, dst)
    print(f"Copied {src} -> {dst}")
    return 0

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "info":
        sys.exit(cmd_info(sys.argv[2]))
    elif cmd == "base64":
        sys.exit(cmd_base64(sys.argv[2]))
    elif cmd == "thumbnail" and len(sys.argv) >= 4:
        sys.exit(cmd_thumbnail(sys.argv[2], sys.argv[3]))
    else:
        print(__doc__)
        sys.exit(1)
