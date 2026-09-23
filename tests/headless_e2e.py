#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from typing import Iterable, Sequence, Tuple


Triangle = Tuple[float, float, float, float, float, float, float, float, float]


@dataclass(frozen=True)
class GeometryCase:
    name: str
    triangles: Sequence[Triangle]
    expected: Sequence[bool]


BASE: Triangle = (0, 0, 0, 2, 0, 0, 0, 2, 0)
INNER: Triangle = (0.25, 0.25, 0, 0.75, 0.25, 0, 0.25, 0.75, 0)
COPLANAR_CROSSING: Triangle = (1, -0.5, 0, 1, 2.5, 0, 1.5, 1, 0)
COPLANAR_OUTSIDE: Triangle = (1.25, 1.25, 0, 3, 1.25, 0, 1.25, 3, 0)
SHARED_EDGE: Triangle = (0, 0, 0, 2, 0, 0, 1, -1, 0)
SHARED_VERTEX: Triangle = (2, 0, 0, 3, 0, 0, 2, 1, 0)
VERTICAL: Triangle = (0.5, 0.5, -1, 0.5, 0.5, 1, 0.5, 1.5, 0)
ABOVE: Triangle = (0, 0, 1, 2, 0, 1, 0, 2, 1)
FAR: Triangle = (100, 100, 100, 101, 100, 100, 100, 101, 100)
SMALL_GAP: Triangle = (2.001, 0, 0, 3, 0, 0, 2.001, 1, 0)


def translated(triangle: Triangle, x: float, y: float, z: float) -> Triangle:
    values = []
    for index in range(0, 9, 3):
        values.extend(
            (triangle[index] + x, triangle[index + 1] + y, triangle[index + 2] + z)
        )
    return tuple(values)  # type: ignore[return-value]


def scaled(triangle: Triangle, factor: float) -> Triangle:
    return tuple(value * factor for value in triangle)  # type: ignore[return-value]


def point(x: float, y: float, z: float) -> Triangle:
    return (x, y, z, x, y, z, x, y, z)


def segment(first: Tuple[float, float, float], second: Tuple[float, float, float]) -> Triangle:
    return first + second + second


def triangle_at(x: float) -> Triangle:
    return (x, 0, 0, x + 1, 0, 0, x, 1, 0)


def encode_triangles(triangles: Sequence[Triangle], newline: str = "\n") -> str:
    lines = [str(len(triangles))]
    for triangle in triangles:
        for index in range(0, 9, 3):
            lines.append(" ".join(str(value) for value in triangle[index : index + 3]))
    return newline.join(lines) + newline


def encode_flags(flags: Iterable[bool]) -> str:
    return " ".join("1" if value else "0" for value in flags) + "\n"


COMMON_CASES = (
    GeometryCase("single", (BASE,), (False,)),
    GeometryCase("identical", (BASE, BASE), (True, True)),
    GeometryCase("containment", (BASE, INNER), (True, True)),
    GeometryCase("coplanar edge crossing", (BASE, COPLANAR_CROSSING), (True, True)),
    GeometryCase("overlapping AABBs without collision", (BASE, COPLANAR_OUTSIDE), (False, False)),
    GeometryCase("shared edge", (BASE, SHARED_EDGE), (True, True)),
    GeometryCase("shared vertex", (BASE, SHARED_VERTEX), (True, True)),
    GeometryCase("non-coplanar intersection", (BASE, VERTICAL), (True, True)),
    GeometryCase("parallel planes", (BASE, ABOVE), (False, False)),
    GeometryCase("disjoint AABBs", (BASE, FAR), (False, False)),
    GeometryCase("small positive gap", (BASE, SMALL_GAP), (False, False)),
    GeometryCase(
        "reversed winding",
        (BASE, (BASE[0], BASE[1], BASE[2], BASE[6], BASE[7], BASE[8], BASE[3], BASE[4], BASE[5])),
        (True, True),
    ),
    GeometryCase("mixed flags", (BASE, VERTICAL, FAR), (True, True, False)),
    GeometryCase(
        "independent groups",
        (BASE, INNER, translated(BASE, 20, 0, 0), translated(INNER, 20, 0, 0), FAR),
        (True, True, True, True, False),
    ),
    GeometryCase(
        "sparse input order",
        (
            translated(INNER, 1000, 0, 0),
            translated(BASE, -1000, 0, 0),
            BASE,
            translated(BASE, 1000, 0, 0),
            INNER,
        ),
        (True, False, True, True, True),
    ),
    GeometryCase(
        "negative coordinates",
        (translated(BASE, -10, -10, -10), translated(BASE, -10, -10, -10)),
        (True, True),
    ),
    GeometryCase(
        "large translation",
        (translated(BASE, 10000, -10000, 5000), translated(INNER, 10000, -10000, 5000)),
        (True, True),
    ),
    GeometryCase("small scale", (scaled(BASE, 0.1), scaled(INNER, 0.1)), (True, True)),
)


