#!/usr/bin/env python3

import argparse
import json
import statistics
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        raise FileNotFoundError(f"Missing JSONL: {path}")

    return [
        json.loads(line)
        for line in path.read_text().splitlines()
        if line.strip()
    ]


def median(values):
    return statistics.median(values) if values else 0.0


def extract(results, key):
    return [r[key] for r in results if key in r]


def build_report(results: list[dict]) -> str:
    warmup = results[0] if results else {}

    seconds = extract(results, "seconds")
    mps = extract(results, "messages_per_second")
    gibps = extract(results, "gibps")

    return "\n".join([
        "Median throughput benchmark results\n",
        f"Warmup checksum: {warmup.get('checksum', 0)}",
        f"Warmup parsed: {warmup.get('messages', 0)}",
        f"Warmup skipped: {warmup.get('skipped', 0)}\n",

        f"Median Benchmark time: {median(seconds)}",
        f"Median Mps: {median(mps)}",
        f"Median GiBbps: {median(gibps)}\n",

        "Median Time before benchmark: 0",
        "Median Time after benchmark: 0",
        f"Median Total time: {median(seconds)}\n",

        "Raw run files:",
        "stdout.txt",
        "stderr.txt",
        "output.jsonl"
    ])


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--name", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output-dir", default="saved")
    parser.add_argument("benchmark_args", nargs=argparse.REMAINDER)

    args = parser.parse_args()

    if args.benchmark_args and args.benchmark_args[0] == "--":
        args.benchmark_args.pop(0)

    if len(args.benchmark_args) < 2:
        print("Expected: <runs> <market_file>", file=sys.stderr)
        sys.exit(1)

    runs = args.benchmark_args[0]
    market_file = args.benchmark_args[1]
    extra_args = args.benchmark_args[2:]

    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    run_dir = (Path.cwd() / args.output_dir / args.name / timestamp).resolve()
    ensure_directory(run_dir)

    output_jsonl = run_dir / "output.jsonl"
    stdout_path = run_dir / "stdout.txt"
    stderr_path = run_dir / "stderr.txt"
    report_path = run_dir / "report.txt"

    command = [
        args.binary,
        runs,
        market_file,
        str(output_jsonl),
        *extra_args
    ]

    print(f"Running: {' '.join(command)}\n")

    with open(stdout_path, "w") as out_f, open(stderr_path, "w") as err_f:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

        assert process.stdout is not None
        assert process.stderr is not None

        for line in process.stdout:
            print(line, end="", flush=True)
            out_f.write(line)

        for line in process.stderr:
            print(line, end="", flush=True)
            err_f.write(line)

        process.wait()

    if process.returncode != 0:
        print(f"Binary failed with code {process.returncode}", file=sys.stderr)
        sys.exit(process.returncode)

    results = load_jsonl(output_jsonl)
    report = build_report(results)

    print("\n" + report + "\n")

    report_path.write_text(report)


if __name__ == "__main__":
    main()
