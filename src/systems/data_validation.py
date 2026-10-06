"""Validate external rule files before they reach combat, economy or rendering.

Invalid rule fields use safe defaults. Invalid packs are disabled, never silently
turned into a different purchase. Diagnostics are shared by the game and CLI.
Only the Python standard library is used, so data checks do not require SDL.
"""
import copy
import logging
import math
from src.systems.resources import ROOT, read_json
from src.progression.achievement_system import METRICS, ID_PATTERN

DEFAULT_MODES = {
    'Quick Match': {'rounds': 10, 'ai_turns': False, 'rewards': True},
    'Classic Match': {'rounds': 30, 'ai_turns': True, 'rewards': True},
    'Practice': {'rounds': 10, 'ai_turns': False, 'rewards': False},
}
DEFAULT_SETTINGS = {
    'volume': .8, 'music_volume': .5, 'sound_volume': .8,
    'fullscreen': False, 'resolution': [1280, 720], 'animation_speed': 1.,
    'difficulty': 'Normal', 'timer': 15, 'language': 'English',
}
# Safe recovery defaults, not the authoring source of truth (the JSON files are).
NUMBER_RULES = {
    'upgrade_cost': (100, 0, 1000000), 'level_stat_bonus': (1, 0, 100),
    'round_xp': (20, 0, 1000000), 'win_xp': (50, 0, 1000000),
    'win_coins': (25, 0, 1000000), 'combo_xp_every': (3, 1, 100),
    'combo_coins_every': (5, 1, 100), 'player_level_xp': (250, 1, 1000000),
    'level_reward_coins': (500, 0, 1000000), 'level_reward_gems': (20, 0, 1000000),
    'combo_bonus_xp': (50, 0, 1000000), 'combo_bonus_coins': (100, 0, 1000000),
    'match_win_gems': (15, 0, 1000000), 'match_other_gems': (5, 0, 1000000),
    'duplicate_coins': (50, 0, 1000000),
}
RARITY_DEFAULTS = dict(zip(
    ('Common', 'Uncommon', 'Rare', 'Epic', 'Legendary', 'Mythic'),
    ((147, 160, 179, 50), (94, 202, 150, 25), (83, 166, 248, 14),
     (180, 119, 246, 7), (239, 188, 94, 3), (245, 102, 147, 1))))


def number(value, low, high, integer=False):
    """Reject booleans, strings and NaN/Infinity, as well as out-of-range values."""
    types = (int,) if integer else (int, float)
    return type(value) in types and low <= value <= high and math.isfinite(value)


def validate_settings(raw, report=None):
    result = copy.deepcopy(DEFAULT_SETTINGS)
    report = report or (lambda message: logging.warning(message))
    if not isinstance(raw, dict):
        report('settings: expected an object; using defaults')
        return result
    options = {'resolution': ([1280, 720], [1920, 1080]),
               'animation_speed': (.5, 1., 1.5, 2.),
               'difficulty': ('Easy', 'Normal', 'Hard', 'Expert'),
               'timer': (0, 10, 15, 30, 60), 'language': ('English',)}
    for key, value in raw.items():
        if key not in result:
            report(f'settings.{key}: unknown setting ignored')
            continue
        if key in ('volume', 'music_volume', 'sound_volume'):
            valid = number(value, 0, 1)
        elif key == 'fullscreen':
            valid = type(value) is bool
        else:
            valid = not isinstance(value, bool) and value in options[key]
        if valid:
            result[key] = copy.deepcopy(value)
        else:
            report(f'settings.{key}: invalid value; using default')
    return result


