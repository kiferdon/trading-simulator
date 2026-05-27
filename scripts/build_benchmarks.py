#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

from common import (
    get_build_directory,
    find_project_root
)


def run_command(
        command: list[str],
        cwd: Path | None = None
) -> None:
    print(f"Running: {' '.join(command)}\n")

    subprocess.run(
        command,
        cwd=cwd,
        check=True
    )


def get_binary_path(
        project_dir: Path,
        preset: str,
        benchmark_name: str
) -> Path:
    return (
            get_build_directory(
                project_dir,
                preset
            ) /
            benchmark_name
    ).resolve()


def configure_project(
        project_dir: Path,
        preset: str
) -> None:
    run_command([
        "cmake",
        "--preset",
        preset,
        "-S",
        str(project_dir)
    ])


def build_target(
        project_dir: Path,
        preset: str,
        target: str
) -> None:
    build_directory = get_build_directory(
        project_dir,
        preset
    )

    run_command([
        "cmake",
        "--build",
        str(build_directory),
        "--target",
        target
    ])


def build_targets(
        project_dir: Path,
        preset: str,
        targets: list[str]
) -> None:
    configure_project(
        project_dir,
        preset
    )

    unique_targets = sorted(
        set(targets)
    )

    for target in unique_targets:
        build_target(
            project_dir,
            preset,
            target
        )


def validate_binary_exists(
        binary_path: Path
) -> None:
    if not binary_path.exists():
        raise FileNotFoundError(
            f"Benchmark binary not found: {binary_path}"
        )


def main():
    script_dir = Path(__file__).resolve().parent

    project_dir = find_project_root(
        script_dir
    )

    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--project-dir",
        default=str(project_dir)
    )

    parser.add_argument(
        "--preset",
        default="release"
    )

    parser.add_argument(
        "--target",
        action="append",
        required=True
    )

    args = parser.parse_args()

    resolved_project_dir = Path(
        args.project_dir
    ).resolve()

    build_targets(
        project_dir=resolved_project_dir,
        preset=args.preset,
        targets=args.target
    )

    print("\nBuild completed.\n")

    for target in sorted(set(args.target)):
        binary_path = get_binary_path(
            resolved_project_dir,
            args.preset,
            target
        )

        validate_binary_exists(
            binary_path
        )

        print(binary_path)


if __name__ == "__main__":
    main()
