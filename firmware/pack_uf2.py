"""Pack an RP2350 ARM Secure UF2 from the Pico SDK binary output."""
from pathlib import Path
import argparse
import hashlib
import struct

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, default=Path(__file__).resolve().parent / 'build-rp2350')
args = parser.parse_args()
data = (args.build_dir / 'iigs_combined.bin').read_bytes()
blocks = (len(data) + 255) // 256
output = bytearray()
for index in range(blocks):
    payload = data[index*256:(index+1)*256].ljust(256, b'\0')
    header = struct.pack('<8I', 0x0A324655, 0x9E5D5157, 0x2000,
                         0x10000000 + index*256, 256, index, blocks, 0xE48BFF59)
    output += header + payload + bytes(220) + struct.pack('<I', 0x0AB16F30)
destination = args.build_dir / 'iigs_combined.uf2'
destination.write_bytes(output)
print(destination.name, len(output), 'bytes SHA256', hashlib.sha256(output).hexdigest())