class DataCatalog:
    def __init__(self, root=ROOT, reader=read_json):
        self.root, self.reader, self.issues = root, reader, []
        self.config = self.rules(self.load('config.json'))
        self.rarities = self.rarity_rules(self.load('data/rarities.json'))
        self.abilities = self.ability_rules(self.load('data/abilities.json'))
        self.packs = self.pack_rules(self.load('data/packs.json'))
        self.achievements = self.achievement_rules(self.load('data/achievements.json'))
        self.settings = validate_settings(self.load('data/settings.json'), self.report)

    def report(self, message):
        self.issues.append(message)
        logging.warning('Data validation: %s', message)

    def load(self, relative):
        data = self.reader(self.root / relative, None)
        if not isinstance(data, dict):
            self.report(f'{relative}: missing, unreadable, or not a JSON object')
            return {}
        return data

    def rules(self, raw):
        result = copy.deepcopy(raw)
        for key, (default, low, high) in NUMBER_RULES.items():
            value = raw.get(key, default)
            if not number(value, low, high, integer=True):
                self.report(f'config.{key}: expected integer {low}..{high}; using {default}')
                value = default
            result[key] = value
        # v1 UI and persistence deliberately have a fixed 30-card deck contract.
        result['deck_size'] = 30
        if raw.get('deck_size', 30) != 30 or isinstance(raw.get('deck_size'), bool):
            self.report('config.deck_size: v1 requires 30 cards; using 30')
        result['tie_rule'] = raw.get('tie_rule', 'carry')
        if result['tie_rule'] not in ('none', 'random', 'carry'):
            self.report('config.tie_rule: expected none/random/carry; using carry')
            result['tie_rule'] = 'carry'
        thresholds = raw.get('card_xp_thresholds', [100, 250, 500])
        if (not isinstance(thresholds, list) or not thresholds or len(thresholds) > 100
                or not all(number(v, 1, 1000000, integer=True) for v in thresholds)
                or thresholds != sorted(set(thresholds))):
            self.report('config.card_xp_thresholds: expected positive strictly increasing integers')
            thresholds = [100, 250, 500]
        result['card_xp_thresholds'] = thresholds
        modes = raw.get('modes', DEFAULT_MODES)
        if not isinstance(modes, dict):
            self.report('config.modes: expected object; using defaults')
            modes = {}
        result['modes'] = {}
        for name, default in DEFAULT_MODES.items():
            row = modes.get(name, default)
            if (not isinstance(row, dict) or not number(row.get('rounds'), 1, 30, integer=True)
                    or type(row.get('ai_turns')) is not bool or type(row.get('rewards')) is not bool):
                self.report(f'config.modes.{name}: invalid mode; using default')
                row = default
            result['modes'][name] = copy.deepcopy(row)
        for name in modes.keys() - DEFAULT_MODES.keys():
            self.report(f'config.modes.{name}: custom modes need a UI/flow implementation; ignored')
        return result

    def rarity_rules(self, raw):
        result = {}
        for name, row in raw.items():
            if not name.strip() or not isinstance(row, dict):
                self.report(f'rarities.{name}: invalid rarity; skipped')
                continue
            color = row.get('color')
            if (not isinstance(color, list) or len(color) != 3
                    or not all(number(v, 0, 255, integer=True) for v in color)):
                self.report(f'rarities.{name}.color: invalid RGB; using neutral frame')
                color = [147, 160, 179]
            clean = {'color': color}
            for key, (default, low, high, integer) in {
                'weight': (0, 0, 1000000, False), 'border': (2, 1, 5, True),
                'glow': (15, 0, 100, False),
            }.items():
                value = row.get(key, default)
                if not number(value, low, high, integer):
                    self.report(f'rarities.{name}.{key}: invalid value; using {default}')
                    value = default
                clean[key] = value
            result[name] = clean
        if not result:
            self.report('rarities: no usable definitions; using recovery frames')
            result = {name: {'color': list(values[:3]), 'weight': values[3], 'border': 2, 'glow': 15}
                      for name, values in RARITY_DEFAULTS.items()}
        return result

    def ability_rules(self, raw):
        result = {}
        for name, row in raw.items():
            if (not name.strip() or not isinstance(row, dict)
                    or not isinstance(row.get('effect'), str) or not row['effect'].strip()):
                self.report(f'abilities.{name}: missing effect; disabled')
                continue
            clean = copy.deepcopy(row)  # Preserve custom fields for registered handlers.
            for key, (default, low, high, integer) in {
                'amount': (0, 0, 1000, False), 'chance': (0, 0, 1, False),
                'duration': (1, 1, 30, True), 'threshold': (80, 1, 100, False),
                'ai_strength_estimate': (0, 0, 1000, False),
            }.items():
                if key in clean and not number(clean[key], low, high, integer):
                    self.report(f'abilities.{name}.{key}: invalid value; using {default}')
                    clean[key] = default
            if not isinstance(clean.get('description', ''), str):
                self.report(f'abilities.{name}.description: expected text; using empty text')
                clean['description'] = ''
            result[name] = clean
        return result

    def pack_rules(self, raw):
        result = {}
        for name, row in raw.items():
            if (not name.strip() or not isinstance(row, dict)
                    or not number(row.get('price'), 0, 1000000, integer=True)
                    or not number(row.get('count'), 1, 10, integer=True)
                    or row.get('currency', 'gems') not in ('gems', 'coins')):
                self.report(f'packs.{name}: invalid price/count/currency; disabled')
                continue
            weights = row.get('weights', {k: v['weight'] for k, v in self.rarities.items()})
            if (not isinstance(weights, dict) or not weights
                    or any(k not in self.rarities or not number(v, 0, 1000000) for k, v in weights.items())
                    or sum(weights.values()) <= 0):
                self.report(f'packs.{name}: invalid or empty rarity weights; disabled')
                continue
            result[name] = {**row, 'weights': weights, 'currency': row.get('currency', 'gems')}
        return result

    def check_cards(self, database):
        for card in database:
            if card.rarity not in self.rarities:
                self.report(f'cards.{card.id}: unknown rarity {card.rarity}; neutral frame, not in packs')
            if card.ability not in self.abilities:
                self.report(f'cards.{card.id}: unknown ability {card.ability}; effect disabled')
        if len(database.cards) < 30:
            self.report('cards: fewer than 30 usable cards; matches are unavailable')
        for name, pack in self.packs.items():
            if not any(pack['weights'].get(card.rarity, 0) > 0 for card in database):
                self.report(f'packs.{name}: no cards match its rarity weights; purchase unavailable')

    def achievement_rules(self, raw):
        result = {}
        for key, row in raw.items():
            valid = (ID_PATTERN.fullmatch(key) is not None and isinstance(row, dict))
            if valid:
                valid = all(isinstance(row.get(field), str) and row[field].strip()
                            and len(row[field]) <= limit
                            for field, limit in (('title', 64), ('description', 160), ('group', 32)))
            if valid:
                metric = row.get('metric')
                valid = (isinstance(metric, str) and metric in METRICS
                         and number(row.get('target'), 1, 1000000000, integer=True))
            if valid:
                rewards = row.get('rewards')
                valid = (isinstance(rewards, dict) and bool(rewards)
                         and all(currency in ('coins', 'gems') and number(amount, 0, 1000000, integer=True)
                                 for currency, amount in rewards.items())
                         and sum(rewards.values()) > 0)
            if not valid:
                self.report(f'achievements.{key}: invalid definition or reward; disabled')
                continue
            result[key] = {field: row[field] for field in ('title', 'description', 'group', 'metric', 'target')}
            result[key]['rewards'] = {'coins': rewards.get('coins', 0), 'gems': rewards.get('gems', 0)}
        return result
