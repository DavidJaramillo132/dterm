"""Test harness for the DTerm server.

Every test runs a private server: its own port, its own HOME, its own shared
secret. Nothing here can touch a real ~/.dterm or a real running dterm.

Processes are identified by reading HOME out of /proc/<pid>/environ rather than
by matching command lines. Pattern matching on command lines is unreliable in
both directions: `pgrep -f` also matches the test runner's own command line,
and `pgrep` without -f compares only the process name, so "bash --login" never
matches anything and the check silently passes forever.
"""

import hashlib
import hmac
import os
import shutil
import signal
import socket
import struct
import subprocess
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SERVER = os.environ.get("DTERM_SERVER", os.path.join(REPO, "build", "dterm"))
CLIENT = os.environ.get("DTERM_CLIENT", os.path.join(REPO, "build", "dterm-client"))

VERSION = 2
SECRET = "3f8a" * 16

HELLO, INPUT, OUTPUT, RESIZE, PING, PONG, CHALLENGE, AUTH, AUTH_OK, AUTH_FAIL, ATTACH = range(11)

NAMES = {
    HELLO: "HELLO", INPUT: "INPUT", OUTPUT: "OUTPUT", RESIZE: "RESIZE",
    PING: "PING", PONG: "PONG", CHALLENGE: "CHALLENGE", AUTH: "AUTH",
    AUTH_OK: "AUTH_OK", AUTH_FAIL: "AUTH_FAIL", ATTACH: "ATTACH",
}


def frame(kind, payload=b""):
    return bytes([kind]) + struct.pack(">I", len(payload)) + payload


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def processes_under(home):
    """Every live process whose HOME is `home`, as (pid, cmdline) pairs."""
    found = []

    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue

        try:
            with open(f"/proc/{entry}/environ", "rb") as handle:
                environ = handle.read()
            with open(f"/proc/{entry}/cmdline", "rb") as handle:
                cmdline = handle.read()
        except OSError:
            continue   # the process died while we were looking at it

        if b"HOME=" + home.encode() + b"\0" in environ:
            found.append((int(entry), cmdline.replace(b"\0", b" ").strip().decode()))

    return found


class Connection:
    """One client connection, speaking the wire protocol by hand."""

    def __init__(self, sock):
        self.sock = sock

    def send(self, kind, payload=b""):
        self.sock.sendall(frame(kind, payload))

    def read_frame(self, timeout=5.0):
        """One whole frame, or None if the server hung up or went quiet."""
        self.sock.settimeout(timeout)
        header = b""

        try:
            while len(header) < 5:
                chunk = self.sock.recv(5 - len(header))
                if not chunk:
                    return None
                header += chunk

            length = struct.unpack(">I", header[1:5])[0]
            payload = b""

            while len(payload) < length:
                chunk = self.sock.recv(length - len(payload))
                if not chunk:
                    return None
                payload += chunk
        except socket.timeout:
            return None

        return (header[0], payload)

    def drain_output(self, seconds=1.0):
        """Everything the shell printed within `seconds`, frames unwrapped."""
        self.sock.settimeout(seconds)
        raw = b""

        try:
            while True:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                raw += chunk
        except socket.timeout:
            pass

        text = b""
        offset = 0

        while offset + 5 <= len(raw):
            kind = raw[offset]
            length = struct.unpack(">I", raw[offset + 1:offset + 5])[0]

            if kind == OUTPUT:
                text += raw[offset + 5:offset + 5 + length]

            offset += 5 + length

        return text

    def run(self, command, settle=0.9):
        """Types a command and returns what came back."""
        self.send(INPUT, command.encode() if isinstance(command, str) else command)
        time.sleep(settle)
        return self.drain_output(0.7)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


class Server:
    """A dterm server on a private port, with a private HOME."""

    def __init__(self, ping_seconds=None):
        self.home = tempfile.mkdtemp(prefix="dterm-test-")
        self.port = free_port()
        self.process = None
        self.ping_seconds = ping_seconds

    @property
    def session_dir(self):
        return os.path.join(self.home, ".dterm", "sessions")

    def sessions(self):
        try:
            return sorted(n[:-5] for n in os.listdir(self.session_dir)
                          if n.endswith(".sock"))
        except OSError:
            return []

    def shells(self):
        return [pid for pid, cmd in processes_under(self.home) if cmd == "bash --login"]

    def server_processes(self):
        """The listener plus one process per live connection.

        Matches on the first word only: the listener carries a port argument,
        so comparing the whole command line silently matches nothing.
        """
        return [pid for pid, cmd in processes_under(self.home)
                if cmd.split(" ")[0] == SERVER]

    def start(self):
        environment = dict(os.environ)
        environment["HOME"] = self.home
        environment["DTERM_SECRET"] = SECRET

        if self.ping_seconds is not None:
            environment["DTERM_PING_SECONDS"] = str(self.ping_seconds)

        self.process = subprocess.Popen(
            [SERVER, str(self.port)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

        for _ in range(100):
            time.sleep(0.05)
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return self
            except OSError:
                if self.process.poll() is not None:
                    raise RuntimeError("server exited: " + self.process.stdout.read())

        raise RuntimeError("server never started listening")

    def kill(self):
        """Kills the listener only. Sessions are meant to survive this."""
        if self.process and self.process.poll() is None:
            self.process.kill()
            self.process.wait()

    def restart(self):
        self.kill()
        environment = dict(os.environ)
        environment["HOME"] = self.home
        environment["DTERM_SECRET"] = SECRET

        if self.ping_seconds is not None:
            environment["DTERM_PING_SECONDS"] = str(self.ping_seconds)

        self.process = subprocess.Popen(
            [SERVER, str(self.port)], env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        time.sleep(0.6)
        return self

    def stop(self):
        """Tears down everything this server ever started, sessions included."""
        self.kill()

        for pid, _ in processes_under(self.home):
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass

        time.sleep(0.3)
        shutil.rmtree(self.home, ignore_errors=True)

    # -- connecting -------------------------------------------------------

    def connect(self):
        return Connection(socket.create_connection(("127.0.0.1", self.port), timeout=5))

    def authenticate(self, secret=SECRET, version=VERSION):
        """HELLO -> CHALLENGE -> AUTH. Returns the connection, or None."""
        link = self.connect()
        link.send(HELLO, b"DTRM" + bytes([version]))

        answer = link.read_frame()
        if answer is None or answer[0] != CHALLENGE:
            link.close()
            return None

        mac = hmac.new(secret.encode(), answer[1], hashlib.sha256).digest()
        link.send(AUTH, mac)

        answer = link.read_frame()
        if answer is None or answer[0] != AUTH_OK:
            link.close()
            return None

        return link

    def attach(self, name="default", rows=24, cols=80, settle=1.0):
        """A full connection, attached to a session and ready to type into."""
        link = self.authenticate()
        if link is None:
            return None

        link.send(ATTACH, name.encode())
        link.send(RESIZE, struct.pack(">HH", rows, cols))
        time.sleep(settle)
        link.drain_output(0.5)
        return link

    def __enter__(self):
        return self.start()

    def __exit__(self, *_):
        self.stop()
        return False
