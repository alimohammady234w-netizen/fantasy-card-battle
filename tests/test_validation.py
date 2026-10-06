import copy
import json
import math
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from src.game_manager import GameManager
from src.systems.data_validation import DataCatalog, DEFAULT_SETTINGS, validate_settings
from src.systems.resources import ROOT, read_json, setup_logging
from tools.validate_data import validate


class ValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT/'data', self.root/'data')
        shutil.copy(ROOT/'config.json', self.root/'config.json')

    def tearDown(self):
        self.temp.cleanup()

    def modify(self, relative, values):
        path = self.root/relative
        data = json.loads(path.read_text())
        data.update(values)
        path.write_text(json.dumps(data))
        return DataCatalog(self.root)

    def test_shipped_catalog_valid(self):
        catalog, database = validate(ROOT)
        self.assertEqual(catalog.issues, [])
        self.assertEqual(len(database.cards), 45)

    def test_zero_divisors_and_negative_prices_recover(self):
        catalog = self.modify('config.json', {'combo_xp_every': 0, 'combo_coins_every': -2,
                              'player_level_xp': 0, 'upgrade_cost': -100})
        self.assertEqual(catalog.config['combo_xp_every'], 3)
        self.assertEqual(catalog.config['combo_coins_every'], 5)
        self.assertEqual(catalog.config['player_level_xp'], 250)
        self.assertEqual(catalog.config['upgrade_cost'], 100)
        self.assertEqual(len(catalog.issues), 4)

    def test_nonfinite_and_wrong_numeric_types(self):
        for bad in (None, True, '10', [], {}, float('nan'), float('inf'), 10**1000):
            with self.subTest(value=type(bad).__name__):
                catalog = self.modify('config.json', {'win_xp': bad})
                self.assertEqual(catalog.config['win_xp'], 50)
                self.assertTrue(catalog.issues)

    def test_threshold_list_validation(self):
        for bad in ([], [0], [100, 50], [100, 100], [None], [[10]], '100'):
            catalog = self.modify('config.json', {'card_xp_thresholds': bad})
            self.assertEqual(catalog.config['card_xp_thresholds'], [100, 250, 500])

    def test_modes_invalid_shape_and_flags(self):
        for bad in (None, [], {'Quick Match': {'rounds': -1}},
                    {'Classic Match': {'rounds': 30, 'ai_turns': 'yes', 'rewards': False}}):
            catalog = self.modify('config.json', {'modes': bad})
            self.assertEqual(len(catalog.config['modes']), 3)
            self.assertEqual(catalog.config['modes']['Classic Match']['rounds'], 30)
        catalog = self.modify('config.json', {'modes': {'Experimental': {}}})
        self.assertNotIn('Experimental', catalog.config['modes'])
        self.assertTrue(catalog.issues)

    def test_deck_and_tie_contracts(self):
        catalog = self.modify('config.json', {'deck_size': 0, 'tie_rule': ['random']})
        self.assertEqual(catalog.config['deck_size'], 30)
        self.assertEqual(catalog.config['tie_rule'], 'carry')

    def test_valid_authoring_values_preserved(self):
        catalog = self.modify('config.json', {'win_xp': 80, 'upgrade_cost': 120,
                                             'card_xp_thresholds': [50, 120, 600]})
        self.assertEqual(catalog.issues, [])
        self.assertEqual(catalog.config['win_xp'], 80)
        self.assertEqual(catalog.config['card_xp_thresholds'], [50, 120, 600])

    def test_invalid_rarity_color_and_border(self):
        catalog = self.modify('data/rarities.json', {'Epic': {'color': [-1, 300], 'border': 500,
                                                            'weight': float('nan'), 'glow': 'lots'}})
        row = catalog.rarities['Epic']
        self.assertEqual(row['color'], [147, 160, 179])
        self.assertEqual(row['border'], 2)
        self.assertEqual(row['weight'], 0)
        self.assertEqual(row['glow'], 15)

    def test_missing_all_files_has_safe_recovery(self):
        root = self.root/'empty'
        root.mkdir()
        catalog = DataCatalog(root)
        self.assertEqual(len(catalog.rarities), 6)
        self.assertEqual(catalog.packs, {})
        self.assertEqual(catalog.abilities, {})
        self.assertGreaterEqual(len(catalog.issues), 5)

    def test_malformed_json_root_types(self):
        for path in ('config.json', 'data/rarities.json', 'data/abilities.json', 'data/packs.json'):
            (self.root/path).write_text('[null]')
        catalog = DataCatalog(self.root)
        self.assertTrue(catalog.issues)
        self.assertEqual(catalog.config['win_xp'], 50)

    def test_ability_field_recovery(self):
        catalog = self.modify('data/abilities.json', {
            'Poison': {'effect': 'poison', 'duration': -100, 'amount': 'bad', 'description': []},
            'Critical Strike': {'effect': 'critical', 'chance': 2}, 'Broken': None})
        self.assertEqual(catalog.abilities['Poison']['duration'], 1)
        self.assertEqual(catalog.abilities['Poison']['amount'], 0)
        self.assertEqual(catalog.abilities['Poison']['description'], '')
        self.assertEqual(catalog.abilities['Critical Strike']['chance'], 0)
        self.assertNotIn('Broken', catalog.abilities)

    def test_custom_effect_fields_are_preserved(self):
        catalog = self.modify('data/abilities.json', {'New': {'effect': 'custom_effect', 'custom_scale': 3}})
        self.assertEqual(catalog.abilities['New']['custom_scale'], 3)
        checked, _ = validate(self.root)
        self.assertTrue(any('handler registration' in issue for issue in checked.issues))

    def test_invalid_packs_disabled_not_repriced(self):
        for changes in ({'price': -10}, {'count': 0}, {'count': 11}, {'currency': 'xp'},
                        {'weights': {'Common': -1}}, {'weights': {'Common': 0}},
                        {'weights': {'Unknown': 1}}, {'weights': [1, 2]},
                        {'weights': {'Common': float('nan')}}):
            row = {'price': 30, 'count': 3, 'currency': 'gems', 'weights': {'Common': 1}, **changes}
            catalog = self.modify('data/packs.json', {'Bronze': row})
            self.assertNotIn('Bronze', catalog.packs)
            self.assertIn('Gold', catalog.packs)

    def test_new_rarity_and_pack_are_supported(self):
        self.modify('data/rarities.json', {'Astral': {'color': [12, 80, 230], 'weight': 3, 'border': 4, 'glow': 60}})
        catalog = self.modify('data/packs.json', {'Astral': {'price': 10, 'count': 4, 'weights': {'Astral': 1}}})
        self.assertIn('Astral', catalog.packs)
        self.assertEqual(catalog.issues, [])
        catalog, _ = validate(self.root)
        self.assertTrue(any('no cards match' in issue for issue in catalog.issues))

    def test_bad_settings_default_without_crashing(self):
        for bad in (None, [], 'bad', {'fullscreen': 'false', 'timer': True, 'volume': float('inf'),
                                    'resolution': {}, 'animation_speed': []}):
            settings = validate_settings(bad, lambda message: None)
            self.assertEqual(settings, DEFAULT_SETTINGS)

    def test_invalid_cards_fail_authoring_check(self):
        path = self.root/'data/cards.json'
        rows = json.loads(path.read_text())
        rows.append({'id': 999, 'power': -1})
        path.write_text(json.dumps(rows))
        catalog, db = validate(self.root)
        self.assertEqual(len(db.cards), 45)
        self.assertTrue(any('rows were skipped' in issue for issue in catalog.issues))

    def test_cli_exit_codes_and_json(self):
        command = [sys.executable, str(ROOT/'tools/validate_data.py'), '--root', str(self.root), '--json']
        completed = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0)
        self.assertTrue(json.loads(completed.stdout)['ok'])
        self.modify('config.json', {'upgrade_cost': -1})
        completed = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(completed.returncode, 1)
        self.assertFalse(json.loads(completed.stdout)['ok'])

    def test_corrupt_rules_still_allow_full_match(self):
        self.modify('config.json', {'combo_xp_every': 0, 'player_level_xp': 0, 'card_xp_thresholds': []})
        def reader(path, default):
            return read_json(self.root / Path(path).relative_to(ROOT), default)
        with patch('src.game_manager.read_json', side_effect=reader):
            game = GameManager(self.root/'profile.json')
        battle = game.start('Quick Match')
        while not battle.finished:
            battle.resolve('power', 'Greater')
            battle.next_round()
        self.assertTrue(game.save())
        self.assertEqual(game.profile['statistics']['matches_played'], 1)

    def test_log_permission_error_is_nonfatal(self):
        with patch('src.systems.resources.USER_ROOT') as root:
            root.mkdir.side_effect = PermissionError('read-only folder')
            setup_logging()


if __name__ == '__main__':
    unittest.main()
