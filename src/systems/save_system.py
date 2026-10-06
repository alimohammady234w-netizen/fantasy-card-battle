"""Validated, atomic local JSON persistence. Corrupt files are retained for recovery."""
import json
import logging
import time
from pathlib import Path
from src.systems.resources import USER_ROOT, ROOT, read_json, ATTRIBUTES

from src.battle.match_journal import sanitize_journal
from src.progression.achievement_system import sanitize_claims
from src.systems.data_validation import DEFAULT_SETTINGS, validate_settings

STAT_KEYS = ('matches_played', 'matches_won', 'matches_lost', 'cards_won', 'cards_lost',
             'best_combo', 'highest_score', 'total_xp', 'total_coins_earned', 'total_packs_opened')


class SaveSystem:
    def __init__(self, database, path=None):
        self.database = database
        self.path = Path(path or USER_ROOT / 'saves/player_save.json')
        self.last_error = ''

    def default(self):
        ids = list(self.database.cards)[:30]
        settings = validate_settings(read_json(ROOT / 'data/settings.json', {}))
        return {'version': 1, 'level': 1, 'xp': 0, 'coins': 1500, 'gems': 300,
                'unlocked_cards': ids, 'owned_cards': {str(i): 1 for i in ids},
                'card_progress': {}, 'decks': {'First Light': ids}, 'selected_deck': 'First Light',
                'settings': settings, 'statistics': dict.fromkeys(STAT_KEYS, 0), 'match_history': [], 'active_match': None, 'claimed_achievements': []}

    def validate(self, data):
        if not isinstance(data, dict):
            raise ValueError('Save must be an object')
        result = self.default()
        for key in ('level', 'xp', 'coins', 'gems'):
            value = data.get(key, result[key])
            if type(value) is not int or value < (1 if key == 'level' else 0):
                raise ValueError('Invalid currency or progression')
            result[key] = value
        owned = data.get('owned_cards', result['owned_cards'])
        if not isinstance(owned, dict):
            raise ValueError('Invalid collection')
        result['owned_cards'] = {str(k): v for k, v in owned.items()
                                 if str(k).isdigit() and int(k) in self.database.cards
                                 and type(v) is int and v > 0}
        result['unlocked_cards'] = [int(k) for k in result['owned_cards']]
        decks = data.get('decks', result['decks'])
        if not isinstance(decks, dict):
            raise ValueError('Invalid decks')
        result['decks'] = {}
        for name, ids in decks.items():
            if not isinstance(ids, list):
                continue
            result['decks'][str(name)[:40]] = list(dict.fromkeys(
                i for i in ids if type(i) is int and str(i) in result['owned_cards']))[:30]
        if not result['decks']:
            result['decks'] = {'First Light': result['unlocked_cards'][:30]}
        selected = data.get('selected_deck')
        result['selected_deck'] = selected if selected in result['decks'] else next(iter(result['decks']))
        progress = data.get('card_progress', {})
        if not isinstance(progress, dict):
            raise ValueError('Invalid card progression')
        result['card_progress'] = {}
        for key, value in progress.items():
            if key not in result['owned_cards'] or not isinstance(value, dict):
                continue
            level, xp = value.get('level', 1), value.get('xp', 0)
            if type(level) is not int or type(xp) is not int or level < 1 or xp < 0:
                continue
            upgrades = value.get('upgrades', {})
            if not isinstance(upgrades, dict):
                upgrades = {}
            result['card_progress'][key] = {'level': level, 'xp': xp, 'upgrades': {
                a: min(99, v) for a, v in upgrades.items()
                if a in ATTRIBUTES and type(v) is int and v >= 0}}
        settings = data.get('settings', {})
        if not isinstance(settings, dict):
            settings = {}
        merged = {**result['settings'], **settings}
        result['settings'] = validate_settings(merged)
        stats = data.get('statistics', {})
        if isinstance(stats, dict):
            result['statistics'].update({k: v for k, v in stats.items()
                                         if k in STAT_KEYS and type(v) is int and v >= 0})
        result['match_history'] = sanitize_journal(data.get('match_history', []))
        # Full checkpoint validation needs the current database/rules in GameManager.
        result['active_match'] = data.get('active_match')
        result['claimed_achievements'] = sanitize_claims(data.get('claimed_achievements', []))
        return result

    def load(self):
        try:
            with self.path.open(encoding='utf-8') as handle:
                return self.validate(json.load(handle))
        except FileNotFoundError:
            pass
        except (OSError, ValueError, TypeError, KeyError) as error:
            logging.warning('Save recovery: %s', error)
            try:
                self.path.rename(self.path.with_suffix(f'.corrupt-{time.time_ns()}'))
            except OSError:
                pass
        data = self.validate(self.default())
        self.save(data)
        return data

    def save(self, data):
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            temp = self.path.with_suffix('.tmp')
            temp.write_text(json.dumps(data, indent=2), encoding='utf-8')
            temp.replace(self.path)
            self.last_error = ''
            return True
        except OSError as error:
            self.last_error = str(error)
            logging.error('Save failed: %s', error)
            return False
