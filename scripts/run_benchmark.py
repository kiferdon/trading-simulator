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


def metric_summary(values) -> str:
    if not values:
        return "median=0.0 min=0.0 max=0.0 MAD=0.0"
    center = statistics.median(values)
    mad = statistics.median(abs(value - center) for value in values)
    return (
        f"median={center} min={min(values)} max={max(values)} MAD={mad}"
    )


def extract(results, key):
    return [r[key] for r in results if key in r]


def build_report(results: list[dict]) -> str:
    if not results:
        return "No benchmark results"

    first = results[0]
    # Older benchmark formats have no validity flag. An explicit flag must be
    # true before a run contributes to performance comparisons.
    performance_results = [
        result for result in results if result.get("valid", True) is True
    ]
    metric_names = sorted({
        key
        for result in performance_results
        for key, value in result.items()
        if isinstance(value, (int, float))
        and not isinstance(value, bool)
        and (
            isinstance(value, float)
            or key.endswith("_cycles")
            or key.endswith("_hz")
        )
    })

    lines = ["Benchmark result distribution", ""]
    if "effective_config" in first:
        lines.extend([
            "Effective config:",
            json.dumps(first["effective_config"], indent=2),
            f"Config path: {first.get('config_path', '')}",
            ""
        ])

    valid_runs = sum(result.get("valid") is True for result in results)
    if any("valid" in result for result in results):
        lines.extend([f"Valid runs: {valid_runs}/{len(results)}", ""])

    if not performance_results:
        lines.append("No valid runs; performance statistics unavailable.")
    for name in metric_names:
        lines.append(f"{name}: {metric_summary(extract(performance_results, name))}")

    journal_keys = sorted({
        key
        for result in results
        for key, value in result.get("journal", {}).items()
        if isinstance(value, (int, float)) and not isinstance(value, bool)
    })
    if journal_keys:
        lines.extend(["", "Journal diagnostics (all runs):"])
        for key in journal_keys:
            values = [
                result["journal"][key]
                for result in results
                if key in result.get("journal", {})
            ]
            lines.append(f"{key}: {metric_summary(values)}")
        logger_failures = sum(
            result.get("journal", {}).get("logger_failed") is True
            for result in results
        )
        lines.append(f"Logger failures: {logger_failures}/{len(results)}")

    for index, result in enumerate(results, start=1):
        if result.get("valid", True) is True:
            continue
        journal = result.get("journal", {})
        validation = result.get("journal_validation", {})
        lines.extend([
            "",
            f"Invalid run {index}: input_complete={result.get('input_complete')}, "
            f"records_dropped={journal.get('records_dropped', 0)}, "
            f"logger_failed={journal.get('logger_failed', False)}, "
            f"validation_error={validation.get('error', '')}"
        ])

    lines.extend([
        "",
        f"First checksum: {first.get('checksum', 0)}",
        f"First parsed: {first.get('parsed', first.get('messages', 0))}",
        f"First skipped: {first.get('skipped', 0)}",
        f"First frames: {first.get('frames', 0)}",
        "",
        "Raw run files:",
        "stdout.txt",
        "stderr.txt",
        "output.jsonl"
    ])
    return "\n".join(lines)


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

    results = []
    if output_jsonl.exists() or process.returncode == 0:
        results = load_jsonl(output_jsonl)
        report = build_report(results)
        print("\n" + report + "\n")
        report_path.write_text(report)

    if process.returncode != 0:
        print(f"Binary failed with code {process.returncode}", file=sys.stderr)
        sys.exit(process.returncode)
    if any(result.get("valid", True) is not True for result in results):
        print("Benchmark contains invalid runs", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
