from pathlib import Path
import argparse

import matplotlib.pyplot as plt


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Plot latency histogram from a benchmark results directory."
    )
    parser.add_argument(
        "results_dir",
        nargs="?",
        default="benchmarks/saved",
        help="Directory containing hist.txt. Images are written to this same directory.",
    )

    args = parser.parse_args()

    results_dir = Path(args.results_dir).resolve()
    hist_path = results_dir / "hist.txt"

    if not hist_path.exists():
        raise FileNotFoundError(f"Histogram file not found: {hist_path}")

    x = []
    y = []

    with hist_path.open() as f:
        for line in f:
            a, b = line.split()
            x.append(int(a))
            y.append(int(b))

    plt.figure()
    plt.plot(x, y)
    plt.xlabel("Latency (cycles)")
    plt.ylabel("Frequency")
    plt.title("Latency Distribution")
    plt.tight_layout()

    plt.savefig(results_dir / "latency.png", dpi=150)

    plt.yscale("log")
    plt.savefig(results_dir / "latencyLog.png", dpi=150)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
