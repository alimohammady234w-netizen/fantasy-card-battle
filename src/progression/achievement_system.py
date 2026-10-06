"""Read-only achievement evaluation; GameManager owns the atomic claim transaction.

Definitions are JSON data. Progress is derived from persisted lifetime statistics,
so old saves gain eligible achievements retroactively without replaying matches.
"""
import re
import logging
from dataclasses import dataclass

STAT_METRICS = frozenset({
    'matches_played', 'matches_won', 'matches_lost', 'cards_won', 'cards_lost',
    'best_combo', 'highest_score', 'total_xp', 'total_coins_earned', 'total_packs_opened',
})
METRICS = STAT_METRICS | {'player_level', 'unlocked_cards', 'highest_card_level', 'upgrades_purchased'}
ID_PATTERN = re.compile(r'^[a-z0-9_]{1,64}$')


def sanitize_claims(value):
    """Keep unknown valid IDs: removing/reintroducing a definition must not repay it."""
    if not isinstance(value, list):
        logging.warning('Ignoring malformed achievement claim ledger')
        return []
    return list(dict.fromkeys(key for key in value if isinstance(key, str) and ID_PATTERN.fullmatch(key)))


@dataclass(frozen=True)
class AchievementProgress:
    current: int
    target: int
    claimed: bool

    @property
    def ready(self):
        return not self.claimed and self.current >= self.target

    @property
    def status(self):
        return 'CLAIMED' if self.claimed else 'READY' if self.ready else 'LOCKED'

    @property
    def ratio(self):
        return min(self.current, self.target) / self.target


class AchievementSystem:
    def __init__(self, profile, definitions, database):
        self.profile, self.definitions, self.database = profile, definitions, database

    def metric(self, name):
        p = self.profile
        if name in STAT_METRICS:
            return p['statistics'].get(name, 0)
        if name == 'player_level':
            return p['level']
        if name == 'unlocked_cards':
            return len(p['owned_cards'])
        if name == 'upgrades_purchased':
            return sum(sum(state.get('upgrades', {}).values())
                       for key, state in p['card_progress'].items() if key in p['owned_cards'])
        if name == 'highest_card_level':
            return max((p['card_progress'].get(key, {}).get('level', self.database[int(key)].level)
                        for key in p['owned_cards'] if int(key) in self.database.cards), default=0)
        raise ValueError('Unknown achievement metric: ' + name)

    def progress(self, achievement_id):
        if achievement_id not in self.definitions:
            raise ValueError('Achievement is unavailable.')
        definition = self.definitions[achievement_id]
        return AchievementProgress(self.metric(definition['metric']), definition['target'],
                                   achievement_id in self.profile.get('claimed_achievements', []))

    @property
    def ready_count(self):
        return sum(self.progress(key).ready for key in self.definitions)

    @property
    def claimed_count(self):
        claimed = set(self.profile.get('claimed_achievements', []))
        return sum(key in claimed for key in self.definitions)

    def reward_for_claim(self, achievement_id):
        state = self.progress(achievement_id)
        if state.claimed:
            raise ValueError('This achievement reward has already been claimed.')
        if not state.ready:
            raise ValueError('Complete the achievement before claiming its reward.')
        return dict(self.definitions[achievement_id]['rewards'])
