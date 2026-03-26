#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ENV_NAME="${ENV_NAME:-speicher_wroover_8mb_uart}"
PORT="/dev/ttyACM0"

cd "$SCRIPT_DIR"

if [[ ! -d .venv ]]; then
    python3 -m venv .venv
fi

. .venv/bin/activate

if ! python -m platformio --version >/dev/null 2>&1; then
    python -m pip install --upgrade pip
    python -m pip install platformio
fi

exec pio run -e "$ENV_NAME" -t upload --upload-port "$PORT"
