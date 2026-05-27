import subprocess
from pathlib import Path


def find_project_root(
        start_path: Path
) -> Path:
    current = start_path.resolve()

    for directory in [current, *current.parents]:
        if (
                (directory / "CMakePresets.json").exists()
                or
                (directory / ".git").exists()
        ):
            return directory

    raise RuntimeError(
        "Failed to locate project root"
    )


def resolve_project_path(
        project_dir: Path,
        path: str
) -> Path:
    candidate = Path(path)

    if candidate.is_absolute():
        return candidate.resolve()

    return (
            project_dir /
            candidate
    ).resolve()


def get_build_directory(
        project_dir: Path,
        preset: str
) -> Path:
    return (
            project_dir /
            f"cmake-build-{preset}"
    ).resolve()


def run_live_output(
        command: list[str],
        output_path: Path | None = None,
        environment: dict[str, str] | None = None
) -> None:
    print(f"Running: {' '.join(command)}\n")

    output_file = None

    if output_path is not None:
        output_path.parent.mkdir(
            parents=True,
            exist_ok=True
        )

        output_file = open(
            output_path,
            "w"
        )

    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=environment
        )

        assert process.stdout is not None

        for line in process.stdout:
            print(line, end="")

            if output_file is not None:
                output_file.write(line)

        process.wait()

        if process.returncode != 0:
            raise subprocess.CalledProcessError(
                process.returncode,
                command
            )

    finally:
        if output_file is not None:
            output_file.close()
