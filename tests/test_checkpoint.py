import os
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import copy
import json
import tempfile
import unittest
from dataclasses import asdict
from pathlib import Path
from unittest.mock import patch
import pygame
from src.battle.checkpoint import CheckpointCodec
from src.game_manager import GameManager
from src.game import Game
from src.systems.resources import ATTRIBUTES


class CheckpointTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)/'save.json'
        self.manager = GameManager(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def start(self, mode='Quick Match'):
        return self.manager.start(mode)

    def reload(self):
        self.assertTrue(self.manager.save())
        return GameManager(self.path)

    def decode(self, data):
        m = self.manager
        return CheckpointCodec.decode(data, m.database, m.config, m.ability_data, m.progression)

    def test_fresh_match_roundtrip(self):
        battle = self.start()
        snapshot = CheckpointCodec.encode(battle)
        restored = self.reload()
        self.assertTrue(restored.can_resume)
        self.assertEqual(snapshot, CheckpointCodec.encode(restored.resume()))
        self.assertIsNone(restored.battle.result)

    def test_resolved_round_does_not_pay_twice(self):
        battle = self.start()
        battle.resolve('power', 'Greater')
        restored = self.reload()
        before = copy.deepcopy(restored.profile)
        self.assertEqual(asdict(battle.result), asdict(restored.resume().result))
        with self.assertRaises(ValueError):
            restored.battle.resolve('speed', 'Less')
        restored.save()
        self.assertEqual(restored.profile, before)
        restored.battle.next_round()
        self.assertEqual(restored.profile['coins'], before['coins'])
        self.assertEqual(restored.profile['statistics']['total_xp'], before['statistics']['total_xp'])

    def test_deterministic_continuation_all_modes_and_difficulties(self):
        for difficulty in ('Easy', 'Normal', 'Hard', 'Expert'):
            for mode in ('Quick Match', 'Classic Match', 'Practice'):
                with self.subTest(mode=mode, difficulty=difficulty):
                    self.manager.profile['settings']['difficulty'] = difficulty
                    original = self.start(mode)
                    for _ in range(3):
                        original.resolve('speed', 'Less')
                        original.next_round()
                    restored_manager = self.reload()
                    restored = restored_manager.resume()
                    while not original.finished:
                        attr = ATTRIBUTES[original.index % len(ATTRIBUTES)]
                        direction = 'Greater' if original.index % 2 else 'Less'
                        self.assertEqual(asdict(original.resolve(attr, direction)),
                                         asdict(restored.resolve(attr, direction)))
                        self.assertEqual(original.states, restored.states)
                        self.assertEqual(original.rng.getstate(), restored.rng.getstate())
                        original.next_round()
                        restored.next_round()
                    for key in ('coins', 'gems', 'level', 'xp', 'statistics', 'card_progress'):
                        self.assertEqual(self.manager.profile[key], restored_manager.profile[key])
                    self.assertEqual(original.scores, restored.scores)
                    self.manager.save()

    def test_pending_statuses_and_hp_survive(self):
        battle = self.start()
        battle.states[0]['hp'] = 72
        battle.states[0]['statuses'] = [{'amount': 4, 'turns': 2}, {'amount': 6, 'turns': 1}]
        restored = self.reload().resume()
        self.assertEqual(battle.states, restored.states)
        self.assertEqual(asdict(battle.resolve('power', 'Greater')),
                         asdict(restored.resolve('power', 'Greater')))

    def test_frozen_stats_ignore_later_upgrades(self):
        battle = self.start()
        card = battle.cards[0]
        values = battle.values()[0]
        state = self.manager.progression.card_state(card.id)
        state['level'] = 30
        state['upgrades']['power'] = 40
        self.assertEqual(battle.values()[0], values)
        restored = self.reload().resume()
        self.assertEqual(restored.values()[0], values)
        self.manager.start('Quick Match')
        self.assertEqual(self.manager.battle.player_values[str(card.id)], card.values(state))

    def test_changed_settings_do_not_change_saved_difficulty(self):
        battle = self.start()
        self.manager.profile['settings']['difficulty'] = 'Expert'
        restored = self.reload().resume()
        self.assertEqual(restored.ai.difficulty, battle.ai.difficulty)
        self.assertEqual(restored.ai.difficulty, 'Normal')

    def test_deck_editor_does_not_modify_pending_deck(self):
        battle = self.start()
        original = list(battle.player_deck)
        self.manager.decks.toggle('First Light', original[0])
        restored = self.reload().resume()
        self.assertEqual(restored.player_deck, original)
        self.assertFalse(self.manager.decks.validate(self.manager.decks.decks['First Light']))

    def test_checkpoint_cleared_after_finish(self):
        battle = self.start()
        while not battle.finished:
            battle.resolve('luck', 'Greater')
            battle.next_round()
        restored = self.reload()
        self.assertIsNone(restored.profile['active_match'])
        self.assertFalse(restored.can_resume)
        self.assertEqual(restored.profile['statistics']['matches_played'], 1)
        self.assertEqual(len(restored.profile['match_history']), 1)
        with self.assertRaises(ValueError):
            restored.resume()

    def test_final_resolved_round_resume_finalizes_once(self):
        battle = self.start()
        for i in range(battle.round_limit):
            battle.resolve('defense', 'Greater')
            if i < battle.round_limit-1:
                battle.next_round()
        restored = self.reload()
        self.assertEqual(restored.battle.index, 9)
        self.assertEqual(restored.profile['statistics']['matches_played'], 0)
        restored.battle.next_round()
        coins, gems = restored.profile['coins'], restored.profile['gems']
        restored.battle.finish()
        restored.save()
        self.assertEqual(restored.profile['coins'], coins)
        self.assertEqual(restored.profile['gems'], gems)
        self.assertFalse(GameManager(self.path).can_resume)

    def test_discard_keeps_earned_rewards(self):
        self.start().resolve('speed', 'Less')
        self.manager.save()
        before = copy.deepcopy(self.manager.profile)
        self.assertTrue(self.manager.discard_match())
        restored = GameManager(self.path)
        self.assertFalse(restored.can_resume)
        for key in ('coins', 'gems', 'xp', 'level', 'statistics'):
            self.assertEqual(restored.profile[key], before[key])
        self.assertEqual(restored.profile['statistics']['matches_played'], 0)

    def test_old_save_migrates_without_checkpoint(self):
        self.manager.profile.pop('active_match')
        self.manager.saves.save(self.manager.profile)
        restored = GameManager(self.path)
        self.assertIsNone(restored.profile['active_match'])
        self.assertFalse(restored.can_resume)

    def test_malformed_checkpoint_preserves_progression(self):
        for bad in ([], {}, 'bad', {'version': 100}, {'mode': []}):
            data = copy.deepcopy(self.manager.profile)
            data['coins'], data['active_match'] = 9876, bad
            self.path.write_text(json.dumps(data))
            restored = GameManager(self.path)
            self.assertEqual(restored.profile['coins'], 9876)
            self.assertIsNone(restored.profile['active_match'])
            self.assertTrue(restored.resume_warning)

    def test_changed_rules_reject_checkpoint(self):
        snapshot = CheckpointCodec.encode(self.start())
        self.manager.config['win_xp'] += 1
        with self.assertRaisesRegex(ValueError, 'rules changed'):
            self.decode(snapshot)

    def test_invalid_snapshot_fields_rejected(self):
        original = CheckpointCodec.encode(self.start())
        for key, bad in (('index', -1), ('index', 30), ('mode', []), ('id', 'not-an-id'),
                         ('difficulty', 'Cheater'), ('player_deck', [1]*30), ('ai_deck', [9000]*30),
                         ('rng', [3, [0]*4, None]), ('states', []), ('player_values', {}),
                         ('history', []), ('scores', [3, 0])):
            snapshot = copy.deepcopy(original)
            snapshot[key] = bad
            if key == 'history':
                snapshot['index'] = 5
            with self.assertRaises(ValueError, msg=key):
                self.decode(snapshot)

    def test_malformed_resolved_history_rejected(self):
        battle = self.start()
        battle.resolve('power', 'Greater')
        original = CheckpointCodec.encode(battle)
        for key, bad in (('raw', [0, 0]), ('strength', [float('nan'), 1]),
                         ('captured', 60), ('winner', True), ('round', 2), ('leveled', 1)):
            snapshot = copy.deepcopy(original)
            snapshot['history'][0][key] = bad
            with self.assertRaises(ValueError, msg=key):
                self.decode(snapshot)

    def test_completed_match_id_cannot_be_restored(self):
        snapshot = CheckpointCodec.encode(self.start())
        self.manager.profile['match_history'] = [{'id': snapshot['id']}]
        with self.assertRaisesRegex(ValueError, 'already completed'):
            self.decode(snapshot)

    def test_failed_save_retains_old_atomic_pair(self):
        battle = self.start()
        self.manager.save()
        old = self.path.read_bytes()
        battle.resolve('power', 'Greater')
        with patch('pathlib.Path.replace', side_effect=PermissionError('locked')):
            self.assertFalse(self.manager.save())
        self.assertEqual(self.path.read_bytes(), old)
        restored = GameManager(self.path)
        self.assertIsNone(restored.battle.result)
        self.assertEqual(restored.profile['statistics']['total_xp'], 0)


class CheckpointUITests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)/'save.json'
        self.app = Game(self.path)

    def tearDown(self):
        pygame.quit()
        self.temp.cleanup()

    def click(self, x, y):
        self.app.draw()
        pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1, pos=(x, y)))
        self.app.events()
        self.app.draw()

    def begin(self):
        self.app.navigate('play')
        self.app.screen.start()
        self.assertEqual(self.app.route, 'battle')

    def test_start_creates_initial_disk_checkpoint(self):
        self.begin()
        self.assertTrue(GameManager(self.path).can_resume)

    def test_suspend_menu_resume_flow(self):
        self.begin()
        match_id = self.app.manager.battle.match_id
        self.app.home()
        self.assertIsNotNone(self.app.modal)
        self.app.accept_modal()
        self.assertEqual(self.app.route, 'menu')
        self.app.navigate('play')
        self.click(200, 640)
        self.assertEqual(self.app.route, 'battle')
        self.assertEqual(self.app.manager.battle.match_id, match_id)

    def test_resolved_screen_restored_after_restart(self):
        self.begin()
        self.app.screen.attribute = 'speed'
        self.app.screen.choose('Less')
        self.app.screen.update(1)
        before = self.app.manager.profile['coins']
        self.app.quit()
        self.app = Game(self.path)
        self.app.resume_match()
        self.app.draw()
        self.assertIsNotNone(self.app.manager.battle.result)
        self.assertEqual(self.app.screen.attribute, 'speed')
        self.assertEqual(self.app.manager.profile['coins'], before)
        self.app.screen.next_round()
        self.assertEqual(self.app.manager.battle.index, 1)

    def test_new_match_requires_replace_confirmation(self):
        self.begin()
        match_id = self.app.manager.battle.match_id
        self.app.suspend_match()
        self.app.navigate('play')
        self.app.screen.start()
        self.assertIsNotNone(self.app.modal)
        self.assertEqual(self.app.manager.battle.match_id, match_id)
        self.app.modal = None  # Cancel leaves the old match available.
        self.assertTrue(self.app.manager.can_resume)
        self.app.screen.start()
        self.app.accept_modal()
        self.assertNotEqual(self.app.manager.battle.match_id, match_id)
        self.assertEqual(self.app.route, 'battle')

    def test_discard_button_confirmation(self):
        self.begin()
        self.app.suspend_match()
        self.app.navigate('play')
        self.click(453, 640)
        self.assertIsNotNone(self.app.modal)
        self.app.accept_modal()
        self.app.draw()
        self.assertFalse(self.app.manager.can_resume)
        self.assertFalse(GameManager(self.path).can_resume)

    def test_resume_before_reveal_keeps_enemy_hidden(self):
        self.begin()
        self.app.screen.elapsed = 10
        self.app.screen.attribute = 'power'
        self.app.screen.choose('Greater')
        self.app.screen.update(.1)
        self.app.quit()
        self.app = Game(self.path)
        self.app.resume_match()
        self.app.draw()
        self.assertIsNone(self.app.manager.battle.result)
        self.assertEqual(self.app.screen.elapsed, 0)
        self.assertIsNone(self.app.screen.attribute)

    def test_practice_checkpoint_no_rewards_or_journal(self):
        self.app.navigate('play')
        self.app.screen.mode = 'Practice'
        self.app.screen.start()
        coins = self.app.manager.profile['coins']
        self.app.quit()
        self.app = Game(self.path)
        self.app.resume_match()
        while self.app.route == 'battle':
            screen = self.app.screen
            screen.attribute = 'power'
            screen.choose('Greater')
            screen.update(1)
            screen.next_round()
        self.assertEqual(self.app.route, 'result')
        self.assertEqual(self.app.manager.profile['coins'], coins)
        self.assertEqual(self.app.manager.profile['match_history'], [])
        self.assertIsNone(self.app.manager.profile['active_match'])

    def test_resume_screen_both_resolutions(self):
        self.begin()
        self.app.suspend_match()
        for size in ([1280, 720], [1920, 1080]):
            self.app.manager.profile['settings']['resolution'] = size
            self.app.set_display()
            self.app.navigate('play')
            self.app.draw()
            self.assertTrue(self.app.manager.can_resume)


if __name__ == '__main__':
    unittest.main()
