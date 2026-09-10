import os
import platform
import queue
import shutil
import socket
import ssl
import struct
import subprocess
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import parse_qs

CERT_FILE = "cert.pem"
KEY_FILE = "key.pem"
MKCERT_VER = "v1.4.4"
MKCERT_BASE = f"https://github.com/FiloSottile/mkcert/releases/download/{MKCERT_VER}"


def get_mkcert_url():
    sys_type = platform.system().lower()
    arch = platform.machine().lower()

    os_info = {
        "windows": {"amd64": "windows-amd64.exe"},
        "linux": {
            "x86_64": "linux-amd64",
            "amd64": "linux-amd64",
            "aarch64": "linux-arm64",
            "arm64": "linux-arm64",
        },
        "darwin": {"arm64": "darwin-arm64", "amd64": "darwin-amd64"},
    }

    try:
        suffix = os_info[sys_type][arch] or os_info[sys_type]["amd64"]
        return f"{MKCERT_BASE}/mkcert-{MKCERT_VER}-{suffix}"
    except KeyError:
        raise RuntimeError(f"Unsupported OS: {sys_type}-{arch}")


def setup_certs():
    if Path(CERT_FILE).exists() and Path(KEY_FILE).exists():
        print("[SSL] Certificates found, skipping setup.")
        return

    print("[SSL] Generating certificates...")
    exe = "mkcert.exe" if platform.system() == "Windows" else "mkcert"
    mkcert_path = Path(exe)

    if not mkcert_path.exists():
        url = get_mkcert_url()
        print(f"[SSL] Downloading mkcert from {url}")
        try:
            urllib.request.urlretrieve(url, mkcert_path)
            if platform.system() != "Windows":
                os.chmod(str(mkcert_path), 0o755)
        except Exception as e:
            print(f"[SSL] Failed to download mkcert: {e}")
            return

    subprocess.run(
        [str(mkcert_path), "-install"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [str(mkcert_path), "*.growtopia1.com", "*.growtopia2.com"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    for p in Path(".").glob("*.pem"):
        if p.name in [CERT_FILE, KEY_FILE]:
            continue
        if "-key" in p.name:
            if Path(KEY_FILE).exists():
                Path(KEY_FILE).unlink()
            shutil.move(str(p), KEY_FILE)
        else:
            if Path(CERT_FILE).exists():
                Path(CERT_FILE).unlink()
            shutil.move(str(p), CERT_FILE)

HEADER_FMT = "<IHBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

PACKET_FLAG_STREAM = 1 << 0
PACKET_FLAG_POD = 1 << 1

PACKET_ID_HELLO = 1
PACKET_ID_HEARTBEAT = 2
PACKET_ID_HTTP_DATA = 3
PACKET_ID_HTTP_TOGGLE = 4

class PacketHeader:
    def __init__(self, size=0, pkt_id=0, flags=PACKET_FLAG_STREAM, reserved=0):
        self.size = size
        self.pkt_id = pkt_id
        self.flags = flags
        self.reserved = reserved

    def pack(self):
        return struct.pack(HEADER_FMT, self.size, self.pkt_id, self.flags, self.reserved)

    @classmethod
    def unpack(cls, data):
        sz, pkt_id, flags, res = struct.unpack(HEADER_FMT, data[:HEADER_SIZE])
        return cls(sz, pkt_id, flags, res)

class PacketWriter:
    def __init__(self, pkt_id, flags=PACKET_FLAG_STREAM):
        self.pkt_id = pkt_id
        self.flags = flags
        self.buf = bytearray()

    def write_int8(self, val):
        self.buf.extend(struct.pack("<b", val))

    def write_uint8(self, val):
        self.buf.extend(struct.pack("<B", val))

    def write_int16(self, val):
        self.buf.extend(struct.pack("<h", val))

    def write_uint16(self, val):
        self.buf.extend(struct.pack("<H", val))

    def write_int32(self, val):
        self.buf.extend(struct.pack("<i", val))

    def write_uint32(self, val):
        self.buf.extend(struct.pack("<I", val))

    def write_int64(self, val):
        self.buf.extend(struct.pack("<q", val))

    def write_uint64(self, val):
        self.buf.extend(struct.pack("<Q", val))

    def write_float(self, val):
        self.buf.extend(struct.pack("<f", val))

    def write_double(self, val):
        self.buf.extend(struct.pack("<d", val))

    def write_string(self, text):
        encoded = text.encode("utf-8")
        self.write_uint16(len(encoded))
        self.buf.extend(encoded)

    def write_raw(self, raw_bytes):
        if raw_bytes:
            self.buf.extend(raw_bytes)

    def finalize(self):
        hdr = PacketHeader(
            size=len(self.buf),
            pkt_id=self.pkt_id,
            flags=self.flags,
            reserved=0,
        )
        return hdr.pack() + bytes(self.buf)

class PacketReader:
    def __init__(self, data):
        self.data = data
        self.offset = 0
        if len(data) >= HEADER_SIZE:
            self.header = PacketHeader.unpack(data[:HEADER_SIZE])
            self.offset = HEADER_SIZE
        else:
            self.header = None

    def _read_fmt(self, fmt):
        sz = struct.calcsize(fmt)
        if self.offset + sz > len(self.data):
            raise IndexError("Buffer overflow")
        val = struct.unpack(fmt, self.data[self.offset : self.offset + sz])
        self.offset += sz
        return val[0]

    def read_int8(self):
        return self._read_fmt("<b")

    def read_uint8(self):
        return self._read_fmt("<B")

    def read_int16(self):
        return self._read_fmt("<h")

    def read_uint16(self):
        return self._read_fmt("<H")

    def read_int32(self):
        return self._read_fmt("<i")

    def read_uint32(self):
        return self._read_fmt("<I")

    def read_int64(self):
        return self._read_fmt("<q")

    def read_uint64(self):
        return self._read_fmt("<Q")

    def read_float(self):
        return self._read_fmt("<f")

    def read_double(self):
        return self._read_fmt("<d")

    def read_string(self):
        length = self.read_uint16()
        if length == 0:
            return ""
        return self.read_raw(length).decode("utf-8", errors="ignore")

    def read_raw(self, sz):
        if sz <= 0:
            return b""
        if self.offset + sz > len(self.data):
            raise IndexError("Buffer overflow")
        raw = self.data[self.offset : self.offset + sz]
        self.offset += sz
        return raw

TCP_HOST = "127.0.0.1"
TCP_PORT = 19500
TIMEOUT = 5.0

def patch_redirect_host(payload, target_ip="127.0.0.1", target_port=19000):
    if not payload:
        return payload

    lines = payload.splitlines()
    out = []

    for line in lines:
        if "|" in line and not line.startswith("#"):
            k, _ = line.split("|", 1)
            k = k.strip()
            if k == "server":
                line = f"server|{target_ip}"
            elif k == "port":
                line = f"port|{target_port}"
        out.append(line)

    return "\n".join(out)

class TCPBroadway:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.running = False
        self.lock = threading.Lock()
        self.waiters = []
        self.waiters_lock = threading.Lock()

    def start(self):
        self.running = True
        threading.Thread(target=self._reconnect_loop, daemon=True).start()

    def _reconnect_loop(self):
        while self.running:
            if not self.sock:
                print(f"[TCP] Connecting to {self.host}:{self.port}...")
                if self._connect():
                    time.sleep(1.3) # not safe tbh
                    self._send_hello()
                    threading.Thread(target=self._heartbeat_loop, daemon=True).start()
                    self._recv_loop()
            time.sleep(5)

    def _connect(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(TIMEOUT)
            s.connect((self.host, self.port))
            s.settimeout(None)
            with self.lock:
                self.sock = s
            print(f"[TCP] Connected!")
            return True
        except Exception as e:
            print(f"[TCP] Connection error: {e}")
            with self.lock:
                self.sock = None
            return False

    def _send_raw(self, data):
        with self.lock:
            if not self.sock:
                return False
            try:
                self.sock.sendall(data)
                return True
            except Exception as e:
                print(f"[TCP] Send failed: {e}")
                return False

    def _send_hello(self):
        w = PacketWriter(PACKET_ID_HELLO)
        if self._send_raw(w.finalize()):
            print("[TCP] Sent hello packet")

    def _heartbeat_loop(self):
        while self.running and self.sock:
            time.sleep(15)
            if not self.sock:
                break
            w = PacketWriter(PACKET_ID_HEARTBEAT)
            if not self._send_raw(w.finalize()):
                break

    def request_server_data(self, protocol, version, platform, user_agent, timeout=5.0):
        if not self.sock:
            print("[TCP] Not connected")
            return ""

        q = queue.Queue()
        with self.waiters_lock:
            self.waiters.append(q)

        w = PacketWriter(PACKET_ID_HTTP_DATA)
        w.write_float(version)
        w.write_int32(platform)
        w.write_int32(protocol)
        w.write_string(user_agent)

        if not self._send_raw(w.finalize()):
            with self.waiters_lock:
                if q in self.waiters:
                    self.waiters.remove(q)
            return ""

        print(f"[TCP] Request sent | Proto: {protocol}, Ver: {version:.2f}, Plat: {platform}")

        try:
            return q.get(timeout=timeout)
        except queue.Empty:
            print("[TCP] Request timeout")
            with self.waiters_lock:
                if q in self.waiters:
                    self.waiters.remove(q)
            return ""

    def _recv_loop(self):
        while self.running and self.sock:
            try:
                hdr_bytes = self._recv_exact(HEADER_SIZE)
                if not hdr_bytes:
                    print("[TCP] Connection lost")
                    break

                hdr_reader = PacketReader(hdr_bytes)
                body_sz = hdr_reader.header.size

                body_bytes = b""
                if body_sz > 0:
                    body_bytes = self._recv_exact(body_sz)
                    if not body_bytes:
                        break

                reader = PacketReader(hdr_bytes + body_bytes)
                self._handle_packet(reader)

            except Exception as e:
                print(f"[TCP] Recv error: {e}")
                break

        self._cleanup()

    def _handle_packet(self, reader):
        hdr = reader.header
        if hdr.pkt_id == PACKET_ID_HELLO:
            http_manager.send_status_ack(http_manager.is_running)
                            
        elif hdr.pkt_id == PACKET_ID_HTTP_DATA:
            try:
                payload = reader.read_string()
            except Exception:
                payload = ""

            with self.waiters_lock:
                if self.waiters:
                    q = self.waiters.pop(0)
                    q.put(payload)

        elif hdr.pkt_id == PACKET_ID_HTTP_TOGGLE:
            enable = reader.read_int32()
            if enable == 1:
                http_manager.start_servers()
            else:
                http_manager.stop_servers()

    def _recv_exact(self, size):
        buf = bytearray()
        while len(buf) < size and self.running:
            try:
                chunk = self.sock.recv(size - len(buf))
                if not chunk:
                    return None
                buf.extend(chunk)
            except Exception:
                return None
        return bytes(buf)

    def _cleanup(self):
        with self.lock:
            if self.sock:
                try:
                    self.sock.close()
                except Exception:
                    pass
                self.sock = None

        with self.waiters_lock:
            for q in self.waiters:
                q.put("")
            self.waiters.clear()

tcp_broadway = TCPBroadway(TCP_HOST, TCP_PORT)

HTTP_PORT = 80
HTTPS_PORT = 443

class HTTPHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == "/growtopia/server_data.php":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            ua = self.headers.get("User-Agent", "")

            body_str = body.decode("utf-8", errors="ignore")
            print(f"\n[HTTP] POST server_data.php ({length} bytes)\n{body_str}")

            parsed = parse_qs(body_str)
            params = {k: v[0] for k, v in parsed.items() if v}

            if not all(k in params for k in ["protocol", "version", "platform"]):
                print(f"[HTTP] Invalid params: {params}")
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Error: Bad Params")
                return

            try:
                proto = int(params["protocol"])
                ver = float(params["version"])
                plat = int(params["platform"])

                resp = tcp_broadway.request_server_data(proto, ver, plat, ua)
                if not resp:
                    print("[HTTP] Empty backend resp")
                    self.send_response(500)
                    self.end_headers()
                    return

                patched = patch_redirect_host(resp, "127.0.0.1", 19000)

                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.end_headers()
                self.wfile.write(patched.encode("utf-8"))

            except ValueError as e:
                print(f"[HTTP] Parse error: {e}")
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Error: Invalid Data Types")
        else:
            self.send_error(404)

    def do_GET(self):
        self.do_POST()

    def log_message(self, fmt, *args):
        return

class HTTPManager:
    def __init__(self):
        self.httpd_80 = None
        self.httpd_443 = None
        self.is_running = False

    def send_status_ack(self, is_active):
        w = PacketWriter(PACKET_ID_HTTP_TOGGLE)
        w.write_int32(1 if is_active else 0)
        tcp_broadway._send_raw(w.finalize())

    def start_servers(self):
        if self.is_running:
            return
        try:
            self.httpd_80 = HTTPServer(("", HTTP_PORT), HTTPHandler)
            
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ctx.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
            self.httpd_443 = HTTPServer(("", HTTPS_PORT), HTTPHandler)
            self.httpd_443.socket = ctx.wrap_socket(self.httpd_443.socket, server_side=True)

            threading.Thread(target=self.httpd_80.serve_forever, daemon=True).start()
            threading.Thread(target=self.httpd_443.serve_forever, daemon=True).start()
            
            self.is_running = True
            print("[HTTP Manager] Ports 80 and 443 are now BOUND and LISTENING.")
            self.send_status_ack(True)
        except Exception as e:
            print(f"[HTTP Manager] Failed to bind ports: {e}")
            self.send_status_ack(False)

    def stop_servers(self):
        if not self.is_running:
            return
        if self.httpd_80:
            self.httpd_80.shutdown()
            self.httpd_80.server_close()
        if self.httpd_443:
            self.httpd_443.shutdown()
            self.httpd_443.server_close()
        
        self.is_running = False
        print("[HTTP Manager] Ports 80 and 443 are now UNBOUND and RELEASED.")
        self.send_status_ack(False)

http_manager = HTTPManager()

if __name__ == "__main__":
    print("GrowTrace HTTP Server")

    setup_certs()
    http_manager.start_servers()
    tcp_broadway.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting...")