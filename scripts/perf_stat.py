#!/usr/bin/env python3

import argparse
import os
import subprocess
from pathlib import Path

from build_benchmarks import (
    configure_project,
    build_target
)
from common import find_project_root, run_live_output


def run_command(command: list[str]) -> None:
    print(f"Running: {' '.join(command)}\n")

    subprocess.run(
        command,
        check=True
    )


def resolve_project_path(
        project_dir: Path,
        path: str
) -> Path:
    candidate = Path(path)

    if candidate.is_absolute():
        return candidate.resolve()

    return (project_dir / candidate).resolve()


def create_fifo(path: Path) -> None:
    if path.exists():
        path.unlink()

    os.mkfifo(path)


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
        "--benchmark",
        required=True,
        help="Benchmark executable name"
    )

    parser.add_argument(
        "--runs",
        type=int,
        default=2
    )

    parser.add_argument(
        "--cpu",
        type=int,
        default=2
    )

    parser.add_argument(
        "--market-data",
        default="market-data/12302019.NASDAQ_ITCH50"
    )

    parser.add_argument(
        "--output",
        default="benchmarks/saved/perf-stat.txt"
    )

    parser.add_argument(
        "--events",
        nargs="*",
        default=[]
    )

    parser.add_argument(
        "benchmark_args",
        nargs=argparse.REMAINDER,
        help="Extra executable arguments after -- (for example -- --config path)"
    )

    args = parser.parse_args()
    if args.benchmark_args and args.benchmark_args[0] == "--":
        args.benchmark_args.pop(0)

    project_dir = Path(args.project_dir).resolve()

    build_dir = (
            project_dir /
            f"cmake-build-{args.preset}"
    )

    benchmark_binary = (
            build_dir /
            args.benchmark
    ).resolve()

    market_data = resolve_project_path(
        project_dir,
        args.market_data
    )

    output_file = resolve_project_path(
        project_dir,
        args.output
    )
    output_jsonl = output_file.with_name(output_file.name + ".jsonl")

    output_file.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    configure_project(
        project_dir,
        args.preset
    )

    build_target(
        project_dir,
        args.preset,
        args.benchmark
    )

    if not benchmark_binary.exists():
        raise FileNotFoundError(
            f"Benchmark binary not found: {benchmark_binary}"
        )

    if not market_data.exists():
        raise FileNotFoundError(
            f"Market data not found: {market_data}"
        )

    fifo_path = Path(
        f"/tmp/perf_stat_ctl_{os.getpid()}"
    )

    create_fifo(fifo_path)

    try:
        perf_command = [
            "perf",
            "stat",

            "-d",
            "-d",
            "-d",

            "--control",
            f"fifo:{fifo_path}",

            "--delay",
            "-1",
        ]

        if args.events:
            perf_command.extend([
                "-e",
                ",".join(args.events)
            ])

        perf_command.extend([
            "--",

            "taskset",
            "-c",
            str(args.cpu),

            str(benchmark_binary),
            str(args.runs),
            str(market_data),
            str(output_jsonl),
            *args.benchmark_args
        ])

        print("Running perf stat\n")
        print(f"Benchmark: {benchmark_binary}")
        print(f"CPU: {args.cpu}")
        print(f"FIFO: {fifo_path}")
        print(f"Output: {output_file}")
        print(f"Benchmark JSONL: {output_jsonl}\n")

        environment = os.environ.copy()

        environment["PERF_CTL_FIFO"] = str(fifo_path)

        run_live_output(perf_command, output_file, environment)

    finally:
        if fifo_path.exists():
            fifo_path.unlink()


if __name__ == "__main__":
    main()
