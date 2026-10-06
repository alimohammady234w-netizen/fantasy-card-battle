"""Resource locations work from source and PyInstaller, independent of cwd."""
import json
import logging
import os
import sys
from pathlib import Path

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[2]))
USER_ROOT = (Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'ArcanaCardGame'
             if getattr(sys, 'frozen', False) else ROOT)
ATTRIBUTES = ('power', 'speed', 'height', 'defense', 'intelligence', 'stamina', 'luck', 'age')


def read_json(path, default):
    try:
        with Path(path).open(encoding='utf-8') as handle:
            return json.load(handle)
    except (OSError, ValueError) as error:
        logging.warning('Cannot load %s: %s', path, error)
        return default


def setup_logging():
    try:
        USER_ROOT.mkdir(parents=True, exist_ok=True)
        logging.basicConfig(filename=USER_ROOT / 'game.log', level=logging.INFO,
                            format='%(asctime)s %(levelname)s %(message)s')
    except OSError as error:
        logging.basicConfig(level=logging.WARNING)
        logging.warning('File logging unavailable; using stderr: %s', error)
