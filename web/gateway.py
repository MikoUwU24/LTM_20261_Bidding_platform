#!/usr/bin/env python3
"""
Web Gateway & Multi-Client Playground for LTM TCP Bidding Platform.
Bridges HTTP/SSE browser tabs to the C TCP Server (127.0.0.1:5500).
Each virtual client tab ("alice", "bob", "carol", ...) maintains its own dedicated TCP socket.
"""
import errno
import json
import os
import queue
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATIC_INDEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "index.html")
TCP_HOST = os.environ.get("TCP_HOST", "127.0.0.1")
TCP_PORT = int(os.environ.get("TCP_PORT", "5500"))
WEB_PORT = int(os.environ.get("WEB_PORT", "8080"))


class TcpClientSession:
    def __init__(self, client_id: str):
        self.client_id = client_id
        self.sock = None
        self.connected = False
        self.lock = threading.Lock()
        self.subscribers = []
        self.history = []  # last 200 events
        self.connect()

    def connect(self):
        with self.lock:
            if self.sock:
                try:
                    self.sock.close()
                except Exception:
                    pass
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.connect((TCP_HOST, TCP_PORT))
                self.sock = s
                self.connected = True
                self._emit("SYS", f"Connected to TCP server {TCP_HOST}:{TCP_PORT}")
                t = threading.Thread(target=self._reader_loop, args=(s,), daemon=True)
                t.start()
            except Exception as e:
                self.sock = None
                self.connected = False
                self._emit("ERR", f"TCP connect failed: {e}")

    def disconnect(self):
        with self.lock:
            if self.sock:
                try:
                    self.sock.close()
                except Exception:
                    pass
                self.sock = None
            self.connected = False
            self._emit("SYS", "Disconnected from TCP server")

    def send_line(self, line: str):
        with self.lock:
            if not self.connected or not self.sock:
                self.connect()
            if not self.sock:
                return False, "Not connected to TCP server"
            try:
                raw = (line.rstrip("\r\n") + "\r\n").encode("utf-8")
                self.sock.sendall(raw)
                self._emit("TX", line.rstrip("\r\n"))
                return True, "Sent"
            except Exception as e:
                self.connected = False
                self._emit("ERR", f"Send error: {e}")
                return False, str(e)

    def send_raw_chunks(self, chunks, delay_ms=180):
        def _worker():
            for idx, ch in enumerate(chunks):
                with self.lock:
                    if not self.connected or not self.sock:
                        self.connect()
                    if not self.sock:
                        return
                    try:
                        data = ch.encode("utf-8")
                        self.sock.sendall(data)
                        disp = ch.replace("\r", "\\r").replace("\n", "\\n")
                        self._emit("TX_RAW", f"chunk[{idx+1}/{len(chunks)}] ({len(data)}B): {disp}")
                    except Exception as e:
                        self.connected = False
                        self._emit("ERR", f"Raw send error: {e}")
                        return
                if idx + 1 < len(chunks) and delay_ms > 0:
                    time.sleep(delay_ms / 1000.0)
        threading.Thread(target=_worker, daemon=True).start()
        return True

    def _reader_loop(self, s: socket.socket):
        buf = b""
        while True:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line_bytes, buf = buf.split(b"\n", 1)
                    if line_bytes.endswith(b"\r"):
                        line_bytes = line_bytes[:-1]
                    text = line_bytes.decode("utf-8", errors="replace")
                    kind = "NOTIFY" if text.startswith("NOTIFY ") else "RX"
                    self._emit(kind, text)
            except Exception:
                break
        with self.lock:
            if self.sock is s:
                self.connected = False
                self.sock = None
                self._emit("SYS", "TCP connection closed by server")

    def _emit(self, kind: str, text: str):
        ev = {
            "ts": time.strftime("%H:%M:%S"),
            "client": self.client_id,
            "kind": kind,
            "text": text,
        }
        self.history.append(ev)
        if len(self.history) > 250:
            self.history = self.history[-250:]
        dead = []
        for q in self.subscribers:
            try:
                q.put_nowait(ev)
            except Exception:
                dead.append(q)
        for q in dead:
            if q in self.subscribers:
                self.subscribers.remove(q)


SESSIONS = {}
SESSIONS_LOCK = threading.Lock()


def get_session(client_id: str) -> TcpClientSession:
    with SESSIONS_LOCK:
        if client_id not in SESSIONS:
            SESSIONS[client_id] = TcpClientSession(client_id)
        return SESSIONS[client_id]


def read_text_file(rel_path: str, tail_lines: int = 80) -> str:
    full = os.path.join(ROOT_DIR, rel_path)
    if not os.path.exists(full):
        return ""
    try:
        with open(full, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
        return "".join(lines[-tail_lines:])
    except Exception as e:
        return f"Error reading {rel_path}: {e}"


class GatewayHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return

    def _send_json(self, code: int, payload: dict):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path in ("/", "/index.html"):
            try:
                with open(STATIC_INDEX, "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            except Exception as e:
                self._send_json(500, {"error": str(e)})
            return

        if parsed.path == "/api/files":
            self._send_json(200, {
                "tcp_target": f"{TCP_HOST}:{TCP_PORT}",
                "accounts": read_text_file("data/accounts.txt", 100),
                "rooms": read_text_file("data/rooms.txt", 100),
                "items": read_text_file("data/items.txt", 100),
                "log": read_text_file("logs/server.log", 120),
            })
            return

        if parsed.path == "/api/events":
            qs = parse_qs(parsed.query)
            cid = qs.get("client", ["alice"])[0]
            sess = get_session(cid)
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()

            q = queue.Queue()
            for ev in list(sess.history):
                q.put(ev)
            sess.subscribers.append(q)
            try:
                while True:
                    try:
                        ev = q.get(timeout=10)
                        msg = f"data: {json.dumps(ev, ensure_ascii=False)}\n\n"
                        self.wfile.write(msg.encode("utf-8"))
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": keepalive\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, Exception):
                pass
            finally:
                if q in sess.subscribers:
                    sess.subscribers.remove(q)
            return

        self._send_json(404, {"error": "Not found"})

    def do_POST(self):
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        try:
            body = json.loads(raw.decode("utf-8"))
        except Exception:
            body = {}

        cid = body.get("client", "alice")
        sess = get_session(cid)

        if parsed.path == "/api/send":
            cmd = body.get("command", "")
            ok, msg = sess.send_line(cmd)
            self._send_json(200 if ok else 502, {"ok": ok, "message": msg, "connected": sess.connected})
            return

        if parsed.path == "/api/send_raw":
            chunks = body.get("chunks", [])
            delay_ms = int(body.get("delay_ms", 180))
            sess.send_raw_chunks(chunks, delay_ms)
            self._send_json(200, {"ok": True})
            return

        if parsed.path == "/api/reconnect":
            sess.connect()
            self._send_json(200, {"ok": sess.connected, "connected": sess.connected})
            return

        if parsed.path == "/api/disconnect":
            sess.disconnect()
            self._send_json(200, {"ok": True, "connected": sess.connected})
            return

        if parsed.path == "/api/clear":
            sess.history.clear()
            self._send_json(200, {"ok": True})
            return

        self._send_json(404, {"error": "Not found"})


def main():
    for name in ("alice", "bob", "carol"):
        get_session(name)
    httpd = ThreadingHTTPServer(("0.0.0.0", WEB_PORT), GatewayHandler)
    print(f"Web Gateway running at http://localhost:{WEB_PORT} -> TCP {TCP_HOST}:{TCP_PORT}")
    sys.stdout.flush()
    httpd.serve_forever()


if __name__ == "__main__":
    main()
