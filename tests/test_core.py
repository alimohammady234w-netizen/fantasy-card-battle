import copy
import inspect
import json
import random
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from src.game_manager import GameManager
from src.cards.card_database import CardDatabase
from src.cards.card import Card
from src.ai.ai_player import AIPlayer
from src.abilities.ability_manager import AbilityManager
from src.systems.save_system import SaveSystem
from src.systems.resources import ATTRIBUTES, ROOT


class CoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)/'save.json'
        self.g = GameManager(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def battle(self, mode='Quick Match'):
        battle = self.g.start(mode)
        battle.rng = random.Random(19)
        return battle

    def fixed_battle(self, player=85, enemy=72):
        battle = self.battle()
        a, b = battle.cards
        battle.abilities.definitions = {}
        battle.values = lambda: [dict.fromkeys(ATTRIBUTES, player), dict.fromkeys(ATTRIBUTES, enemy)]
        return battle

    def test_database_schema_and_diversity(self):
        cards = list(self.g.database)
        self.assertGreaterEqual(len(cards), 40)
        self.assertEqual(len({c.name for c in cards}), len(cards))
        self.assertEqual(len(self.g.database.categories), 15)
        self.assertEqual(len({c.rarity for c in cards}), 6)
        self.assertEqual(len({c.ability for c in cards}), 15)
        for card in cards:
            self.assertTrue(all(1 <= value <= 100 for value in card.stats.values()))
        self.assertEqual(sum(r['weight'] for r in self.g.rarities.values()), 100)

    def test_missing_database(self):
        self.assertEqual(len(CardDatabase(Path(self.temp.name)/'missing.json').cards), 0)

    def test_invalid_card_is_skipped(self):
        rows = json.loads((ROOT/'data/cards.json').read_text())
        bad = copy.deepcopy(rows[0])
        bad['power'] = 400
        self.path.write_text(json.dumps([bad, {}, None, rows[1], rows[1]]))
        db = CardDatabase(self.path)
        self.assertEqual(len(db.cards), 1)

    def test_invalid_database_shape(self):
        self.path.write_text('{}')
        self.assertFalse(CardDatabase(self.path).cards)

    def test_new_save_created(self):
        self.assertTrue(self.path.exists())
        self.assertEqual(len(self.g.profile['owned_cards']), 30)
        self.assertTrue(self.g.decks.validate(self.g.profile['decks']['First Light']))

    def test_save_round_trip(self):
        self.g.profile['coins'] = 4321
        self.g.profile['settings']['difficulty'] = 'Expert'
        self.g.progression.reward(1, 525, 200)
        self.g.save()
        self.assertEqual(self.g.profile, self.g.saves.load())
        self.assertFalse(self.path.with_suffix('.tmp').exists())

    def test_corrupt_save_recovery(self):
        for content in ('{broken', '[]', '{"coins":-10}', '{"decks":null}'):
            self.path.write_text(content)
            recovered = self.g.saves.load()
            self.assertEqual(recovered['coins'], 1500)
        self.assertEqual(len(list(self.path.parent.glob('*.corrupt-*'))), 4)

    def test_settings_validation(self):
        self.g.profile['settings'].update(timer=-100, volume='bad', resolution=[0, 0], difficulty='Cheater')
        self.g.save()
        profile = self.g.saves.load()
        self.assertEqual(profile['settings']['timer'], 15)
        self.assertEqual(profile['settings']['resolution'], [1280, 720])
        self.assertEqual(profile['settings']['difficulty'], 'Normal')

    def test_save_write_failure(self):
        self.g.saves.path = self.path.parent  # Existing directory is not replaceable as a save.
        self.assertFalse(self.g.save())
        self.assertTrue(self.g.saves.last_error)

    def test_missing_config_graceful(self):
        with patch('src.game_manager.read_json', return_value={}):
            game = GameManager(Path(self.temp.name)/'second.json')
        self.assertIn('Quick Match', game.config['modes'])
        self.assertIsNotNone(game.start('Quick Match'))

    def test_deck_crud(self):
        decks = self.g.decks
        decks.create('Test deck')
        for i in range(1, 31):
            decks.toggle('Test deck', i)
        self.assertTrue(decks.validate(decks.decks['Test deck']))
        decks.rename('Test deck', 'Renamed')
        self.assertEqual(self.g.profile['selected_deck'], 'Renamed')
        decks.toggle('Renamed', 1)
        self.assertFalse(decks.validate(decks.decks['Renamed']))
        decks.delete('Renamed')
        self.assertEqual(self.g.profile['selected_deck'], 'First Light')
        with self.assertRaises(ValueError):
            decks.delete('First Light')

    def test_deck_rejects_invalid_cards_and_names(self):
        with self.assertRaises(ValueError):
            self.g.decks.create('First Light')
        with self.assertRaises(ValueError):
            self.g.decks.toggle('First Light', 45)
        self.g.profile['decks']['First Light'] = [1]*30
        with self.assertRaises(ValueError):
            self.battle()

    def test_deck_cannot_exceed_thirty(self):
        self.g.profile['owned_cards']['31'] = 1
        with self.assertRaises(ValueError):
            self.g.decks.toggle('First Light', 31)

    def test_card_draw_and_shuffle(self):
        b = self.battle()
        self.assertEqual(len(b.player_deck), 30)
        self.assertEqual(len(b.ai_deck), 30)
        self.assertEqual(len(set(b.ai_deck)), 30)
        self.assertIsInstance(b.cards[0], Card)
        self.assertNotEqual(b.player_deck, list(range(1, 31)))

    def test_greater(self):
        b = self.fixed_battle()
        result = b.resolve('power', 'Greater', False)
        self.assertEqual(result.winner, 0)
        self.assertEqual(b.scores, [2, 0])
        self.assertEqual(result.raw, (85, 72))

    def test_less(self):
        b = self.fixed_battle()
        result = b.resolve('power', 'Less', False)
        self.assertEqual(result.winner, 1)
        self.assertEqual(b.scores, [0, 2])
        self.assertEqual(result.strength, (16, 29))

    def test_tie_no_winner(self):
        b = self.fixed_battle(50, 50)
        b.config['tie_rule'] = 'none'
        self.assertIsNone(b.resolve('age', 'Less').winner)
        self.assertEqual(b.scores, [0, 0])
        self.assertEqual(b.pot, [])

    def test_tie_random(self):
        b = self.fixed_battle(50, 50)
        b.config['tie_rule'] = 'random'
        self.assertIn(b.resolve('age', 'Greater').winner, (0, 1))
        self.assertEqual(sum(b.scores), 2)

    def test_tie_carry(self):
        b = self.fixed_battle(50, 50)
        b.resolve('age', 'Greater')
        b.next_round()
        b.values = lambda: [dict.fromkeys(ATTRIBUTES, 90), dict.fromkeys(ATTRIBUTES, 20)]
        result = b.resolve('power', 'Greater')
        self.assertEqual(result.captured, 4)
        self.assertEqual(b.scores, [4, 0])

    def test_combo_and_rewards(self):
        b = self.fixed_battle()
        results = []
        for _ in range(5):
            results.append(b.resolve('power', 'Greater'))
            b.next_round()
        self.assertEqual(results[2].xp, 100)
        self.assertEqual(results[4].coins, 125)
        self.assertEqual(b.combos, [5, 0])
        self.assertEqual(self.g.profile['statistics']['best_combo'], 5)

    def test_invalid_and_double_resolution(self):
        b = self.battle()
        with self.assertRaises(ValueError):
            b.next_round()
        with self.assertRaises(ValueError):
            b.resolve('unknown', 'Less')
        b.resolve('power', 'Greater')
        coins = self.g.profile['coins']
        with self.assertRaises(ValueError):
            b.resolve('power', 'Greater')
        self.assertEqual(coins, self.g.profile['coins'])

    def test_all_modes_complete(self):
        for mode in ('Quick Match', 'Classic Match', 'Practice'):
            b = self.battle(mode)
            while not b.finished:
                b.resolve('speed', 'Less')
                b.next_round()
            self.assertEqual(len(b.history), b.round_limit)
            stats = copy.deepcopy(self.g.profile['statistics'])
            b.finish()
            self.assertEqual(stats, self.g.profile['statistics'])
        self.assertEqual(self.g.profile['statistics']['matches_played'], 2)

    def test_practice_does_not_modify_profile(self):
        before = copy.deepcopy(self.g.profile)
        b = self.battle('Practice')
        while not b.finished:
            b.resolve('power', 'Greater')
            b.next_round()
        self.assertEqual(before, self.g.profile)

    def test_classic_alternates(self):
        b = self.battle('Classic Match')
        self.assertFalse(b.ai_turn)
        b.resolve('power', 'Greater')
        b.next_round()
        self.assertTrue(b.ai_turn)
        result = b.resolve()
        self.assertIn(result.attribute, ATTRIBUTES)

    def test_ai_no_hidden_argument(self):
        self.assertEqual(list(inspect.signature(AIPlayer.choose).parameters), ['self', 'own_values', 'public', 'ability'])
        own = dict.fromkeys(ATTRIBUTES, 50)
        own['power'] = 99
        for difficulty in ('Easy', 'Normal', 'Hard', 'Expert'):
            decision = AIPlayer(difficulty, random.Random(4)).choose(own)
            self.assertIn(decision.attribute, ATTRIBUTES)
            self.assertIn(decision.direction, ('Greater', 'Less'))
            if difficulty != 'Easy':
                self.assertEqual((decision.attribute, decision.direction), ('power', 'Greater'))
        own['age'] = 1
        self.assertEqual(AIPlayer('Normal').choose(own).direction, 'Less')

    def test_levels_and_upgrade_cap(self):
        progression = self.g.progression
        progression.reward(1, 1000)
        self.assertGreater(progression.card_state(1)['level'], 1)
        self.assertGreater(self.g.profile['level'], 1)
        card = self.g.database[1]
        before = card.values(progression.card_state(1))['power']
        progression.upgrade(card, 'power')
        self.assertEqual(card.values(progression.card_state(1))['power'], before+1)
        progression.card_state(1)['upgrades']['power'] = 100
        with self.assertRaises(ValueError):
            progression.upgrade(card, 'power')
        self.g.profile['coins'] = 0
        with self.assertRaises(ValueError):
            progression.upgrade(card, 'speed')
        with self.assertRaises(ValueError):
            progression.upgrade(self.g.database[45], 'speed')

    def test_all_packs(self):
        self.g.profile['gems'] = 10000
        for name, pack in self.g.pack_data.items():
            gems = self.g.profile['gems']
            rewards = self.g.packs.open(name)
            self.assertEqual(len(rewards), pack['count'])
            self.assertEqual(self.g.profile['gems'], gems-pack['price'])
            self.assertTrue(all(str(c.id) in self.g.profile['owned_cards'] for c, _ in rewards))
        self.assertEqual(self.g.profile['statistics']['total_packs_opened'], 6)

    def test_pack_duplicate_protection(self):
        owned = self.g.profile['owned_cards']
        self.g.pack_data['Test'] = {'price': 1, 'count': 1, 'weights': {'Mythic': 1}, 'currency': 'gems'}
        missing = {c.id for c in self.g.database if c.rarity=='Mythic' and str(c.id) not in owned}
        while missing:
            card, duplicate = self.g.packs.open('Test')[0]
            self.assertFalse(duplicate)
            self.assertIn(card.id, missing)
            missing.remove(card.id)
        coins = self.g.profile['coins']
        self.assertTrue(self.g.packs.open('Test')[0][1])
        self.assertEqual(self.g.profile['coins'], coins+50)

    def test_unaffordable_pack_no_mutation(self):
        self.g.profile['gems'] = 0
        before = copy.deepcopy(self.g.profile)
        with self.assertRaises(ValueError):
            self.g.packs.open('Mythic')
        self.assertEqual(before, self.g.profile)

    def test_all_ability_effects(self):
        definitions = self.g.ability_data
        manager = AbilityManager(definitions, random.Random(1))
        # Ensure probabilistic effects trigger, for deterministic branch assertions.
        manager.rng.random = lambda: 0.
        for name in definitions:
            states = [{'hp': 80, 'statuses': [], 'lost_last': True} for _ in range(2)]
            values = [dict.fromkeys(ATTRIBUTES, 90), dict.fromkeys(ATTRIBUTES, 50)]
            enemy = 'Boost' if name in ('Mirror', 'Copy', 'Silence') else ''
            strengths, logs = manager.resolve([name, enemy], [40, 50], values, states)
            self.assertTrue(logs, name)
            if name=='Heal':
                self.assertEqual(states[0]['hp'], 92)
            elif name in ('Freeze', 'Poison'):
                self.assertTrue(states[1]['statuses'])
            elif name=='Silence':
                self.assertEqual(strengths, [40, 50])
            else:
                self.assertNotEqual(strengths, [40, 50], name)

    def test_status_expires_next_round(self):
        b = self.fixed_battle()
        b.states[0]['statuses'] = [{'amount': 6, 'turns': 1}]
        first = b.resolve('power', 'Greater')
        self.assertEqual(first.strength[0], 79)
        self.assertEqual(b.states[0]['statuses'], [])
        b.next_round()
        self.assertEqual(b.resolve('power', 'Greater').strength[0], 85)

    def test_silence_and_mirror_no_recursion(self):
        manager = AbilityManager(self.g.ability_data)
        states = [{'hp': 100, 'statuses': [], 'lost_last': False} for _ in range(2)]
        values = [dict.fromkeys(ATTRIBUTES, 50)]*2
        strength, _ = manager.resolve(['Mirror', 'Copy'], [50, 50], values, states)
        self.assertEqual(strength, [50, 50])
        strength, _ = manager.resolve(['Silence', 'Rage'], [50, 50], values, states)
        self.assertEqual(strength, [50, 50])


if __name__ == '__main__':
    unittest.main()
