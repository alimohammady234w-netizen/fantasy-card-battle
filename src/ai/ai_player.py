"""The AI interface deliberately has no opponent-card argument."""
import random
from dataclasses import dataclass
from src.systems.resources import ATTRIBUTES


@dataclass(frozen=True)
class Decision:
    attribute: str
    direction: str
    use_ability: bool = True


class AIPlayer:
    def __init__(self, difficulty='Normal', rng=None):
        self.difficulty = difficulty
        self.rng = rng or random.Random()

    def choose(self, own_values, public=None, ability=''):
        public = public or {}
        if self.difficulty == 'Easy':
            return Decision(self.rng.choice(ATTRIBUTES), self.rng.choice(('Greater', 'Less')),
                            self.rng.random() < .35)
        choices = [(value if direction == 'Greater' else 101 - value, attribute, direction)
                   for attribute, value in own_values.items() for direction in ('Greater', 'Less')]
        if self.difficulty == 'Expert':
            distribution = public.get('distribution', {})
            ranked = []
            for strength, attribute, direction in choices:
                pool = distribution.get(attribute, list(range(1, 101)))
                value = own_values[attribute]
                definition = public.get('ability_definition', {})
                estimate = definition.get('ai_strength_estimate', 0)
                if definition.get('effect') == 'rage' and own_values['power'] < definition.get('threshold', 80):
                    estimate = 0
                estimate -= public.get('status_penalty', 0)
                value += estimate if direction == 'Greater' else -estimate
                probability = sum((x < value if direction == 'Greater' else x > value)
                                  + .5 * (x == value) for x in pool) / max(1, len(pool))
                # When behind late in a match, prefer decisive extremes on close choices.
                urgency = max(0, public.get('opponent_score', 0) - public.get('own_score', 0))
                risk = min(.12, urgency / max(1, public.get('remaining', 30)) * .02)
                combo = .005 * min(5, public.get('combo', 0))
                ranked.append((probability + risk * strength / 100 + combo * probability,
                               attribute, direction))
            _, attribute, direction = max(ranked)
        else:
            _, attribute, direction = max(choices)
        chance = {'Normal': .6, 'Hard': .9, 'Expert': 1.}.get(self.difficulty, 1.)
        return Decision(attribute, direction, self.rng.random() < chance)
