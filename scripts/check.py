import struct, hashlib
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <path>")
    sys.exit(1)
p = sys.argv[1]

with open(p,"rb") as f:
    hdr=f.read(32)

magic, load, hdr_sz, prot_sz, img_sz = struct.unpack_from("<IIHHI", hdr, 0)

# IMAGE_MAGIC is 0x96f3b83d
assert magic == 0x96f3b83d, hex(magic)

hashed_len = hdr_sz + img_sz + prot_sz

with open(p,"rb") as f:
    data=f.read(hashed_len)

print(hashlib.sha256(data).hexdigest())
