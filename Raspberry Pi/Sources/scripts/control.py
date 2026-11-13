"""Send one local command to the sole Pi I2C owner; never accesses hardware."""
import argparse
import json
import socket
import sys
sys.dont_write_bytecode = True


def request(path, command):
    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as client:
        client.settimeout(8)
        client.connect(str(path))
        client.sendall((command + '\n').encode('ascii'))
        reply = client.recv(4096)
        result = json.loads(reply)
        if 'error' in result and result['error']:
            raise RuntimeError(result['error'])
        return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--socket', required=True)
    p.add_argument('command', nargs='+')
    a = p.parse_args()
    print(json.dumps(request(a.socket, ' '.join(a.command)), indent=2))
