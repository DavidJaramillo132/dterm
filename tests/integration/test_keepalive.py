"""A client that stops answering is detached, but its shell is left alone."""

import time
import unittest

import dterm
from dterm import PING, PONG


class KeepaliveTest(unittest.TestCase):

    PING_SECONDS = 2

    def setUp(self):
        self.server = dterm.Server(ping_seconds=self.PING_SECONDS).start()

    def tearDown(self):
        self.server.stop()

    def test_a_quiet_client_gets_pinged(self):
        link = self.server.attach("work")

        answer = link.read_frame(timeout=self.PING_SECONDS * 3)
        self.assertIsNotNone(answer, "a PING should have arrived")
        self.assertEqual(answer[0], PING)
        link.close()

    def test_answering_pings_keeps_the_client_attached(self):
        link = self.server.attach("work")
        answered = 0
        deadline = time.time() + self.PING_SECONDS * 5

        while time.time() < deadline:
            answer = link.read_frame(timeout=self.PING_SECONDS * 3)
            self.assertIsNotNone(answer, "the connection should have stayed open")

            if answer[0] == PING:
                link.send(PONG)
                answered += 1

        self.assertGreaterEqual(answered, 2)
        self.assertIn(b"STILL_ATTACHED", link.run("echo STILL_ATTACHED\n"))
        link.close()

    def test_ignoring_pings_detaches_but_does_not_kill_the_shell(self):
        link = self.server.attach("work")
        link.run("export MARK=untouched\n")

        pings = 0
        detached = False
        deadline = time.time() + self.PING_SECONDS * 8

        while time.time() < deadline:
            answer = link.read_frame(timeout=self.PING_SECONDS * 3)

            if answer is None:
                detached = True
                break

            if answer[0] == PING:
                pings += 1   # deliberately never answered

        self.assertTrue(detached, "the server should have given up on us")
        self.assertEqual(pings, 2, "it should ping twice before dropping the client")
        link.close()

        self.assertEqual(self.server.sessions(), ["work"],
                         "the session must survive a dropped client")

        again = self.server.attach("work")
        self.assertIn(b"MARK=untouched", again.run("echo MARK=$MARK\n"))
        again.close()
