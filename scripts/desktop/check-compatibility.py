#!/usr/bin/env python3
"""Loopback client/server signon checks using separately staged PC versions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--server', type=Path, required=True)
parser.add_argument('--client', type=Path, required=True)
parser.add_argument('--game-data', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--wine-prefix', type=Path)
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)

def stage_copy(source, name):
    destination = output/name
    destination.mkdir(exist_ok=True)
    entries = [line.split('  ', 1)[1] for line in (source/'SHA256SUMS').read_text().splitlines()]
    for entry in entries:
        target = destination/entry
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source/entry, target)
    return destination

def port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

def command(stage, parameters):
    windows = (stage/'xash3d.exe').exists()
    game = args.game_data.resolve()
    prefix = ['wine', str(stage/'xash3d.exe')] if windows else [str(stage/'xash3d')]
    rodir = 'Z:'+str(game).replace('/', '\\') if windows else str(game)
    return prefix + ['-game', 'cstrike', '-novr', '-ref', 'gl', '-rodir', rodir,
                     '-dev', '3', '-log', 'engine.log', '-nosound', '-ip', '127.0.0.1', '-noip6'] + parameters

def environment(stage):
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1', WINEDEBUG='-all')
    env['LD_LIBRARY_PATH'] = str(stage)+os.pathsep+env.get('LD_LIBRARY_PATH', '')
    if args.wine_prefix:
        env['WINEPREFIX'] = str(args.wine_prefix.resolve())
    return env

server = stage_copy(args.server.resolve(), 'server')
client = stage_copy(args.client.resolve(), 'client')
server_port, client_port = port(), port()
config = client/'cstrike/codex-compat-client.cfg'
config.write_text('name compat-test\n' + f'connect 127.0.0.1:{server_port}\n' +
                  'wait\n'*180 + 'jointeam 2\n' + 'wait\n'*60 + 'joinclass 1\n' +
                  'wait\n'*240 + 'status\n' + 'wait\n'*30 + 'quit\n')
server_file = (output/'server.log').open('wb')
client_file = (output/'client.log').open('wb')
server_process = client_process = None
try:
    server_process = subprocess.Popen(command(server, ['-dedicated', '-console', '-port', str(server_port),
        '+sv_lan', '1', '+log', 'on', '+maxplayers', '8', '+map', 'de_dust2']),
        cwd=server, env=environment(server), stdin=subprocess.PIPE, stdout=server_file, stderr=subprocess.STDOUT)
    deadline = time.monotonic()+25
    ready = False
    while time.monotonic()<deadline:
        if server_process.poll() is not None:
            raise RuntimeError('Server failed to start; see server.log')
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as query:
            query.settimeout(.25)
            query.sendto(b'\xff\xff\xff\xffinfo 49', ('127.0.0.1', server_port))
            try:
                reply = query.recvfrom(8192)[0]
                if reply.startswith(b'\xff\xff\xff\xff'):
                    ready = True
                    break
            except socket.timeout:
                pass
        time.sleep(.05)
    assert ready, 'Server never responded to a protocol-49 status query'
    client_process = subprocess.Popen(command(client, ['-windowed', '-port', str(client_port), '+exec', config.name]),
        cwd=client, env=environment(client), stdout=client_file, stderr=subprocess.STDOUT)
    client_process.wait(timeout=45)
    assert client_process.returncode == 0, f'Client exited {client_process.returncode}'
finally:
    for child in (client_process, server_process):
        if child is None or child.poll() is not None:
            continue
        if child.stdin:
            try:
                child.stdin.write(b'quit\n')
                child.stdin.flush()
                child.wait(timeout=5)
                continue
            except (OSError, subprocess.TimeoutExpired):
                pass
        child.terminate()
        try:
            child.wait(timeout=10)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
    server_file.close()
    client_file.close()

def read_logs(directory, fallback):
    files = list(directory.glob('**/engine.log')) + list(directory.glob('**/L*.log'))
    return fallback.read_text(errors='replace')+'\n'+'\n'.join(f.read_text(errors='replace') for f in files)
server_log = read_logs(server, output/'server.log')
client_log = read_logs(client, output/'client.log')
assert 'compat-test' in server_log, 'Server did not register the client'
assert 'entered the game' in server_log, 'Client did not complete server signon'
assert re.search(r'(joined team|joined.*CT|Counter-Terrorist)', server_log, re.I), 'Client did not join a team'
assert 'de_dust2' in client_log, 'Client did not receive the map'
assert not re.search(r'(protocol mismatch|Host_Error:|bad server message|svc_bad)', client_log, re.I), 'Protocol error in client log'
result = {'passed': True, 'signon': True, 'team_join': True, 'map': 'de_dust2',
          'client_windows': (client/'xash3d.exe').exists(), 'server_windows': (server/'xash3d.exe').exists(),
          'scope': 'Loopback LAN signon and team join; no Internet authentication/anti-cheat certification'}
for role, directory in [('client', client), ('server', server)]:
    binary = directory/('xash.dll' if (directory/'xash.dll').exists() else 'libxash.so')
    result[role+'_engine_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
(output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
