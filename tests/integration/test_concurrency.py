"""Many clients at once, each in its own process and its own session."""

import time
import unittest

import dterm


class ConcurrencyTest(unittest.TestCase):

    def setUp(self):
        self.server = dterm.Server().start()
        self.links = []

    def tearDown(self):
        for link in self.links:
            link.close()
        self.server.stop()

    def test_several_sessions_are_isolated(self):
        count = 5

        for index in range(count):
            link = self.server.attach(f"s{index}", rows=20 + index, cols=100 + index)
            self.assertIsNotNone(link, f"session s{index} should have attached")
            self.links.append(link)

        for index, link in enumerate(self.links):
            link.run(f"export WHO=s{index}\n", settle=0.4)

        for index, link in enumerate(self.links):
            answer = link.run("echo [$WHO-$(stty size | tr ' ' x)]\n")
            self.assertIn(f"[s{index}-{20 + index}x{100 + index}]".encode(), answer)

        self.assertEqual(len(self.server.sessions()), count)
        self.assertEqual(len(self.server.shells()), count)

    def test_one_client_dying_does_not_disturb_the_others(self):
        alive = self.server.attach("alive")
        doomed = self.server.attach("doomed")
        self.links.append(alive)

        doomed.close()
        time.sleep(1.2)

        self.assertIn(b"STILL_HERE", alive.run("echo STILL_HERE\n"))
        self.assertIn("doomed", self.server.sessions(),
                      "the dead client's session should persist, not vanish")

    def test_the_connection_limit_is_enforced(self):
        pending = []

        for _ in range(24):
            try:
                link = self.server.connect()
                link.send(dterm.HELLO, b"DTRM" + bytes([dterm.VERSION]))
                pending.append(link)
            except OSError:
                break

        time.sleep(1.0)
        connections = self.server.server_processes()

        # Guard against the check passing because nothing was found at all.
        self.assertGreater(len(connections), 1, "the connections should be visible")

        # One listener plus at most MAX_CLIENTS connection processes.
        self.assertLessEqual(len(connections), 17, "the 16-client limit should hold")

        for link in pending:
            link.close()

    def test_half_open_connections_time_out_on_their_own(self):
        for _ in range(5):
            link = self.server.connect()
            link.send(dterm.HELLO, b"DTRM" + bytes([dterm.VERSION]))
            link.close()

        time.sleep(7.0)
        self.assertEqual(len(self.server.server_processes()), 1,
                         "only the listener should remain")
