#!/usr/bin/env python3
"""
dashboard_bridge.py — Local Wi-Fi & Web Bridge for PawState
Bridges the micro:bit USB Serial telemetry to a local web server (port 8080)
allowing any smartphone or PC browser on the same Wi-Fi network to view live
dog emotional state, confidence, 5x5 LED mirror, and audio alerts.
"""

import os
import sys
import time
import json
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import serial
import serial.tools.list_ports

HOST = '0.0.0.0'
PORT = 8080
HTML_FILE = 'pawstate_dashboard.html'

# Global connected SSE client queues
client_queues = []
clients_lock = threading.Lock()

def get_local_ip():
    """Detect local IP on Wi-Fi/LAN"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

def broadcast_event(data_dict):
    """Broadcast JSON payload to all connected SSE browser clients"""
    msg = f"data: {json.dumps(data_dict)}\n\n".encode('utf-8')
    with clients_lock:
        to_remove = []
        for q in client_queues:
            try:
                q.write(msg)
                q.flush()
            except Exception:
                to_remove.append(q)
        for dead in to_remove:
            client_queues.remove(dead)

class PawStateHTTPHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Silence standard HTTP access logging to keep terminal clean
        pass

    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            if os.path.exists(HTML_FILE):
                with open(HTML_FILE, 'rb') as f:
                    self.wfile.write(f.read())
            else:
                self.wfile.write(b"<h1>pawstate_dashboard.html not found!</h1>")

        elif self.path == '/events':
            # Server-Sent Events (SSE) streaming endpoint
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            with clients_lock:
                client_queues.append(self.wfile)

            # Keep connection open until client disconnects
            try:
                while True:
                    time.sleep(1)
            except Exception:
                with clients_lock:
                    if self.wfile in client_queues:
                        client_queues.remove(self.wfile)
        else:
            self.send_response(404)
            self.end_headers()

def auto_detect_microbit():
    """Find the micro:bit COM port"""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or '').lower()
        mfg = (p.manufacturer or '').lower()
        if 'mbed' in desc or 'daplink' in desc or 'micro:bit' in desc or 'microbit' in desc or 'nordic' in desc or 'mbed' in mfg:
            return p.device
    if ports:
        return ports[0].device
    return None

def serial_reader_thread(port_name):
    """Read lines from micro:bit and broadcast parsed events"""
    print(f"[Serial] Connecting to micro:bit on {port_name} at 115200 baud...")
    while True:
        ser = None
        try:
            ser = serial.Serial(port_name, 115200, timeout=1)
            print(f"[Serial] Connected to {port_name}!")
            last_probs = [0, 0, 0, 0, 0]

            while True:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if not line:
                    continue

                print(f"  [RAW] {line}")

                # Parse probability array: P:0,251,0,0,0
                if line.startswith('P:'):
                    try:
                        parts = [int(x) for x in line[2:].split(',')]
                        if len(parts) == 5:
                            last_probs = parts
                    except Exception:
                        pass

                # Parse state change: [ML] State changed to: Walking (Confidence: 85%)
                if '[ML] State changed to:' in line:
                    try:
                        parts = line.split('[ML] State changed to:')[1].strip()
                        state_name = parts.split('(')[0].strip()
                        conf = int(parts.split('Confidence:')[1].replace('%', '').replace(')', '').strip())
                        payload = {
                            "type": "state",
                            "state": state_name,
                            "confidence": conf,
                            "probs": last_probs
                        }
                        broadcast_event(payload)
                    except Exception as e:
                        print(f"  [Parse Error] {e}")

                # Parse anxiety alert
                if 'ANXIETY SPIKE' in line:
                    broadcast_event({
                        "type": "alert"
                    })

        except Exception as e:
            if ser is not None:
                ser.close()
            print(f"[Serial] Connection error ({e}). Retrying in 3 seconds...")
            time.sleep(3)

def main():
    port_arg = sys.argv[1] if len(sys.argv) > 1 else None
    port = port_arg or auto_detect_microbit()

    local_ip = get_local_ip()

    print("=" * 70)
    print("🐾 PawState Real-Time Canine Monitor — Wi-Fi & Web Bridge")
    print("=" * 70)
    print(f"  Local PC Dashboard:     http://localhost:{PORT}")
    print(f"  Mobile Phone Dashboard: http://{local_ip}:{PORT}")
    print("=" * 70)
    if port:
        print(f"  Target Serial Port:     {port}")
    else:
        print("  [WARN] No micro:bit port detected yet. Waiting for board to be plugged in...")
        port = "COM5"

    # Start serial reader in background daemon thread
    t = threading.Thread(target=serial_reader_thread, args=(port,), daemon=True)
    t.start()

    # Start HTTP server
    server = ThreadingHTTPServer((HOST, PORT), PawStateHTTPHandler)
    print(f"[Server] Serving dashboard on {local_ip}:{PORT} (Press Ctrl+C to stop)...")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Server] Stopped.")

if __name__ == '__main__':
    main()
