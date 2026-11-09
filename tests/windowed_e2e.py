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
    args = parser.parse_args()
    environment = os.environ.copy()
    environment.pop("WAYLAND_DISPLAY", None)
    try:
        completed = subprocess.run(
            [args.binary],
            input=INPUT,
            text=True,
            capture_output=True,
            timeout=20,
            env=environment,
            check=False,
        )
    except subprocess.TimeoutExpired:
        print("windowed Vulkan application did not stop", file=sys.stderr)
        return 1
    if completed.returncode != 0 or completed.stdout or completed.stderr:
        print(
            f"unexpected windowed result: "
            f"{completed.returncode=} {completed.stdout=!r} "
            f"{completed.stderr=!r}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
