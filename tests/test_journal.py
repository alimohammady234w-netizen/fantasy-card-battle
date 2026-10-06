import os
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import copy
import json
import tempfile
import unittest
from pathlib import Path
import pygame
from src.game import Game
from src.game_manager import GameManager
from src.battle.match_journal import record_match, sanitize_journal, MAX_MATCHES


def complete(manager, mode='Quick Match'):
    battle = manager.start(mode)
    while not battle.finished:
        battle.resolve('speed', 'Less')
        battle.next_round()
    return battle


class JournalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)/'player.json'
        self.manager = GameManager(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def entry(self):
        complete(self.manager)
        return self.manager.profile['match_history'][-1]

    def test_completed_match_saved_with_round_details(self):
        battle = complete(self.manager)
        entry = self.manager.profile['match_history'][0]
        self.assertEqual(entry['id'], battle.match_id)
        self.assertEqual(entry['scores'], battle.scores)
        self.assertEqual(len(entry['rounds']), 10)
        self.assertEqual(entry['xp'], sum(r.xp for r in battle.history))
        self.assertEqual(entry['coins'], sum(r.coins for r in battle.history))
        for row, result in zip(entry['rounds'], battle.history):
            self.assertEqual(row['raw'], list(result.raw))
            self.assertEqual(row['strength'], list(result.strength))
            self.assertEqual(row['abilities'], result.abilities)
            self.assertTrue(row['player'])
            self.assertTrue(row['opponent'])

    def test_exact_json_round_trip(self):
        entry = self.entry()
        self.assertEqual(entry, json.loads(json.dumps(entry)))
        self.assertTrue(self.manager.save())
        self.assertEqual(self.manager.profile, self.manager.saves.load())
        reloaded = GameManager(self.path)
        self.assertEqual(reloaded.profile['match_history'], [entry])

    def test_classic_records_alternating_chooser(self):
        complete(self.manager, 'Classic Match')
        rows = self.manager.profile['match_history'][0]['rounds']
        self.assertEqual(len(rows), 30)
        self.assertEqual([r['chooser'] for r in rows], ['Player', 'AI']*15)

    def test_no_record_before_completion(self):
        battle = self.manager.start('Quick Match')
        battle.resolve('power', 'Greater')
        record_match(battle)
        self.manager.save()
        self.assertEqual(self.manager.saves.load()['match_history'], [])

    def test_practice_preserves_entire_profile(self):
        before = copy.deepcopy(self.manager.profile)
        complete(self.manager, 'Practice')
        self.assertEqual(before, self.manager.profile)

    def test_repeated_finish_and_record_are_idempotent(self):
        battle = complete(self.manager)
        before = copy.deepcopy(self.manager.profile)
        battle.finish()
        record_match(battle)
        self.assertEqual(before, self.manager.profile)

    def test_no_hidden_deck_data(self):
        battle = complete(self.manager)
        entry = self.manager.profile['match_history'][0]
        self.assertEqual(set(entry), {'id', 'completed_at', 'mode', 'difficulty', 'scores', 'rounds', 'xp', 'coins'})
        self.assertNotIn('ai_deck', json.dumps(entry))
        revealed = [self.manager.database[i].name for i in battle.ai_deck[:10]]
        self.assertEqual([r['opponent'] for r in entry['rounds']], revealed)
        self.assertEqual(len(entry['rounds']), battle.round_limit)

    def test_last_fifty_retained(self):
        entry = self.entry()
        entries = [{**copy.deepcopy(entry), 'id': str(i)} for i in range(55)]
        self.manager.profile['match_history'] = entries[:50]
        battle = complete(self.manager)
        actual = self.manager.profile['match_history']
        self.assertEqual(len(actual), MAX_MATCHES)
        self.assertEqual(actual[0]['id'], '1')
        self.assertEqual(actual[-1]['id'], battle.match_id)
        sanitized = sanitize_journal(entries)
        self.assertEqual(len(sanitized), 50)
        self.assertEqual(sanitized[0]['id'], '5')

    def test_old_saves_load_without_journal(self):
        old = copy.deepcopy(self.manager.profile)
        old.pop('match_history')
        old['coins'] = 3210
        self.path.write_text(json.dumps(old))
        loaded = self.manager.saves.load()
        self.assertEqual(loaded['coins'], 3210)
        self.assertEqual(loaded['match_history'], [])
        self.assertFalse(list(self.path.parent.glob('*.corrupt-*')))

    def test_bad_journal_never_resets_currencies(self):
        for bad in (None, {}, 'broken', [None, {}, []]):
            data = copy.deepcopy(self.manager.profile)
            data['coins'], data['gems'], data['match_history'] = 5432, 899, bad
            self.path.write_text(json.dumps(data))
            loaded = self.manager.saves.load()
            self.assertEqual((loaded['coins'], loaded['gems']), (5432, 899))
            self.assertEqual(loaded['match_history'], [])

    def test_invalid_entry_isolated_from_valid_neighbors(self):
        entry = self.entry()
        damaged = copy.deepcopy(entry)
        damaged['id'] = 'damaged'
        damaged['rounds'][0]['strength'][0] = float('nan')
        self.assertEqual(sanitize_journal([damaged, entry]), [entry])

    def test_reject_malformed_fields(self):
        entry = self.entry()
        for key, value in (('rounds', []), ('completed_at', 'yesterday'), ('completed_at', '2026-10-06'),
                           ('scores', [-1, 0]), ('scores', [0]), ('id', None)):
            damaged = copy.deepcopy(entry)
            damaged[key] = value
            self.assertEqual(sanitize_journal([damaged]), [], key)
        for key, value in (('raw', [101, 1]), ('raw', None), ('strength', [float('inf'), 1]),
                           ('round', True), ('winner', True), ('captured', -5),
                           ('attribute', 'mana'), ('chooser', 'unknown'), ('abilities', [None])):
            damaged = copy.deepcopy(entry)
            damaged['rounds'][0][key] = value
            self.assertEqual(sanitize_journal([damaged]), [], key)

    def test_duplicate_entries_removed_and_unknown_fields_stripped(self):
        entry = self.entry()
        altered = copy.deepcopy(entry)
        altered['hidden_deck'] = [9000]
        altered['rounds'][0]['hidden_information'] = 'not allowed'
        self.assertEqual(sanitize_journal([altered, entry]), [entry])

    def test_totals_recomputed_without_rewards(self):
        entry = self.entry()
        forged = copy.deepcopy(entry)
        forged['xp'] = forged['coins'] = 9999999
        before = copy.deepcopy(self.manager.profile)
        self.assertEqual(sanitize_journal([forged]), [entry])
        self.assertEqual(before, self.manager.profile)


class JournalUITests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.app = Game(Path(self.temp.name)/'player.json')

    def tearDown(self):
        pygame.quit()
        self.temp.cleanup()

    def click(self, x, y):
        self.app.draw()
        pygame.event.post(pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1, pos=(x, y)))
        self.app.events()
        self.app.draw()

    def test_menu_routes_and_empty_journal(self):
        self.click(150, 655)
        self.assertEqual(self.app.route, 'journal')
        self.assertEqual(self.app.screen.entries(), [])
        self.app.home()
        self.click(340, 655)
        self.assertEqual(self.app.route, 'help')

    def test_interactive_greater_less_example_is_read_only(self):
        before = copy.deepcopy(self.app.manager.profile)
        self.app.navigate('help')
        self.click(710, 491)
        self.assertEqual(self.app.screen.direction, 'Less')
        self.click(530, 491)
        self.assertEqual(self.app.screen.direction, 'Greater')
        self.assertEqual(before, self.app.manager.profile)

    def test_all_help_tabs_and_ability_navigation(self):
        self.app.navigate('help')
        for index in range(3):
            self.app.screen.tab = index
            self.app.draw()
        self.app.screen.tab = 1
        self.click(357, 568)
        self.assertEqual(self.app.screen.ability_index, 1)
        self.click(93, 568)
        self.assertEqual(self.app.screen.ability_index, 0)
        self.app.manager.ability_data.clear()
        self.app.draw()

    def test_journal_pagination_and_filter(self):
        complete(self.app.manager, 'Classic Match')
        entry = self.app.manager.profile['match_history'][0]
        self.app.manager.profile['match_history'] = [{**copy.deepcopy(entry), 'id': str(i)} for i in range(10)]
        self.app.navigate('journal')
        self.app.draw()
        self.assertEqual(self.app.screen.selected, '9')
        self.click(1215, 658)
        self.assertEqual(self.app.screen.round_page, 1)
        self.click(347, 658)
        self.assertEqual(self.app.screen.page, 1)
        self.click(170, 181)
        self.assertEqual(self.app.screen.selected, '1')
        self.assertEqual(self.app.screen.round_page, 0)
        for _ in range(4):
            self.app.screen.cycle_filter()
            self.app.draw()
        self.assertEqual(self.app.screen.filter, 'ALL')

    def test_journal_view_does_not_grant_more_rewards(self):
        complete(self.app.manager)
        before = copy.deepcopy(self.app.manager.profile)
        for _ in range(3):
            self.app.navigate('journal')
            self.app.draw()
            self.app.home()
        self.assertEqual(before, self.app.manager.profile)

    def test_result_to_journal_button(self):
        complete(self.app.manager)
        self.app.navigate('result')
        self.click(632, 646)
        self.assertEqual(self.app.route, 'journal')
        self.assertEqual(len(self.app.screen.entries()), 1)

    def test_both_resolutions_all_new_pages(self):
        complete(self.app.manager, 'Classic Match')
        for size in ([1280, 720], [1920, 1080]):
            self.app.manager.profile['settings']['resolution'] = size
            self.app.set_display()
            self.app.navigate('journal')
            for page in range(6):
                self.app.screen.round_page = page
                self.app.draw()
            self.app.navigate('help')
            for page in range(3):
                self.app.screen.tab = page
                self.app.draw()
            self.assertEqual(self.app.window.get_size(), tuple(size))


if __name__ == '__main__':
    unittest.main()
