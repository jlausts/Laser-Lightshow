import socket
import threading
import time
import json
from laser import Laser  # Make sure this is your DLL wrapper class

HOST = '127.0.0.1'
PORT = 65432

params = {
    'x': [],
    'y': [],
    'rgb': [128, 128, 128]
}
params_lock = threading.Lock()
laser = Laser()

def handle_client(conn):
    global params
    with conn:
        data = conn.recv(4096)
        if not data:
            return
        try:
            decoded = json.loads(data.decode('utf-8'))
            with params_lock:
                params['x'] = decoded.get('x', [])
                params['y'] = decoded.get('y', [])
                params['rgb'] = decoded.get('rgb', [128, 128, 128])
        except Exception as e:
            print(f"Error parsing socket data: {e}")

def socket_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Laser socket server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            threading.Thread(target=handle_client, args=(conn,), daemon=True).start()

def laser_loop():
    while True:
        with params_lock:
            payload = []
            for freq, amp in params['x']:
                payload.append(['X', freq, amp])
            for freq, amp in params['y']:
                payload.append(['Y', freq, amp])
            r, g, b = params['rgb']
            payload += [['R', r], ['G', g], ['B', b]]
        laser.send(payload)

threading.Thread(target=socket_server, daemon=True).start()
laser_loop()
