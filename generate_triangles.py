import sys
import numpy as np


def generate_triangles(avg_size, count, cube_size):
    v1 = np.random.uniform(0, cube_size, size=(count, 3))

    scale = avg_size / np.sqrt(2)
    offsets_v2 = np.random.normal(0, scale, size=(count, 3))
    offsets_v3 = np.random.normal(0, scale, size=(count, 3))

    v2 = v1 + offsets_v2
    v3 = v1 + offsets_v3

    v2 = np.clip(v2, 0, cube_size)
    v3 = np.clip(v3, 0, cube_size)

    triangles = np.hstack((v1, v2, v3))

    print(count)

    for triangle in triangles:
        sys.stdout.write(" ".join(f"{num:.6f}" for num in triangle) + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(
            "Использование: python script.py <средний_размер> <количество> <размер_куба>",
            file=sys.stderr,
        )
        print("Пример: python script.py 5.0 1000 100.0", file=sys.stderr)
        sys.exit(1)

    try:
        avg_size = float(sys.argv[1])
        count = int(sys.argv[2])
        cube_size = float(sys.argv[3])

        if count <= 0 or avg_size <= 0 or cube_size <= 0:
            raise ValueError("Все параметры должны быть больше нуля.")

        generate_triangles(avg_size, count, cube_size)

    except ValueError as e:
        print(f"Ошибка в аргументах: {e}", file=sys.stderr)
        sys.exit(1)

