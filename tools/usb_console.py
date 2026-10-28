"""Send explicit USB CDC commands. Default command STATUS is read-only."""
import argparse
import time
import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser()
parser.add_argument('command', nargs='*', default=['STATUS'])
parser.add_argument('--port', help='Override automatic RP2350 CDC port discovery')
args = parser.parse_args()
ports = [p.device for p in list_ports.comports() if p.vid == 0x2e8a and p.pid == 9]
if not args.port and len(ports) != 1:
    parser.error('Specify --port; found RP2350 ports: ' + ', '.join(ports))
with serial.Serial(args.port or ports[0], 115200, timeout=.3, write_timeout=2) as device:
    device.reset_input_buffer()
    device.write((' '.join(args.command) + '\n').encode()); device.flush()
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        data = device.read(4096)
        if data:
            print(data.decode(errors='replace').replace('\r', ''), end='')
