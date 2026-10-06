#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ -n "${VIRTUAL_ENV:-}" ]; then
  PYTHON="$VIRTUAL_ENV/bin/python"
else
  PYTHON="$PWD/venv/bin/python"
fi
if [ ! -x "$PYTHON" ]; then
  echo 'Install Python 3.12+, then run:' >&2
  echo 'python3.12 -m venv venv' >&2
  echo 'venv/bin/python -m pip install -r requirements-build.txt' >&2
  exit 1
fi
exec "$PYTHON" tools/build_release.py "$@"
