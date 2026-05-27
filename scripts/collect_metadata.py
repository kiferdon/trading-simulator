import subprocess
from datetime import datetime


def collect_metadata(project_dir: str, build_dir: str) -> str:
    def run(cmd):
        return subprocess.run(cmd, capture_output=True, text=True).stdout.strip()

    git_commit = run(["git", "rev-parse", "HEAD"])
    git_status = run(["git", "status", "--short"])
    compiler_version = run(["g++", "--version"]).splitlines()[0]
    cpu_info = run(["bash", "-c", "lscpu | grep 'Model name\\|CPU\\|Thread\\|Core'"])

    return "\n".join([
        f"Date: {datetime.now().isoformat()}",
        f"Git commit: {git_commit}",
        "",
        "Git status:",
        git_status,
        "",
        f"Compiler: {compiler_version}",
        "",
        "CPU:",
        cpu_info,
        "",
        f"Build dir: {build_dir}",
    ])
