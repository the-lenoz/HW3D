#!/usr/bin/env python3

import argparse
import sys

import numpy as np


def positive_float(value: str) -> float:
    parsed = float(value)
    if not np.isfinite(parsed) or parsed <= 0:
        raise argparse.ArgumentTypeError("значение должно быть конечным и больше нуля")
    return parsed


def finite_float(value: str) -> float:
    parsed = float(value)
    if not np.isfinite(parsed):
        raise argparse.ArgumentTypeError("значение должно быть конечным")
    return parsed


def triangle_count(value: str) -> int:
    parsed = int(value)
    if parsed <= 0 or parsed >= 1_000_000:
        raise argparse.ArgumentTypeError(
            "количество должно быть в диапазоне (0, 1000000)"
        )
    return parsed


def positive_seconds(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("длительность должна быть больше нуля")
    return parsed


def format_vector(vector: np.ndarray) -> str:
    return " ".join(f"{coordinate:.6f}" for coordinate in vector)


def generate_triangles(
    average_size: float,
    count: int,
    cube_size: float,
    duration_seconds: int,
    angular_speed: float,
    seed: int | None,
) -> None:
    generator = np.random.default_rng(seed)
    v1 = generator.uniform(0, cube_size, size=(count, 3))

    scale = average_size / np.sqrt(2)
    offsets_v2 = generator.normal(0, scale, size=(count, 3))
    offsets_v3 = generator.normal(0, scale, size=(count, 3))

    v2 = np.clip(v1 + offsets_v2, 0, cube_size)
    v3 = np.clip(v1 + offsets_v3, 0, cube_size)

    axis_origins = (v1 + v2 + v3) / 3.0
    axis_directions = generator.normal(size=(count, 3))
    direction_lengths = np.linalg.norm(axis_directions, axis=1)
    zero_directions = direction_lengths == 0.0
    axis_directions[zero_directions] = np.array([0.0, 0.0, 1.0])
    direction_lengths[zero_directions] = 1.0
    axis_directions /= direction_lengths[:, np.newaxis]
    axis_length = max(1.0, cube_size * 0.01)
    axis_second_points = axis_origins + axis_directions * axis_length

    angular_speeds = generator.uniform(
        0.0,
        abs(angular_speed),
        size=count,
    )
    if angular_speed < 0.0:
        angular_speeds *= -1.0

    print(count)
    print(duration_seconds)
    for index in range(count):
        print(format_vector(v1[index]))
        print(format_vector(v2[index]))
        print(format_vector(v3[index]))
        print(format_vector(axis_origins[index]))
        print(format_vector(axis_second_points[index]))
        print(f"{angular_speeds[index]:.6f}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Генерация входных данных для HW3D.",
    )
    parser.add_argument("average_size", type=positive_float, help="средний размер")
    parser.add_argument("count", type=triangle_count, help="количество треугольников")
    parser.add_argument("cube_size", type=positive_float, help="размер куба")
    parser.add_argument(
        "--duration",
        type=positive_seconds,
        default=10,
        help="длительность симуляции в секундах (по умолчанию: 10)",
    )
    parser.add_argument(
        "--angular-speed",
        type=finite_float,
        default=45.0,
        help=(
            "максимальный модуль случайной скорости в градусах/с: "
            "+ по часовой, - против часовой (по умолчанию: 45)"
        ),
    )
    parser.add_argument(
        "--seed",
        type=int,
        help="seed генератора для воспроизводимого результата",
    )
    return parser.parse_args()


if __name__ == "__main__":
    try:
        arguments = parse_arguments()
        generate_triangles(
            arguments.average_size,
            arguments.count,
            arguments.cube_size,
            arguments.duration,
            arguments.angular_speed,
            arguments.seed,
        )
    except BrokenPipeError:
        sys.exit(0)
