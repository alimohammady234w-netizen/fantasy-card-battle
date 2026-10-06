import os
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import pygame

from src.game import Game
from src.game_manager import GameManager
from src.battle.checkpoint import CheckpointCodec
from src.progression.achievement_system import METRICS, sanitize_claims
from src.systems.data_validation import DataCatalog
from src.systems.resources import ROOT, read_json
from tools.validate_data import validate


def complete(manager, mode='Quick Match'):
    battle = manager.start(mode)
    while not battle.finished:
        battle.resolve('speed', 'Less')
        battle.next_round()
    manager.save()
    return battle


class AchievementTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.path = self.root/'save.json'
        self.manager = GameManager(self.path)
        self.service = self.manager.achievements

    def tearDown(self):
        self.temp.cleanup()

    def ready(self, key='first_steps'):
        definition = self.service.definitions[key]
        self.manager.profile['statistics'][definition['metric']] = definition['target']

    def test_shipped_achievements_are_valid_and_initially_locked(self):
        catalog, _ = validate(ROOT)
        self.assertEqual(catalog.issues, [])
        self.assertEqual(len(self.service.definitions), 15)
        self.assertEqual(self.service.ready_count, 0)
        self.assertEqual(self.service.claimed_count, 0)
        for key, definition in self.service.definitions.items():
            self.assertIn(definition['metric'], METRICS)
            self.assertEqual(self.service.progress(key).status, 'LOCKED')

    def test_completed_match_unlocks_without_automatic_payment(self):
        complete(self.manager)
        before = copy.deepcopy(self.manager.profile)
        self.assertTrue(self.service.progress('first_steps').ready)
        self.assertGreater(self.service.ready_count, 0)
        self.assertEqual(before, self.manager.profile)
        self.assertEqual(self.manager.profile['claimed_achievements'], [])

    def test_claim_pays_once_and_survives_reload(self):
        self.ready()
        p = self.manager.profile
        coins, gems = p['coins'], p['gems']
        reward = self.manager.claim_achievement('first_steps')
        self.assertEqual(p['coins'], coins+reward['coins'])
        self.assertEqual(p['gems'], gems+reward['gems'])
        self.assertEqual(p['statistics']['total_coins_earned'], reward['coins'])
        self.assertEqual(self.service.progress('first_steps').status, 'CLAIMED')
        reloaded = GameManager(self.path)
        before = copy.deepcopy(reloaded.profile)
        with self.assertRaisesRegex(ValueError, 'already been claimed'):
            reloaded.claim_achievement('first_steps')
        self.assertEqual(before, reloaded.profile)

    def test_locked_and_unknown_claims_are_rejected(self):
        before = copy.deepcopy(self.manager.profile)
        for key in ('first_steps', 'unknown'):
            with self.assertRaises(ValueError):
                self.manager.claim_achievement(key)
        self.assertEqual(before, self.manager.profile)

    def test_double_claim_in_same_frame_is_rejected(self):
        self.ready()
        self.manager.claim_achievement('first_steps')
        before = copy.deepcopy(self.manager.profile)
        with self.assertRaises(ValueError):
            self.manager.claim_achievement('first_steps')
        self.assertEqual(before, self.manager.profile)

    def test_atomic_write_failure_rolls_back_entire_claim(self):
        self.ready()
        self.manager.save()
        before = copy.deepcopy(self.manager.profile)
        disk = self.path.read_bytes()
        with patch('pathlib.Path.replace', side_effect=PermissionError('locked file')):
            with self.assertRaisesRegex(ValueError, 'save failed'):
                self.manager.claim_achievement('first_steps')
        self.assertEqual(before, self.manager.profile)
        self.assertEqual(disk, self.path.read_bytes())
        self.assertTrue(self.service.progress('first_steps').ready)
        self.manager.claim_achievement('first_steps')
        self.assertEqual(self.service.claimed_count, 1)

    def test_claim_preserves_shared_profile_references(self):
        self.ready()
        settings = self.manager.profile['settings']
        stats = self.manager.profile['statistics']
        ledger = self.manager.profile['claimed_achievements']
        self.manager.claim_achievement('first_steps')
        self.assertIs(settings, self.manager.profile['settings'])
        self.assertIs(stats, self.manager.profile['statistics'])
        self.assertIs(ledger, self.manager.profile['claimed_achievements'])
        self.assertIs(self.manager.progression.profile, self.service.profile)
        self.assertIs(self.manager.decks.profile, self.service.profile)

    def test_claim_and_checkpoint_committed_together(self):
        self.ready()
        battle = self.manager.start('Classic Match')
        battle.resolve('power', 'Greater')
        snapshot = CheckpointCodec.encode(battle)
        rewards = self.manager.claim_achievement('first_steps')
        loaded = GameManager(self.path)
        self.assertTrue(loaded.can_resume)
        self.assertEqual(snapshot, CheckpointCodec.encode(loaded.resume()))
        self.assertEqual(loaded.profile['coins'], self.manager.profile['coins'])
        self.assertTrue(loaded.achievements.progress('first_steps').claimed)
        loaded.battle.next_round()
        self.assertEqual(loaded.profile['coins'], self.manager.profile['coins'])

    def test_old_save_progress_is_recognized_retroactively(self):
        profile = copy.deepcopy(self.manager.profile)
        profile.pop('claimed_achievements')
        profile['statistics']['matches_played'] = 10
        self.path.write_text(json.dumps(profile))
        loaded = GameManager(self.path)
        self.assertTrue(loaded.achievements.progress('arena_regular').ready)
        self.assertTrue(loaded.achievements.progress('first_steps').ready)
        self.assertEqual(loaded.profile['coins'], profile['coins'])
        self.assertEqual(loaded.profile['claimed_achievements'], [])

    def test_claim_ledger_validation_retains_unknown_valid_ids(self):
        raw = ['first_steps', 'first_steps', None, [], 'future_achievement', 'bad/key', '', True]
        self.assertEqual(sanitize_claims(raw), ['first_steps', 'future_achievement'])
        self.manager.profile['claimed_achievements'] = raw
        self.manager.save()
        loaded = GameManager(self.path)
        self.assertEqual(loaded.profile['claimed_achievements'], ['first_steps', 'future_achievement'])
        for bad in (None, {}, 'first_steps'):
            self.assertEqual(sanitize_claims(bad), [])

    def test_bad_ledger_does_not_reset_currency(self):
        self.manager.profile['claimed_achievements'] = {'malformed': True}
        self.manager.profile['coins'] = 9876
        self.manager.save()
        loaded = GameManager(self.path)
        self.assertEqual(loaded.profile['coins'], 9876)
        self.assertEqual(loaded.profile['claimed_achievements'], [])

    def test_definition_removal_and_return_cannot_repay(self):
        self.ready()
        self.manager.claim_achievement('first_steps')
        row = self.service.definitions.pop('first_steps')
        self.manager.save()
        self.assertIn('first_steps', self.manager.saves.load()['claimed_achievements'])
        row['rewards']['coins'] += 1000
        self.service.definitions['first_steps'] = row
        with self.assertRaises(ValueError):
            self.manager.claim_achievement('first_steps')

    def test_achievement_edits_do_not_invalidate_battle_checkpoint(self):
        battle = self.manager.start('Quick Match')
        before = CheckpointCodec.encode(battle)
        self.service.definitions['first_steps']['target'] = 2
        self.manager.save()
        reloaded = GameManager(self.path)
        self.assertEqual(before, CheckpointCodec.encode(reloaded.resume()))

    def test_practice_does_not_unlock_or_reward(self):
        before = copy.deepcopy(self.manager.profile)
        complete(self.manager, 'Practice')
        self.assertEqual(self.manager.profile, before)
        self.assertEqual(self.service.ready_count, 0)

    def test_stat_metric_boundaries(self):
        for key, row in self.service.definitions.items():
            if row['metric'] in self.manager.profile['statistics']:
                self.manager.profile['statistics'][row['metric']] = row['target']-1
                self.assertFalse(self.service.progress(key).ready)
                self.manager.profile['statistics'][row['metric']] = row['target']
                self.assertTrue(self.service.progress(key).ready)
        self.manager.profile['statistics']['best_combo'] = 100
        self.assertEqual(self.service.progress('threefold').ratio, 1.)

    def test_level_collection_and_upgrade_metrics(self):
        self.manager.profile['level'] = 3
        self.assertTrue(self.service.progress('growing_power').ready)
        self.manager.progression.upgrade(self.manager.database[1], 'power')
        self.assertTrue(self.service.progress('forge_starter').ready)
        self.manager.progression.card_state(1)['level'] = 3
        self.assertTrue(self.service.progress('masterwork').ready)
        for card_id in range(31, 36):
            self.manager.profile['owned_cards'][str(card_id)] = 1
        self.assertTrue(self.service.progress('curator').ready)
        self.assertFalse(self.service.progress('realm_collector').ready)

    def test_opening_packs_updates_progress(self):
        for _ in range(3):
            self.manager.packs.open('Starter')
        self.assertTrue(self.service.progress('vault_explorer').ready)
        self.assertEqual(self.service.metric('total_packs_opened'), 3)

    def test_metric_queries_never_create_card_progress(self):
        before = copy.deepcopy(self.manager.profile)
        for metric in METRICS:
            self.assertGreaterEqual(self.service.metric(metric), 0)
        for key in self.service.definitions:
            self.service.progress(key)
        self.assertEqual(before, self.manager.profile)
        with self.assertRaises(ValueError):
            self.service.metric('unsupported')

    def test_invalid_definitions_disabled(self):
        catalog = DataCatalog()
        good = copy.deepcopy(catalog.achievements['first_steps'])
        mutations = ({'metric': 'hidden_ai_card'}, {'metric': []}, {'target': 0}, {'target': True},
                     {'target': float('inf')}, {'rewards': {'coins': -1}}, {'rewards': {'xp': 10}},
                     {'rewards': {'gems': float('nan')}}, {'rewards': {'coins': 0}},
                     {'rewards': []}, {'title': None}, {'description': 'x'*161}, {'group': ''})
        for change in mutations:
            result = catalog.achievement_rules({'test': {**good, **change}})
            self.assertEqual(result, {}, change)
        self.assertEqual(catalog.achievement_rules({'bad/id': good, 'empty': None}), {})

    def test_missing_definitions_do_not_block_matches(self):
        def reader(path, default):
            return None if Path(path).name == 'achievements.json' else read_json(path, default)
        with patch('src.game_manager.read_json', side_effect=reader):
            manager = GameManager(self.root/'missing-achievements-save.json')
        self.assertEqual(manager.achievements.definitions, {})
        complete(manager)
        self.assertEqual(manager.profile['statistics']['matches_played'], 1)
        with self.assertRaises(ValueError):
            manager.claim_achievement('first_steps')

    def test_optional_single_currency_and_new_group(self):
        catalog = DataCatalog()
        row = copy.deepcopy(catalog.achievements['first_steps'])
        row['group'], row['rewards'] = 'Exploration', {'gems': 17}
        definition = catalog.achievement_rules({'explorer': row})
        self.assertEqual(definition['explorer']['rewards'], {'coins': 0, 'gems': 17})
        self.service.definitions.update(definition)
        self.ready('explorer')
        coins = self.manager.profile['coins']
        self.manager.claim_achievement('explorer')
        self.assertEqual(self.manager.profile['coins'], coins)

    def test_missing_achievement_json_disables_only_achievements(self):
        shutil.copytree(ROOT/'data', self.root/'data')
        shutil.copy(ROOT/'config.json', self.root/'config.json')
        (self.root/'data/achievements.json').unlink()
        catalog, database = validate(self.root)
        self.assertEqual(catalog.achievements, {})
        self.assertEqual(len(database.cards), 45)
        self.assertTrue(any('achievements.json' in issue for issue in catalog.issues))


