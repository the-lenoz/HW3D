#!/usr/bin/env python3

import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    arguments = parser.parse_args()

    for option in ("--headless", "--use-gpu-intersections", "--unknown"):
        completed = subprocess.run(
            [arguments.binary, option],
            text=True,
            capture_output=True,
            timeout=10,
            check=False,
        )
        expected = (
            1,
            "",
            f"unknown command-line option: {option}\n",
        )
        actual = (
            completed.returncode,
            completed.stdout,
            completed.stderr,
        )
        if actual != expected:
            print(
                f"unexpected CLI result for {option}: {actual!r}",
                file=sys.stderr,
            )
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
