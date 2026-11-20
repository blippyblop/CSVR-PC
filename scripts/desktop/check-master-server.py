#!/usr/bin/env python3
"""Exercise the production 1.0.6 HTTP master with local HTTP/UDP fixtures."""
import argparse
import http.server
import json
import os
from pathlib import Path
import socket
import subprocess
import threading

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stage', type=Path, required=True)
parser.add_argument('--game-data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(2)]
for sock in sockets:
    sock.bind(('127.0.0.1', 0))
    sock.settimeout(25)
body = ('# local fixture\n// comment\nunknown 127.0.0.1:9\n'
        f'ip 127.0.0.1:{sockets[0].getsockname()[1]}\n'
        f'gs 127.0.0.1:{sockets[1].getsockname()[1]}\n').encode()
requests = []
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        requests.append(self.path)
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *_):
        pass
http = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=http.serve_forever, daemon=True).start()
stage = args.stage.resolve()
config = stage / 'cstrike/codex-master-fixture.cfg'
config.write_text('clearmasters\n'
                  f'addmasterstatic http://127.0.0.1:{http.server_address[1]}\n'
                  'internetservers\n' + 'wait\n' * 180 + 'quit\n')
env = dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')
command = [str(stage/'csvr-desktop'), '-rodir', str(args.game_data.resolve()),
           '-dev', '3', '-nosound', '-ip', '127.0.0.1', '-noip6', '+exec', config.name]
packets = []
try:
    with (args.output/'master-fixture.log').open('wb') as log:
        child = subprocess.Popen(command, cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            for sock in sockets:
                packets.append(sock.recvfrom(4096)[0])
            child.wait(timeout=25)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
    assert child.returncode == 0, f'Client exited {child.returncode}'
    assert requests == ['/v1/servers/cstrike'], requests
    assert packets[0].startswith(b'\xff\xff\xff\xffinfo 49'), packets[0]
    assert packets[1] == b'\xff\xff\xff\xffTSource Engine Query\x00', packets[1]
    log = (args.output/'master-fixture.log').read_text(errors='replace')
    assert 'yielded 2 server(s)' in log, 'Master response was not parsed'
    result = {'passed': True, 'http_path': requests[0], 'queries': {
        'xash_protocol_49': packets[0].hex(), 'goldsrc_protocol_48': packets[1].hex()},
        'unknown_directive_ignored': True, 'client_exit': child.returncode}
    (args.output/'master-fixture.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
finally:
    http.shutdown()
    for sock in sockets:
        sock.close()