CPU_CASES = (
    GeometryCase("equal points", (point(1, 1, 1), point(1, 1, 1)), (True, True)),
    GeometryCase("different points", (point(1, 1, 1), point(1, 1, 1.1)), (False, False)),
    GeometryCase("point inside triangle", (BASE, point(0.5, 0.5, 0)), (True, True)),
    GeometryCase("point on triangle edge", (BASE, point(1, 1, 0)), (True, True)),
    GeometryCase("point outside triangle", (BASE, point(2, 2, 0)), (False, False)),
    GeometryCase(
        "segment pierces triangle",
        (BASE, segment((0.5, 0.5, -1), (0.5, 0.5, 1))),
        (True, True),
    ),
    GeometryCase(
        "coplanar segment crosses triangle",
        (BASE, segment((-1, 1, 0), (2, 1, 0))),
        (True, True),
    ),
    GeometryCase(
        "segment misses triangle",
        (BASE, segment((3, 3, -1), (3, 3, 1))),
        (False, False),
    ),
    GeometryCase(
        "crossing segments",
        (segment((-1, 0, 0), (1, 0, 0)), segment((0, -1, 0), (0, 1, 0))),
        (True, True),
    ),
    GeometryCase(
        "collinear segments",
        (
            segment((0, 0, 0), (2, 0, 0)),
            segment((1, 0, 0), (3, 0, 0)),
            segment((4, 0, 0), (5, 0, 0)),
        ),
        (True, True, False),
    ),
    GeometryCase(
        "skew segments",
        (segment((0, 0, 0), (1, 0, 0)), segment((0.5, -1, 1), (0.5, 1, 1))),
        (False, False),
    ),
)


def gpu_cases() -> Sequence[GeometryCase]:
    cases = [
        GeometryCase("GPU ignores degenerate point", (BASE, point(0.5, 0.5, 0)), (False, False))
    ]
    for count in (63, 64, 65, 127, 128, 129):
        triangles = tuple(triangle_at(index * 4) for index in range(count))
        cases.append(
            GeometryCase(
                "workgroup size {}".format(count),
                triangles,
                tuple(False for _ in range(count)),
            )
        )

    triangles = [triangle_at(index * 4) for index in range(129)]
    triangles[64] = triangles[63]
    expected = [False] * len(triangles)
    expected[63] = True
    expected[64] = True
    cases.append(
        GeometryCase(
            "collision across workgroup boundary",
            tuple(triangles),
            tuple(expected),
        )
    )
    return tuple(cases)


class Runner:
    def __init__(self, binary: str, backend: str) -> None:
        self.binary = binary
        self.backend = backend
        self.count = 0
        self.environment = os.environ.copy()
        if backend == "cpu":
            self.environment.pop("DISPLAY", None)
            self.environment.pop("WAYLAND_DISPLAY", None)

    def run(
        self,
        name: str,
        arguments: Sequence[str],
        input_data: str,
        expected_code: int,
        expected_stdout: str,
        expected_stderr: str = "",
    ) -> None:
        self.count += 1
        command = [self.binary] + list(arguments)
        try:
            completed = subprocess.run(
                command,
                input=input_data,
                text=True,
                capture_output=True,
                timeout=15,
                env=self.environment,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            raise AssertionError("{} timed out: {}".format(name, command)) from error

        actual = (completed.returncode, completed.stdout, completed.stderr)
        expected = (expected_code, expected_stdout, expected_stderr)
        if actual != expected:
            raise AssertionError(
                "{} failed\ncommand: {}\nexpected: {!r}\nactual: {!r}\ninput:\n{}".format(
                    name, command, expected, actual, input_data
                )
            )

    def geometry(self, case: GeometryCase) -> None:
        arguments = ["--headless"]
        if self.backend == "gpu":
            arguments.append("--use-gpu-intersections")
        self.run(
            case.name,
            arguments,
            encode_triangles(case.triangles),
            0,
            encode_flags(case.expected),
        )


def run_cpu_process_cases(runner: Runner) -> None:
    runner.run(
        "arbitrary whitespace and trailing data",
        ("--headless",),
        "  1\r\n\t0 0 0\r\n2\t0 0\n0 2 0\r\n trailing data",
        0,
        "0\n",
    )
    runner.run("empty input", ("--headless",), "", 1, "", "failed to read triangle count\n")
    runner.run(
        "non-numeric count",
        ("--headless",),
        "triangle\n",
        1,
        "",
        "failed to read triangle count\n",
    )
    for count in ("0", "-1", "1000000"):
        runner.run(
            "invalid count {}".format(count),
            ("--headless",),
            count + "\n",
            1,
            "",
            "triangle count must be in the range (0, 1000000)\n",
        )
    runner.run(
        "incomplete second triangle",
        ("--headless",),
        encode_triangles((BASE,)).replace("1\n", "2\n", 1),
        1,
        "",
        "failed to read triangle 2\n",
    )
    runner.run(
        "unknown option",
        ("--unknown",),
        "",
        1,
        "",
        "unknown command-line option: --unknown\n",
    )
    runner.run(
        "duplicate headless option",
        ("--headless", "--headless"),
        "",
        1,
        "",
        "duplicate command-line option: --headless\n",
    )
    runner.run(
        "duplicate GPU option",
        ("--use-gpu-intersections", "--use-gpu-intersections"),
        "",
        1,
        "",
        "duplicate command-line option: --use-gpu-intersections\n",
    )


def run_gpu_cli_cases(runner: Runner) -> None:
    input_data = encode_triangles((BASE,))
    runner.run(
        "GPU flag before headless",
        ("--use-gpu-intersections", "--headless"),
        input_data,
        0,
        "0\n",
    )
    runner.run(
        "headless before GPU flag",
        ("--headless", "--use-gpu-intersections"),
        input_data,
        0,
        "0\n",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--backend", choices=("cpu", "gpu"), required=True)
    arguments = parser.parse_args()

    runner = Runner(arguments.binary, arguments.backend)
    for case in COMMON_CASES:
        runner.geometry(case)

    if arguments.backend == "cpu":
        for case in CPU_CASES:
            runner.geometry(case)
        run_cpu_process_cases(runner)
    else:
        for case in gpu_cases():
            runner.geometry(case)
        run_gpu_cli_cases(runner)

    print("{} headless e2e scenarios passed for {}".format(runner.count, arguments.backend))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(error, file=sys.stderr)
        sys.exit(1)
