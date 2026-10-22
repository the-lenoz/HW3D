#!/usr/bin/env python3

import argparse
import math
import subprocess
import sys
from pathlib import Path


def run_generator(script: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(script), *arguments],
        text=True,
        capture_output=True,
        timeout=20,
        check=False,
    )


def parse_output(output: str, expected_count: int, expected_duration: int) -> list[float]:
    lines = output.splitlines()
    assert int(lines[0]) == expected_count
    assert int(lines[1]) == expected_duration
    assert len(lines) == 2 + expected_count * 6

    speeds: list[float] = []
    for index in range(expected_count):
        start = 2 + index * 6
        vectors = [[float(value) for value in lines[start + offset].split()]
                   for offset in range(5)]
        assert all(len(vector) == 3 for vector in vectors)
        assert all(math.isfinite(value) for vector in vectors for value in vector)
        assert vectors[3] != vectors[4]
        speed = float(lines[start + 5])
        assert math.isfinite(speed)
        speeds.append(speed)
    return speeds


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generator", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    arguments = parser.parse_args()

    common = ("2.5", "4", "20", "--duration", "7", "--seed", "123")
    positive = run_generator(arguments.generator, *common, "--angular-speed", "90")
    repeated = run_generator(arguments.generator, *common, "--angular-speed", "90")
    assert positive.returncode == 0, positive.stderr
    assert positive.stdout == repeated.stdout
    assert all(0.0 <= value <= 90.0 for value in parse_output(positive.stdout, 4, 7))

    negative = run_generator(arguments.generator, *common, "--angular-speed", "-90")
    assert negative.returncode == 0, negative.stderr
    assert all(-90.0 <= value <= 0.0 for value in parse_output(negative.stdout, 4, 7))

    zero = run_generator(arguments.generator, *common, "--angular-speed", "0")
    assert zero.returncode == 0, zero.stderr
    assert all(value == 0.0 for value in parse_output(zero.stdout, 4, 7))

    for invalid in (
        ("2.5", "0", "20"),
        ("2.5", "4", "20", "--duration", "0"),
        ("2.5", "4", "20", "--angular-speed", "inf"),
    ):
        failed = run_generator(arguments.generator, *invalid)
        assert failed.returncode != 0
        assert failed.stderr

    application = subprocess.run(
        [str(arguments.binary), "--headless"],
        input=positive.stdout,
        text=True,
        capture_output=True,
        timeout=20,
        check=False,
    )
    assert application.returncode == 0, application.stderr
    assert application.stderr == ""
    assert len(application.stdout.split()) == 4
    assert set(application.stdout.split()) <= {"0", "1"}
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
