"""Nothing gets a shell without proving it holds the shared secret."""

import hashlib
import hmac
import os
import struct
import subprocess
import time
import unittest

import dterm
from dterm import ATTACH, AUTH, AUTH_FAIL, AUTH_OK, CHALLENGE, HELLO, INPUT


class HandshakeTest(unittest.TestCase):

    def setUp(self):
        self.server = dterm.Server().start()

    def tearDown(self):
        self.server.stop()

    def challenge_for(self, version=dterm.VERSION):
        link = self.server.connect()
        link.send(HELLO, b"DTRM" + bytes([version]))
        return link, link.read_frame()

    def test_challenge_follows_hello(self):
        link, answer = self.challenge_for()
        self.assertIsNotNone(answer)
        self.assertEqual(answer[0], CHALLENGE)
        self.assertEqual(len(answer[1]), 32)
        link.close()

    def test_each_connection_gets_a_new_challenge(self):
        first, one = self.challenge_for()
        second, two = self.challenge_for()
        self.assertNotEqual(one[1], two[1])
        first.close()
        second.close()

    def test_wrong_secret_is_rejected(self):
        link, answer = self.challenge_for()
        link.send(AUTH, hmac.new(b"not-the-secret", answer[1], hashlib.sha256).digest())
        reply = link.read_frame()
        self.assertIsNotNone(reply)
        self.assertEqual(reply[0], AUTH_FAIL)
        link.close()

    def test_replayed_response_is_rejected(self):
        first, answer = self.challenge_for()
        mac = hmac.new(dterm.SECRET.encode(), answer[1], hashlib.sha256).digest()
        first.send(AUTH, mac)
        self.assertEqual(first.read_frame()[0], AUTH_OK)
        first.close()

        # The very same MAC against a fresh challenge must not work.
        second, _ = self.challenge_for()
        second.send(AUTH, mac)
        reply = second.read_frame()
        self.assertIsNotNone(reply)
        self.assertEqual(reply[0], AUTH_FAIL)
        second.close()

    def test_older_protocol_version_is_rejected(self):
        link, answer = self.challenge_for(version=1)
        self.assertIsNone(answer)
        link.close()

    def test_raw_bytes_are_rejected(self):
        link = self.server.connect()
        link.sock.sendall(b"GET / HTTP/1.1\r\n\r\n")
        self.assertIsNone(link.read_frame())
        link.close()

    def test_input_before_auth_is_rejected(self):
        link, _ = self.challenge_for()
        link.send(INPUT, b"echo pwned\n")
        self.assertIsNone(link.read_frame())
        link.close()

    def test_attach_before_auth_is_rejected(self):
        link, _ = self.challenge_for()
        link.send(ATTACH, b"default")
        self.assertIsNone(link.read_frame())
        link.close()

    def test_session_names_cannot_escape_the_directory(self):
        for name in (b"../../etc/passwd", b"a/b", b"has space", b"x" * 40, b"tab\there"):
            link = self.server.authenticate()
            self.assertIsNotNone(link, "authentication itself should succeed")
            link.send(ATTACH, name)
            self.assertIsNone(link.read_frame(), f"{name!r} should have been refused")
            link.close()

    def test_a_silent_client_is_dropped(self):
        link = self.server.connect()
        started = time.time()
        self.assertIsNone(link.read_frame(timeout=10))
        self.assertLess(time.time() - started, 9, "should time out in about 5s")
        link.close()

    def test_failed_attempts_never_cost_a_shell(self):
        for _ in range(3):
            link, answer = self.challenge_for()
            link.send(AUTH, hmac.new(b"wrong", answer[1], hashlib.sha256).digest())
            link.read_frame()
            link.close()

        time.sleep(1.0)
        self.assertEqual(self.server.shells(), [])
        self.assertEqual(self.server.sessions(), [])


class SecretFileTest(unittest.TestCase):
    """The server refuses to run without a properly protected secret."""

    def run_server_with(self, home, mode=None, contents=None):
        os.makedirs(os.path.join(home, ".dterm"), exist_ok=True)
        path = os.path.join(home, ".dterm", "secret")

        if contents is not None:
            with open(path, "w") as handle:
                handle.write(contents)
            os.chmod(path, mode)

        environment = dict(os.environ)
        environment["HOME"] = home
        environment.pop("DTERM_SECRET", None)

        return subprocess.run([dterm.SERVER, str(dterm.free_port())],
                              env=environment, capture_output=True,
                              text=True, timeout=10)

    def test_no_secret_refuses_to_start(self):
        import tempfile
        with tempfile.TemporaryDirectory() as home:
            result = self.run_server_with(home)
            self.assertIn("no shared secret found", result.stdout + result.stderr)

    def test_world_readable_secret_refuses_to_start(self):
        import tempfile
        with tempfile.TemporaryDirectory() as home:
            result = self.run_server_with(home, mode=0o644, contents="hunter2\n")
            self.assertIn("readable by other users", result.stdout + result.stderr)

    def test_empty_secret_refuses_to_start(self):
        import tempfile
        with tempfile.TemporaryDirectory() as home:
            result = self.run_server_with(home, mode=0o600, contents="   \n")
            self.assertIn("empty", result.stdout + result.stderr)
