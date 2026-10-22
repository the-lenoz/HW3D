#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys


INPUT = """2
1
0 0 0
2 0 0
0 2 0
0 0 0
0 0 1
0
10 0 0
8 0 0
10 -2 0
5 0 0
5 0 5
-180
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--backend", choices=("cpu", "gpu"), required=True)
    arguments = parser.parse_args()

    command = [arguments.binary]
    if arguments.backend == "gpu":
        command.append("--use-gpu-intersections")
    environment = os.environ.copy()
    environment.pop("WAYLAND_DISPLAY", None)
    try:
        completed = subprocess.run(
            command,
            input=INPUT,
            text=True,
            capture_output=True,
            timeout=10,
            env=environment,
            check=False,
        )
    except subprocess.TimeoutExpired:
        print("windowed application did not stop after simulation duration", file=sys.stderr)
        return 1

    if completed.returncode != 0 or completed.stdout or completed.stderr:
        print(
            "unexpected windowed result: code={} stdout={!r} stderr={!r}".format(
                completed.returncode,
                completed.stdout,
                completed.stderr,
            ),
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
