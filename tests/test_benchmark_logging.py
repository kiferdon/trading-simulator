import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from run_benchmark import build_report


class BenchmarkReportTest(unittest.TestCase):
    def test_invalid_runs_are_excluded_from_performance(self):
        report = build_report([
            {"valid": True, "total_frames_per_second": 100.0},
            {"valid": False, "total_frames_per_second": 1000.0,
             "journal": {"records_dropped": 900},
             "journal_validation": {"error": "journal validation mismatch"}},
        ])
        self.assertIn("Valid runs: 1/2", report)
        self.assertIn("total_frames_per_second: median=100.0", report)
        self.assertIn("Journal diagnostics (all runs):", report)
        self.assertIn("records_dropped: median=900", report)
        self.assertIn("Invalid run 2:", report)
        self.assertIn("validation_error=journal validation mismatch", report)

    def test_all_invalid_runs_have_no_performance_statistics(self):
        report = build_report([
            {"valid": False, "processing_seconds": 0.01, "p99_cycles": 500},
        ])
        self.assertIn("No valid runs; performance statistics unavailable.", report)
        self.assertNotIn("processing_seconds:", report)
        self.assertNotIn("p99_cycles:", report)

    def test_legacy_results_without_validity_are_supported(self):
        report = build_report([{"seconds": 1.0}, {"seconds": 3.0}])
        self.assertIn("seconds: median=2.0", report)
        self.assertNotIn("Invalid run", report)


class SimulatorBenchmarkCliTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binaries = [ROOT / "cmake-build-release" / name for name in
                        ("simulator_throughput", "simulator_latency")]
        for binary in cls.binaries:
            if not binary.exists():
                raise FileNotFoundError(f"Build {binary.name} with the release preset first")

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="hft_benchmark_cli_")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.data = self.directory / "sample.itch"
        frames = bytearray()
        for order_id, side, price in ((1, "B", 1000), (2, "S", 1001)):
            message = bytearray(36)
            message[0] = ord("A")
            message[1:3] = (7).to_bytes(2, "big")
            message[5:11] = order_id.to_bytes(6, "big")
            message[11:19] = order_id.to_bytes(8, "big")
            message[19] = ord(side)
            message[20:24] = (100).to_bytes(4, "big")
            message[32:36] = price.to_bytes(4, "big")
            frames += b"\x00\x24" + message
        self.data.write_bytes(frames)
        self.configs = {}
        for mode in ("on_stop", "disabled"):
            config = self.directory / f"{mode}.json"
            config.write_text(json.dumps({
                "cpu_affinity": None, "tracked_stocks": [7], "event_throttle": 1,
                "trade_journal": {"path": "journal.jsonl", "mode": mode,
                                  "logger_cpu": None, "idle_spin_count": 0},
            }))
            self.configs[mode] = config

    def run_benchmark(self, binary, output, mode="on_stop", runs=1):
        process = subprocess.run([
            str(binary), str(runs), str(self.data), str(output),
            "--config", str(self.configs[mode]),
        ], capture_output=True, text=True, timeout=30)
        results = [json.loads(line) for line in output.read_text().splitlines()]
        return process, results

    def test_invocations_preserve_each_others_artifacts(self):
        for binary in self.binaries:
            with self.subTest(binary=binary.name):
                first_output = self.directory / f"{binary.name}.jsonl"
                process, results = self.run_benchmark(binary, first_output, runs=2)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertFalse(results[0]["journal_retained"])
                self.assertFalse(Path(results[0]["journal_path"]).exists())
                self.assertTrue(results[1]["journal_retained"])
                journal = Path(results[1]["journal_path"])
                original = journal.read_bytes()
                self.assertEqual(len(original.splitlines()), 1)
                stats = Path(str(journal) + ".stats.json")
                original_stats = stats.read_bytes()
                histogram = journal.parent / "simulator_latency_histogram_run_1.hist"
                original_histogram = histogram.read_bytes() if histogram.exists() else None

                # A different result filename in the same directory must not
                # delete an enabled journal when logging is disabled.
                process, _ = self.run_benchmark(
                    binary, self.directory / f"{binary.name}_disabled.jsonl", "disabled")
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertEqual(journal.read_bytes(), original)

                # Reusing the result filename must reserve fresh artifact paths.
                process, repeated = self.run_benchmark(binary, first_output)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertNotEqual(repeated[0]["journal_path"], str(journal))
                self.assertEqual(journal.read_bytes(), original)
                self.assertEqual(stats.read_bytes(), original_stats)
                if original_histogram is not None:
                    self.assertEqual(histogram.read_bytes(), original_histogram)

    def test_invalid_runs_fail_after_writing_results(self):
        self.data.write_bytes(self.data.read_bytes()[:-1])
        for binary in self.binaries:
            with self.subTest(binary=binary.name):
                process, results = self.run_benchmark(
                    binary, self.directory / f"{binary.name}_invalid.jsonl", runs=2)
                self.assertNotEqual(process.returncode, 0)
                self.assertEqual(len(results), 2)
                for result in results:
                    self.assertFalse(result["valid"])
                    self.assertFalse(result["input_complete"])
                    self.assertTrue(result["journal_retained"])
                    self.assertTrue(Path(result["journal_path"]).exists())

    def test_report_is_saved_when_executable_reports_invalid_run(self):
        self.data.write_bytes(self.data.read_bytes()[:-1])
        process = subprocess.run([
            sys.executable, str(ROOT / "scripts/run_benchmark.py"),
            "--name", "invalid", "--binary", str(self.binaries[0]),
            "--output-dir", str(self.directory / "reports"), "--",
            "1", str(self.data), "--config", str(self.configs["on_stop"]),
        ], capture_output=True, text=True, timeout=30)
        self.assertNotEqual(process.returncode, 0)
        reports = list((self.directory / "reports").glob("invalid/*/report.txt"))
        self.assertEqual(len(reports), 1)
        report = reports[0].read_text()
        self.assertIn("Valid runs: 0/1", report)
        self.assertIn("No valid runs; performance statistics unavailable.", report)
        self.assertIn("Invalid run 1:", report)


if __name__ == "__main__":
    unittest.main()
