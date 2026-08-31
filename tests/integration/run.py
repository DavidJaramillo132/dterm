#!/usr/bin/env python3
"""Runs the DTerm integration suite.

    python3 tests/integration/run.py [pattern]

The binaries are taken from $DTERM_SERVER / $DTERM_CLIENT when set, and from
./build otherwise.
"""

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import dterm


def main():
    missing = [path for path in (dterm.SERVER, dterm.CLIENT) if not os.path.exists(path)]

    if missing:
        print("build the project first, these are missing:", *missing, sep="\n  ")
        return 1

    pattern = sys.argv[1] if len(sys.argv) > 1 else "test_*.py"
    suite = unittest.TestLoader().discover(HERE, pattern=pattern)
    result = unittest.TextTestRunner(verbosity=2).run(suite)

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
