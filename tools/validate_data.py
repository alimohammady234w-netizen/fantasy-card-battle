"""Data-only command line checker; exits nonzero for invalid authoring data."""
import argparse
import json
import logging
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from src.cards.card_database import CardDatabase
from src.systems.data_validation import DataCatalog
from src.systems.resources import read_json


def validate(root):
    catalog = DataCatalog(Path(root))
    database = CardDatabase(Path(root) / 'data/cards.json')
    catalog.check_cards(database)
    rows = read_json(Path(root) / 'data/cards.json', None)
    if not isinstance(rows, list):
        catalog.report('cards.json: expected a JSON array')
    elif len(rows) != len(database.cards):
        catalog.report('cards.json: one or more invalid or duplicate rows were skipped')
    # Check registered built-in effect names without importing Pygame.
    from src.abilities.ability_manager import AbilityManager
    effects = AbilityManager(catalog.abilities).handlers
    for name, definition in catalog.abilities.items():
        if definition['effect'] not in effects:
            catalog.report(f'abilities.{name}: custom effect requires handler registration: {definition["effect"]}')
    return catalog, database


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT, help='Project root to validate')
    parser.add_argument('--json', action='store_true', help='Machine-readable report on stdout')
    args = parser.parse_args()
    logging.basicConfig(level=logging.ERROR)
    catalog, database = validate(args.root)
    report = {'ok': not catalog.issues, 'cards': len(database.cards),
              'categories': len(database.categories), 'rarities': len(catalog.rarities),
              'abilities': len(catalog.abilities), 'packs': len(catalog.packs),
              'achievements': len(catalog.achievements),
              'issues': catalog.issues}
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"{'PASS' if report['ok'] else 'FAIL'}: {report['cards']} cards, "
              f"{report['abilities']} abilities, {report['packs']} packs, {report['achievements']} achievements")
        for issue in catalog.issues:
            print(' - ' + issue)
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