class AchievementUITests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)/'profile.json'
        self.app = Game(self.path)

    def tearDown(self):
        pygame.quit()
        self.temp.cleanup()

    def click(self, x, y):
        self.app.draw()
        pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1, pos=(x, y)))
        self.app.events()
        self.app.draw()

    def test_menu_and_settings_navigation(self):
        self.click(1040, 92)
        self.assertEqual(self.app.route, 'achievements')
        self.app.navigate('settings')
        self.click(1090, 157)
        self.assertEqual(self.app.route, 'achievements')

    def test_locked_button_does_not_claim(self):
        before = copy.deepcopy(self.app.manager.profile)
        self.app.navigate('achievements')
        self.click(342, 410)
        self.assertEqual(before, self.app.manager.profile)

    def test_claim_mouse_event_persists(self):
        self.app.manager.profile['statistics']['matches_played'] = 1
        coins = self.app.manager.profile['coins']
        self.app.navigate('achievements')
        self.click(342, 410)
        self.assertEqual(self.app.manager.profile['coins'], coins+150)
        self.assertIn('first_steps', self.app.manager.saves.load()['claimed_achievements'])
        self.click(342, 410)
        self.assertEqual(self.app.manager.profile['coins'], coins+150)

    def test_failed_claim_is_retryable_in_ui(self):
        self.app.manager.profile['statistics']['matches_played'] = 1
        before = self.app.manager.profile['coins']
        self.app.navigate('achievements')
        with patch('pathlib.Path.replace', side_effect=PermissionError('locked')):
            self.click(342, 410)
        self.assertIn('save failed', self.app.toast)
        self.assertEqual(self.app.manager.profile['coins'], before)
        self.assertTrue(self.app.manager.achievements.progress('first_steps').ready)
        self.click(342, 410)
        self.assertEqual(self.app.manager.profile['coins'], before+150)

    def test_filters_and_pagination(self):
        self.app.navigate('achievements')
        self.click(1210, 670)
        self.assertEqual(self.app.screen.page, 1)
        self.click(1210, 670)
        self.assertEqual(self.app.screen.page, 2)
        self.click(180, 224)
        self.assertEqual(self.app.screen.status_filter, 'READY')
        self.assertEqual(self.app.screen.page, 0)
        self.assertEqual(self.app.screen.items(), [])
        self.app.manager.profile['statistics']['matches_played'] = 1
        self.app.draw()
        self.assertEqual(len(self.app.screen.items()), 1)
        self.click(421, 224)
        self.assertEqual(self.app.screen.group, 'Arena')

    def test_read_only_empty_and_missing_definitions(self):
        self.app.navigate('achievements')
        before = copy.deepcopy(self.app.manager.profile)
        self.app.draw()
        self.app.screen.cycle_filter()
        self.app.draw()
        self.assertEqual(before, self.app.manager.profile)
        self.app.manager.achievements.definitions.clear()
        self.app.navigate('achievements')
        self.app.draw()
        self.assertEqual(self.app.screen.items(), [])

    def test_all_pages_both_resolutions_and_claimed_state(self):
        self.app.manager.profile['statistics']['matches_played'] = 1
        self.app.manager.claim_achievement('first_steps')
        for size in ([1280, 720], [1920, 1080]):
            self.app.manager.profile['settings']['resolution'] = size
            self.app.set_display()
            self.app.navigate('achievements')
            for page in range(3):
                self.app.screen.page = page
                self.app.draw()
            self.app.screen.status_filter = 'CLAIMED'
            self.app.draw()
            self.assertEqual(len(self.app.screen.items()), 1)
            self.assertEqual(self.app.window.get_size(), tuple(size))

    def test_claim_preserves_live_sound_settings_reference(self):
        self.app.manager.profile['statistics']['matches_played'] = 1
        self.app.manager.claim_achievement('first_steps')
        self.assertIs(self.app.sound.settings, self.app.manager.profile['settings'])
        self.app.navigate('settings')
        self.app.screen.change('volume')
        self.assertEqual(self.app.sound.settings['volume'], self.app.manager.profile['settings']['volume'])


if __name__ == '__main__':
    unittest.main()
