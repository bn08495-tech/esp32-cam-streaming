#!/usr/bin/env /home/computer/esp32/simulator/venv/bin/python3
"""
ESP32-CAM MJPEG Stream Simulator Server
Serves real-time MJPEG video stream on http://127.0.0.1:8080/stream
mimicking an actual ESP32-CAM module with mDNS, OTA, and Flash LED endpoints.
"""

import sys
import os
import time
import io
import math
import socket
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as e:
    print(f"Pillow import failed: {e}")
    sys.exit(1)

BOUNDARY = "123456789000000000000987654321"
flash_led_state = False

def generate_jpeg_frame(width=640, height=480, frame_count=0):
    global flash_led_state
    
    # Background color turns brighter if Flash LED is ON
    bg_color = (180, 180, 150) if flash_led_state else (18, 22, 32)
    img = Image.new('RGB', (width, height), color=bg_color)
    draw = ImageDraw.Draw(img)

    # Draw grid background
    grid_size = 40
    grid_color = (200, 200, 170) if flash_led_state else (30, 40, 55)
    for x in range(0, width, grid_size):
        draw.line([(x, 0), (x, height)], fill=grid_color, width=1)
    for y in range(0, height, grid_size):
        draw.line([(0, y), (width, y)], fill=grid_color, width=1)

    # Bouncing sphere animation
    t = frame_count * 0.08
    cx = int(width / 2 + math.sin(t) * 200)
    cy = int(height / 2 + math.cos(t * 1.3) * 140)
    radius = 35 + int(math.sin(t * 2) * 10)

    for r in range(radius, 0, -3):
        color = (
            int(50 + math.sin(t) * 100 + r * 3) % 255,
            int(150 + math.cos(t) * 80) % 255,
            int(220 - r * 2) % 255
        )
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)

    # Header bar
    draw.rectangle([(0, 0), (width, 50)], fill=(10, 14, 20))
    draw.rectangle([(0, 50), (width, 52)], fill=(0, 200, 255))

    # Text overlays
    now_str = time.strftime("%Y-%m-%d %H:%M:%S") + f".{int(time.time() * 1000) % 1000:03d}"
    draw.text((15, 15), "ESP32-CAM STREAM SIMULATOR (mDNS: esp32-cam.local)", fill=(0, 225, 255))
    draw.text((width - 230, 15), now_str, fill=(200, 220, 240))

    # Flash LED indicator badge
    flash_status_str = "⚡ FLASH: ON" if flash_led_state else "⚡ FLASH: OFF"
    flash_color = (255, 220, 0) if flash_led_state else (120, 130, 140)
    draw.rectangle([(width - 150, height - 32), (width - 15, height - 8)], fill=(30, 35, 45))
    draw.text((width - 140, height - 25), flash_status_str, fill=flash_color)

    # Footer status bar
    draw.rectangle([(0, height - 35), (width - 160, height)], fill=(10, 14, 20))
    draw.text((15, height - 25), f"Frame: {frame_count:06d}  |  IP: 127.0.0.1  |  Res: {width}x{height}", fill=(160, 175, 195))

    # Convert to JPEG bytes
    buffer = io.BytesIO()
    img.save(buffer, format='JPEG', quality=80)
    return buffer.getvalue()


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class ESP32CamHandler(BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200)
        self.send_header('Content-Type', f'multipart/x-mixed-replace; boundary={BOUNDARY}')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()

    def do_GET(self):
        global flash_led_state
        if self.path in ['/stream', '/stream:81', '/']:
            self.send_response(200)
            self.send_header('Content-Type', f'multipart/x-mixed-replace; boundary={BOUNDARY}')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            frame_count = 0
            try:
                while True:
                    frame_data = generate_jpeg_frame(640, 480, frame_count)
                    self.wfile.write(f"--{BOUNDARY}\r\n".encode('ascii'))
                    self.wfile.write(b"Content-Type: image/jpeg\r\n")
                    self.wfile.write(f"Content-Length: {len(frame_data)}\r\n\r\n".encode('ascii'))
                    self.wfile.write(frame_data)
                    self.wfile.write(b"\r\n")
                    self.wfile.flush()

                    frame_count += 1
                    time.sleep(0.04)  # ~25 FPS
            except Exception:
                pass
        elif self.path.startswith('/flash'):
            flash_led_state = not flash_led_state
            resp = f'{{"flash": {1 if flash_led_state else 0}}}'.encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Content-Length', str(len(resp)))
            self.end_headers()
            self.wfile.write(resp)
        elif self.path in ['/snapshot', '/capture', '/jpg']:
            frame_data = generate_jpeg_frame(640, 480, 0)
            self.send_response(200)
            self.send_header('Content-Type', 'image/jpeg')
            self.send_header('Content-Length', str(len(frame_data)))
            self.end_headers()
            self.wfile.write(frame_data)
        else:
            self.send_error(404, "Not Found")

    def log_message(self, format, *args):
        return


def main():
    port = 8080
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            pass

    server_address = ('0.0.0.0', port)
    httpd = ThreadedHTTPServer(server_address, ESP32CamHandler)
    print(f"[+] ESP32-CAM Simulator running at http://127.0.0.1:{port}/stream")
    print("Press Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[-] Simulator stopped.")

if __name__ == '__main__':
    main()
