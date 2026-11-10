"""Validate rendered images with Blender before the native launcher encodes them."""
import bpy
import json
import sys
import struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from scene_io import file_digest

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 5:
    raise ValueError("Expected FRAMES NEW_REPORT FIRST LAST FPS")
directory, report = Path(args[0]).resolve(), Path(args[1]).resolve()
first, last, fps = map(int, args[2:])
if not 1 <= first <= last <= 999999 or last-first >= 240 or not 1 <= fps <= 120:
    raise ValueError("Film frame/rate budget exceeded")
if not directory.is_dir() or report.exists():
    raise ValueError("Frame input or report destination is invalid")
identity, records = None, []
for frame in range(first, last + 1):
    stem = f"frame_{frame:06d}"
    metadata, png, exr = (directory / (stem + suffix) for suffix in (".json", ".png", ".exr"))
    for file, limit in ((metadata, 65536), (png, 64*1024*1024), (exr, 1024*1024*1024)):
        if not file.is_file() or not 0 < file.stat().st_size <= limit:
            raise ValueError(f"Missing or oversized frame artifact: {file.name}")
    data = json.loads(metadata.read_text())
    if data.get("status") != "RENDERED" or data.get("frame") != frame or data.get("engine") != "CYCLES":
        raise ValueError("Frame metadata is not a completed Cycles render")
    current = {key: data[key] for key in ("source_sha256", "camera", "width", "height", "device_requested")}
    if identity is None:
        identity = current
    if len(data["source_sha256"]) != 64 or any(c not in "0123456789abcdef" for c in data["source_sha256"]):
        raise ValueError("Frame source digest is malformed")
    if current != identity or data.get("png") != png.name or data.get("aovs") != exr.name:
        raise ValueError("Frame identity or artifact names differ within sequence")
    if not 1 <= data["width"] <= 8192 or not 1 <= data["height"] <= 8192:
        raise ValueError("Frame dimensions exceed budget")
    with png.open("rb") as stream:
        header = stream.read(24)
        if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
            raise ValueError("Frame image is not PNG")
        if struct.unpack(">II", header[16:24]) != (data["width"], data["height"]):
            raise ValueError("PNG header dimensions differ from bounded metadata")
    with exr.open("rb") as stream:
        if stream.read(4) != b"\x76\x2f\x31\x01":
            raise ValueError("Frame pass artifact is not EXR")
    image = bpy.data.images.load(str(png), check_existing=False)
    try:
        if tuple(image.size) != (data["width"], data["height"]) or not image.has_data:
            raise ValueError("Blender could not decode expected frame dimensions")
    finally:
        bpy.data.images.remove(image)
    records.append({"frame": frame, "png_sha256": file_digest(png),
                    "exr_sha256": file_digest(exr), "metadata_sha256": file_digest(metadata)})
result = {"status": "IMAGE_SEQUENCE_VERIFIED_NOT_YET_ENCODED", "identity": identity,
          "fps": fps, "first_frame": first, "last_frame": last, "frames": records}
with report.open("x") as stream:
    json.dump(result, stream, indent=2)
print("IMAGE_SEQUENCE_VERIFIED", len(records), flush=True)
