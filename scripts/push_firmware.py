#!/usr/bin/env python3

import struct
import hashlib
import sys
from pathlib import Path

from minio import Minio
from minio.error import S3Error

# ---------------------------
# Config
# ---------------------------
MINIO_ENDPOINT = "localhost:9000"
MINIO_ACCESS_KEY = "minio"
MINIO_SECRET_KEY = "minio123456"
BUCKET = "firmware"

# ---------------------------
# Helpers
# ---------------------------
def sha256_file(path: Path) -> str:
    with open(path,"rb") as f:
        hdr=f.read(32)

    magic, load, hdr_sz, prot_sz, img_sz = struct.unpack_from("<IIHHI", hdr, 0)

    # IMAGE_MAGIC is 0x96f3b83d
    assert magic == 0x96f3b83d, hex(magic)

    hashed_len = hdr_sz + img_sz + prot_sz

    with open(path,"rb") as f:
        data=f.read(hashed_len)

    return hashlib.sha256(data).hexdigest()

# ---------------------------
# Main
# ---------------------------
if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <firmware.bin>")
    sys.exit(1)

file_path = Path(sys.argv[1])
if not file_path.is_file():
    print("Error: file does not exist")
    sys.exit(1)

digest = sha256_file(file_path)
object_name = f"images/{digest}.bin"

client = Minio(
    MINIO_ENDPOINT,
    access_key=MINIO_ACCESS_KEY,
    secret_key=MINIO_SECRET_KEY,
    secure=False,
)

try:
    # Ensure bucket exists
    if not client.bucket_exists(BUCKET):
        client.make_bucket(BUCKET)

    # Upload (idempotent thanks to hash-based name)
    client.fput_object(
        BUCKET,
        object_name,
        str(file_path),
        content_type="application/octet-stream",
    )

    print("Upload successful")
    print(f"SHA-256 : {digest}")
    print(f"Object  : s3://{BUCKET}/{object_name}")

except S3Error as e:
    print("Upload failed:", e)
    sys.exit(1)
