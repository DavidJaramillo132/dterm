"""A session is a shell that outlives the connection that created it."""

import os
import re
import struct
import time
import unittest

import dterm
from dterm import INPUT, OUTPUT, RESIZE


class SessionTest(unittest.TestCase):

    def setUp(self):
        self.server = dterm.Server().start()

    def tearDown(self):
        self.server.stop()

    def test_attaching_creates_a_session(self):
        link = self.server.attach("work")
        self.assertIsNotNone(link)
        self.assertEqual(self.server.sessions(), ["work"])
        self.assertEqual(len(self.server.shells()), 1)
        link.close()

    def test_the_shell_really_runs_commands(self):
        link = self.server.attach("work")
        self.assertIn(b"7-6-42", link.run("echo $((1+6))-$((3+3))-$((6*7))\n"))
        link.close()

    def test_the_client_decides_the_window_size(self):
        link = self.server.attach("work", rows=45, cols=150)
        self.assertIn(b"45 150", link.run("stty size\n"))

        link.send(RESIZE, struct.pack(">HH", 30, 100))
        time.sleep(0.5)
        self.assertIn(b"30 100", link.run("stty size\n"))
        link.close()

    def test_the_shell_survives_a_dropped_connection(self):
        link = self.server.attach("work")
        link.run("export MARK=survived\n")
        link.close()

        time.sleep(1.2)
        self.assertEqual(len(self.server.shells()), 1, "the shell should still be alive")
        self.assertEqual(self.server.sessions(), ["work"])

    def test_reattaching_lands_in_the_same_shell(self):
        first = self.server.attach("work")
        first.run("export MARK=survived\n")
        first.close()
        time.sleep(1.2)

        second = self.server.attach("work")
        self.assertIsNotNone(second)
        self.assertIn(b"MARK=survived", second.run("echo MARK=$MARK\n"))
        second.close()

    def test_reattaching_replays_the_recent_screen(self):
        first = self.server.attach("work")
        first.run("echo UNIQUE_MARKER_9F2A\n")
        first.close()
        time.sleep(1.2)

        second = self.server.authenticate()
        second.send(dterm.ATTACH, b"work")
        replayed = second.drain_output(1.5)
        self.assertIn(b"UNIQUE_MARKER_9F2A", replayed)
        second.close()

    def test_background_work_continues_while_detached(self):
        counter = os.path.join(self.server.home, "counter")
        link = self.server.attach("work")
        link.run(f"bash -c 'for i in $(seq 1 200); do echo $i >> {counter}; "
                 f"sleep 0.2; done' >/dev/null 2>&1 & echo $! > {self.server.home}/jobpid\n")
        link.close()

        time.sleep(1.0)
        before = len(open(counter).read().split())
        time.sleep(2.0)
        after = len(open(counter).read().split())

        self.assertGreater(after, before,
                           "the background job should keep running while nobody is attached")

        os.kill(int(open(f"{self.server.home}/jobpid").read().strip()), 9)

    def test_different_names_are_different_shells(self):
        one = self.server.attach("alpha")
        two = self.server.attach("beta")

        one.run("export WHO=alpha\n")
        self.assertIn(b"WHO=[]", two.run("echo WHO=[$WHO]\n"))
        self.assertIn(b"WHO=[alpha]", one.run("echo WHO=[$WHO]\n"))

        one.close()
        two.close()

    def test_sessions_survive_the_server_being_killed(self):
        link = self.server.attach("work")
        link.run("export MARK=persistent\n")
        link.close()

        self.server.kill()
        time.sleep(1.0)
        self.assertEqual(len(self.server.shells()), 1,
                         "killing the listener must not kill the sessions")

        self.server.restart()
        again = self.server.attach("work")
        self.assertIsNotNone(again)
        self.assertIn(b"MARK=persistent", again.run("echo MARK=$MARK\n"))
        again.close()

    def test_a_second_client_takes_the_session_over(self):
        first = self.server.attach("work")
        second = self.server.attach("work")

        self.assertIn(b"TAKEOVER_OK", second.run("echo TAKEOVER_OK\n"))
        self.assertIsNone(first.read_frame(timeout=2),
                          "the first client should have been detached")

        first.close()
        second.close()

    def test_exiting_the_shell_ends_the_session(self):
        link = self.server.attach("work")
        link.run("exit\n", settle=1.5)
        link.close()

        time.sleep(1.5)
        self.assertEqual(self.server.sessions(), [])
        self.assertEqual(self.server.shells(), [])

    def test_a_live_background_job_keeps_the_session_open(self):
        link = self.server.attach("work")
        marker = os.path.join(self.server.home, "jobpid")
        link.run(f"bash -c 'sleep 30' >/dev/null 2>&1 & echo $! > {marker}\n")
        link.run("exit\n", settle=1.5)
        link.close()

        time.sleep(1.5)
        self.assertEqual(self.server.sessions(), ["work"],
                         "a process still holding the PTY keeps the session alive")

        os.kill(int(open(marker).read().strip()), 9)


class StartDirectoryTest(unittest.TestCase):
    """Where a new shell begins, and the guarantee that reattaching never moves it."""

    def setUp(self):
        self.server = dterm.Server()

    def tearDown(self):
        self.server.stop()

    def make(self, name):
        path = os.path.join(self.server.home, name)
        os.makedirs(path)
        return path

    def start_in(self, start_dir):
        self.server.start_dir = start_dir
        self.server.start()

    @staticmethod
    def cwd(link):
        # The typed command comes back echoed with $PWD unexpanded, so only a
        # match that starts with "/" is the shell's actual answer.
        found = [m for m in re.findall(rb"CWD=\[([^\]]*)\]", link.run("echo CWD=[$PWD]\n"))
                 if m.startswith(b"/")]
        return found[-1].decode() if found else None

    def test_a_new_session_starts_in_the_home_directory_by_default(self):
        # Not wherever the server happened to be launched from: the test runner
        # starts it from the build directory, which is precisely what this rules out.
        self.start_in(None)
        link = self.server.attach("work")
        self.assertEqual(self.cwd(link), self.server.home)
        link.close()

    def test_a_new_session_starts_in_the_configured_directory(self):
        projects = self.make("Projects")
        self.start_in(projects)
        link = self.server.attach("work")
        self.assertEqual(self.cwd(link), projects)
        link.close()

    def test_a_leading_tilde_is_expanded_against_home(self):
        # What a systemd unit would pass: no shell ever expanded it.
        projects = self.make("Projects")
        self.start_in("~/Projects")
        link = self.server.attach("work")
        self.assertEqual(self.cwd(link), projects)
        link.close()

    def test_a_missing_directory_falls_back_to_home(self):
        self.start_in(os.path.join(self.server.home, "does-not-exist"))
        link = self.server.attach("work")
        self.assertEqual(self.cwd(link), self.server.home,
                         "a bad setting must still produce a usable shell")
        link.close()

    def test_reattaching_never_moves_the_shell(self):
        # The start directory applies when a session is created. Applying it on
        # every attach would yank the user out of wherever they had gone, which
        # is exactly the state a persistent session exists to keep.
        projects = self.make("Projects")
        elsewhere = self.make("elsewhere")
        self.start_in(projects)

        link = self.server.attach("work")
        link.run(f"cd {elsewhere}\n")
        link.close()

        link = self.server.attach("work")
        self.assertEqual(self.cwd(link), elsewhere)
        link.close()
