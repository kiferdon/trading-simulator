#!/usr/bin/env python3

import argparse
import shlex
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from build_benchmarks import (
    configure_project,
    build_target
)
from common import (
    resolve_project_path,
    find_project_root, run_live_output
)


def run_command(command: list[str]) -> None:
    run_live_output(command)


def capture_command(
        command: list[str],
        cwd: Path | None = None
) -> str:
    return subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True
    ).stdout.strip()


def collect_metadata(
        project_dir: Path,
        build_dir: Path,
        benchmarks: list[str]
) -> str:
    git_commit = capture_command(
        ["git", "rev-parse", "HEAD"],
        cwd=project_dir
    )

    git_status = capture_command(
        ["git", "status", "--short"],
        cwd=project_dir
    )

    compiler = capture_command([
        "bash",
        "-c",
        "g++ --version | head -n 1"
    ])

    cpu = capture_command([
        "bash",
        "-c",
        (
            "lscpu | grep -E "
            "'Model name|CPU\\(s\\)|Thread|Core|Socket|MHz|NUMA'"
        )
    ])

    return "\n".join([
        f"Date: {datetime.now().isoformat()}",
        f"Git commit: {git_commit}",
        "",
        "Benchmarks:",
        *benchmarks,
        "",
        "Git status:",
        git_status,
        "",
        f"Compiler: {compiler}",
        "",
        "CPU:",
        cpu,
        "",
        f"Build directory: {build_dir.resolve()}",
    ])


def run_benchmark(
        benchmark_runner: Path,
        benchmark_name: str,
        benchmark_binary: Path,
        output_dir: Path,
        runs: int,
        market_data: Path,
        extra_args: list[str]
) -> None:
    command = [
        sys.executable,
        str(benchmark_runner),

        "--name",
        benchmark_name,

        "--binary",
        str(benchmark_binary),

        "--output-dir",
        str(output_dir),

        "--",

        str(runs),
        str(market_data),
        *extra_args
    ]

    run_command(command)


def run_perf_stat(
        perf_stat_script: Path,
        benchmark_name: str,
        project_dir: Path,
        preset: str,
        runs: int,
        cpu: int,
        market_data: Path,
        output_file: Path,
        extra_args: list[str]
) -> None:
    command = [
        sys.executable,
        str(perf_stat_script),

        "--project-dir",
        str(project_dir),

        "--preset",
        preset,

        "--benchmark",
        benchmark_name,

        "--runs",
        str(runs),

        "--cpu",
        str(cpu),

        "--market-data",
        str(market_data),

        "--output",
        str(output_file),

        "--",
        *extra_args
    ]

    run_command(command)


def parse_benchmark_argument(
        benchmark_argument: str
) -> tuple[str, int]:
    split = benchmark_argument.split(":")

    if len(split) != 2:
        raise ValueError(
            f"Invalid benchmark format: {benchmark_argument}"
        )

    benchmark_name = split[0]
    runs = int(split[1])

    return benchmark_name, runs


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
        "--name",
        required=True
    )

    parser.add_argument(
        "--market-data",
        default="market-data/12302019.NASDAQ_ITCH50"
    )

    parser.add_argument(
        "--benchmark",
        action="append",
        required=True,
        help="Format: benchmark_name:runs"
    )

    parser.add_argument(
        "--perf-stat",
        action="append",
        default=[],
        help="Format: benchmark_name:runs"
    )

    parser.add_argument(
        "--benchmark-args",
        action="append",
        default=[],
        metavar="NAME=ARGS",
        help=(
            "Extra arguments for a scheduled executable, in both benchmark and "
            "perf runs. Quote ARGS using shell syntax; repeated entries append."
        )
    )

    parser.add_argument(
        "--perf-cpu",
        type=int,
        default=2
    )

    parser.add_argument(
        "--benchmark-runner",
        default=str(script_dir / "run_benchmark.py")
    )

    parser.add_argument(
        "--perf-stat-script",
        default=str(script_dir / "perf_stat.py")
    )

    args = parser.parse_args()

    benchmark_entries = [
        parse_benchmark_argument(entry)
        for entry in args.benchmark
    ]
    perf_entries = [
        parse_benchmark_argument(entry)
        for entry in args.perf_stat
    ]
    benchmark_names = sorted({
        benchmark_name
        for benchmark_name, _ in benchmark_entries + perf_entries
    })
    extra_args_by_name: dict[str, list[str]] = {}
    for entry in args.benchmark_args:
        benchmark_name, separator, arguments = entry.partition("=")
        if not separator or benchmark_name not in benchmark_names:
            parser.error(
                "--benchmark-args requires NAME=ARGS with NAME matching a "
                "--benchmark or --perf-stat entry"
            )
        try:
            extra_args = shlex.split(arguments)
        except ValueError as error:
            parser.error(f"Invalid --benchmark-args for {benchmark_name}: {error}")
        extra_args_by_name.setdefault(benchmark_name, []).extend(extra_args)

    project_dir = Path(
        args.project_dir
    ).resolve()

    build_dir = (
            project_dir /
            f"cmake-build-{args.preset}"
    )

    market_data = resolve_project_path(
        project_dir,
        args.market_data
    )

    if not market_data.exists():
        raise FileNotFoundError(
            f"Market data not found: {market_data}"
        )

    benchmark_runner = Path(
        args.benchmark_runner
    ).resolve()

    perf_stat_script = Path(
        args.perf_stat_script
    ).resolve()

    benchmark_root = (
            project_dir /
            "benchmarks" /
            "saved" /
            args.name
    )

    benchmark_root.mkdir(
        parents=True,
        exist_ok=True
    )

    # Configure once

    configure_project(
        project_dir,
        args.preset
    )

    # Build all targets

    for benchmark_name in benchmark_names:
        build_target(
            project_dir,
            args.preset,
            benchmark_name
        )

    # Metadata

    metadata = collect_metadata(
        project_dir=project_dir,
        build_dir=build_dir,
        benchmarks=benchmark_names
    )
    metadata += "\n\nExtra benchmark arguments:\n" + "\n".join(
        f"{name}: {shlex.join(extra_args_by_name.get(name, []))}"
        for name in benchmark_names
    )

    (
            benchmark_root /
            "metadata.txt"
    ).write_text(metadata)

    # Run benchmarks

    for benchmark_name, runs in benchmark_entries:
        benchmark_binary = (
                build_dir /
                benchmark_name
        ).resolve()

        run_benchmark(
            benchmark_runner=benchmark_runner,
            benchmark_name=benchmark_name,
            benchmark_binary=benchmark_binary,
            output_dir=benchmark_root,
            runs=runs,
            market_data=market_data,
            extra_args=extra_args_by_name.get(benchmark_name, [])
        )

    # Run perf stat

    for benchmark_name, runs in perf_entries:
        output_file = (
                benchmark_root /
                f"{benchmark_name}_perf_stat.txt"
        )

        run_perf_stat(
            perf_stat_script=perf_stat_script,
            benchmark_name=benchmark_name,
            project_dir=project_dir,
            preset=args.preset,
            runs=runs,
            cpu=args.perf_cpu,
            market_data=market_data,
            output_file=output_file,
            extra_args=extra_args_by_name.get(benchmark_name, [])
        )

    print("\nBenchmark pipeline completed.\n")
    print(f"Results saved to:\n{benchmark_root}")


if __name__ == "__main__":
    main()
