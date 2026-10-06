import random
from uuid import uuid4
from src.battle.match_journal import record_match
from dataclasses import dataclass
from src.ai.ai_player import AIPlayer
from src.abilities.ability_manager import AbilityManager
from src.systems.resources import ATTRIBUTES


@dataclass
class RoundResult:
    round: int
    attribute: str
    direction: str
    raw: tuple
    strength: tuple
    winner: int | None
    captured: int
    xp: int
    coins: int
    abilities: list
    leveled: bool = False


class BattleManager:
    def __init__(self, database, deck, config, definitions, progression, mode='Quick Match', difficulty='Normal', rng=None):
        self.rng = rng or random.Random()
        self.database, self.config, self.progression = database, config, progression
        if len(deck) != config.get('deck_size', 30) or len(set(deck)) != len(deck):
            raise ValueError('Select a valid 30-card deck.')
        if any(i not in database.cards or str(i) not in progression.profile['owned_cards'] for i in deck):
            raise ValueError('Deck contains unavailable cards.')
        if len(database.cards) < len(deck):
            raise ValueError('Not enough cards in the database.')
        self.mode_name = mode
        self.match_id = uuid4().hex
        self.mode = config['modes'][mode]
        self.player_deck = list(deck)
        self.ai_deck = self.rng.sample(list(database.cards), len(deck))
        self.rng.shuffle(self.player_deck)
        self.rng.shuffle(self.ai_deck)
        self.player_values = {str(i): database[i].values(
            progression.profile['card_progress'].get(str(i), {}), config.get('level_stat_bonus', 1)) for i in deck}
        self.round_limit = min(len(deck), self.mode['rounds'])
        self.index = 0
        self.scores, self.combos = [0, 0], [0, 0]
        self.captured = [[], []]
        self.pot = []
        self.states = [{'hp': 100, 'statuses': [], 'lost_last': False} for _ in range(2)]
        self.ai = AIPlayer(difficulty, self.rng)
        self.abilities = AbilityManager(definitions, self.rng)
        self.history = []
        self.result = None
        self.finished = False
        self.recorded = False
        self.distribution = {a: [c.stats[a] for c in database] for a in ATTRIBUTES}

    @property
    def cards(self):
        return self.database[self.player_deck[self.index]], self.database[self.ai_deck[self.index]]

    @property
    def ai_turn(self):
        return self.mode['ai_turns'] and self.index % 2 == 1

    def values(self):
        player, ai = self.cards
        return [dict(self.player_values[str(player.id)]), ai.values()]

    def ai_decision(self):
        return self.ai.choose(self.cards[1].values(), {'distribution': self.distribution,
                              'ability_definition': self.abilities.definitions.get(self.cards[1].ability, {}),
                              'status_penalty': sum(s['amount'] for s in self.states[1]['statuses']),
                              'own_score': self.scores[1], 'opponent_score': self.scores[0],
                              'remaining': self.round_limit-self.index, 'combo': self.combos[1]}, self.cards[1].ability)

    def resolve(self, attribute=None, direction=None, use_ability=True):
        if self.finished or self.result:
            raise ValueError('This round is already resolved.')
        ai_choice = self.ai_decision()
        if self.ai_turn:
            attribute, direction = ai_choice.attribute, ai_choice.direction
        if attribute not in ATTRIBUTES or direction not in ('Greater', 'Less'):
            raise ValueError('Choose an attribute and Greater or Less.')
        cards, values = self.cards, self.values()
        raw = [v[attribute] for v in values]
        strength = [v if direction == 'Greater' else 101-v for v in raw]
        for i, state in enumerate(self.states):
            for effect in state['statuses']:
                strength[i] -= effect['amount']
                effect['turns'] -= 1
            state['statuses'] = [s for s in state['statuses'] if s['turns'] > 0]
        strength, messages = self.abilities.resolve([c.ability for c in cards], strength, values,
                                                   self.states, (use_ability, ai_choice.use_ability))
        winner = 0 if strength[0] > strength[1] else 1 if strength[1] > strength[0] else None
        self.pot.extend([c.id for c in cards])
        if winner is None and self.config.get('tie_rule') == 'random':
            winner = self.rng.randrange(2)
        captured = 0
        if winner is not None:
            captured = len(self.pot)
            self.scores[winner] += captured
            self.captured[winner].extend(self.pot)
            self.pot = []
            self.combos[winner] += 1
            self.combos[1-winner] = 0
            self.states[1-winner]['hp'] = max(0, self.states[1-winner]['hp']-10)
        else:
            self.combos = [0, 0]
            if self.config.get('tie_rule') != 'carry':
                self.pot = []
        for i in range(2):
            self.states[i]['lost_last'] = winner == 1-i
        xp, coins, leveled = 0, 0, False
        if self.mode.get('rewards', True):
            xp = self.config.get('win_xp', 50) if winner == 0 else self.config.get('round_xp', 20)
            coins = self.config.get('win_coins', 25) if winner == 0 else 0
            if self.combos[0] and self.combos[0] % self.config.get('combo_xp_every', 3) == 0:
                xp += self.config.get('combo_bonus_xp', 50)
            if self.combos[0] and self.combos[0] % self.config.get('combo_coins_every', 5) == 0:
                coins += self.config.get('combo_bonus_coins', 100)
            leveled = self.progression.reward(cards[0].id, xp, coins)
            stats = self.progression.profile['statistics']
            stats['best_combo'] = max(stats['best_combo'], self.combos[0])
            if winner is not None:
                stats['cards_won' if winner == 0 else 'cards_lost'] += captured
        self.result = RoundResult(self.index+1, attribute, direction, tuple(raw), tuple(strength),
                                  winner, captured, xp, coins, messages, leveled)
        self.history.append(self.result)
        return self.result

    def next_round(self):
        if not self.result:
            raise ValueError('Resolve the round first.')
        if self.index + 1 >= self.round_limit:
            self.finished = True
            self.finish()
        else:
            self.index += 1
            self.result = None

    def finish(self):
        if self.recorded or not self.finished:
            return
        self.recorded = True
        if not self.mode.get('rewards', True):
            return
        stats = self.progression.profile['statistics']
        stats['matches_played'] += 1
        if self.scores[0] != self.scores[1]:
            stats['matches_won' if self.scores[0] > self.scores[1] else 'matches_lost'] += 1
        stats['highest_score'] = max(stats['highest_score'], self.scores[0])
        self.progression.profile['gems'] += self.config.get('match_win_gems', 15) if self.scores[0] > self.scores[1] else self.config.get('match_other_gems', 5)

        record_match(self)
