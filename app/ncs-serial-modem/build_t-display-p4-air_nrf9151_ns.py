"""Build Serial Modem for the T-Display-P4-Air nRF9151 hardware."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


EXPECTED_MANIFEST = "ncs-serial-modem/ncs-serial-modem"
BOARD_TARGET = "nrf9151dk/nrf9151/ns"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build ncs-serial-modem for T-Display-P4-Air nRF9151."
    )
    parser.add_argument(
        "--no-pristine",
        action="store_true",
        help="Reuse an existing build directory when possible.",
    )
    return parser.parse_args()


def require_path(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"Required path does not exist: {path}")


def main() -> int:
    args = parse_args()

    board_root = Path(__file__).resolve().parent
    workspace = board_root.parent
    component = board_root / "ncs-serial-modem"
    app = component / "app"
    overlay = board_root / "t-display-p4-air_nrf9151_ns.overlay"
    config = board_root / "t-display-p4-air_nrf9151_ns.conf"
    build_dir = board_root / "build" / "t-display-p4-air_nrf9151_ns"

    if shutil.which("west") is None:
        raise RuntimeError(
            "west is not available. Open a terminal from the nRF Connect for "
            "VS Code toolchain environment."
        )

    for required_path in (app, overlay, config):
        require_path(required_path)

    manifest_result = subprocess.run(
        ["west", "config", "manifest.path"],
        cwd=workspace,
        check=True,
        capture_output=True,
        text=True,
    )
    manifest_path = manifest_result.stdout.strip().replace("\\", "/")
    if manifest_path != EXPECTED_MANIFEST:
        raise RuntimeError(
            f"West manifest.path is {manifest_path!r}; expected "
            f"{EXPECTED_MANIFEST!r}. See README.md."
        )

    pristine = "auto" if args.no_pristine else "always"
    command = [
        "west",
        "build",
        "-p",
        pristine,
        "-b",
        BOARD_TARGET,
        "-d",
        str(build_dir),
        str(app),
        "--",
        f"-DEXTRA_DTC_OVERLAY_FILE={overlay}",
        f"-DEXTRA_CONF_FILE={config}",
    ]

    subprocess.run(command, cwd=workspace, check=True)
    print(f"Build completed: {build_dir}")
    print(f'Flash with: west flash -r jlink -d "{build_dir}"')
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
