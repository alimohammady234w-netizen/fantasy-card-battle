"""Bounded, JSON-only records of completed matches, never hidden deck snapshots."""
import logging
import math
from datetime import datetime, timezone
from src.systems.resources import ATTRIBUTES

MAX_MATCHES = 50


def _integer(value, low=0, high=1000000000):
    if type(value) is not int or not low <= value <= high:
        raise ValueError('Invalid journal integer')
    return value


def _text(value, limit=96):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('Invalid journal text')
    return value[:limit]


def _pair(values, raw=False):
    if not isinstance(values, list) or len(values) != 2:
        raise ValueError('Invalid comparison pair')
    for value in values:
        if type(value) not in (int, float) or not -1000000 <= value <= 1000000 or not math.isfinite(value):
            raise ValueError('Invalid comparison value')
        if raw and not 1 <= value <= 100:
            raise ValueError('Invalid attribute value')
    return list(values)


def _round(row, index):
    if not isinstance(row, dict) or row.get('round') != index:
        raise ValueError('Invalid round order')
    if row.get('attribute') not in ATTRIBUTES or row.get('direction') not in ('Greater', 'Less'):
        raise ValueError('Invalid comparison rule')
    winner = row.get('winner')
    if winner is not None and (type(winner) is not int or winner not in (0, 1)):
        raise ValueError('Invalid round winner')
    if row.get('chooser') not in ('Player', 'AI'):
        raise ValueError('Invalid turn owner')
    abilities = row.get('abilities', [])
    if not isinstance(abilities, list) or not all(isinstance(text, str) for text in abilities):
        raise ValueError('Invalid ability log')
    return {'round': _integer(row['round'], 1, 30),
            'player': _text(row.get('player')), 'opponent': _text(row.get('opponent')),
            'attribute': row['attribute'], 'direction': row['direction'], 'chooser': row['chooser'],
            'raw': _pair(row.get('raw'), raw=True), 'strength': _pair(row.get('strength')),
            'winner': winner, 'captured': _integer(row.get('captured'), 0, 60),
            'xp': _integer(row.get('xp')), 'coins': _integer(row.get('coins')),
            'abilities': [text[:160] for text in abilities[:4]]}


def sanitize_journal(raw):
    """Bad optional history never resets valid progression or currencies."""
    if not isinstance(raw, list):
        logging.warning('Ignoring malformed match journal')
        return []
    clean, seen = [], set()
    for item in raw[-MAX_MATCHES:]:
        try:
            if not isinstance(item, dict):
                raise ValueError('Invalid journal entry')
            match_id = _text(item.get('id'), 64)
            if match_id in seen:
                continue
            completed = _text(item.get('completed_at'), 40)
            stamp = datetime.fromisoformat(completed)
            if stamp.tzinfo is None:
                raise ValueError('Journal timestamp requires a timezone')
            rows = item.get('rounds')
            if not isinstance(rows, list) or not 1 <= len(rows) <= 30:
                raise ValueError('Invalid journal rounds')
            rounds = [_round(row, i+1) for i, row in enumerate(rows)]
            scores = item.get('scores')
            if not isinstance(scores, list) or len(scores) != 2:
                raise ValueError('Invalid match score')
            scores = [_integer(score, 0, 60) for score in scores]
            if scores != [sum(r['captured'] for r in rounds if r['winner'] == side) for side in (0, 1)]:
                raise ValueError('Scores disagree with captured cards')
            entry = {'id': match_id, 'completed_at': completed,
                     'mode': _text(item.get('mode'), 40), 'difficulty': _text(item.get('difficulty'), 20),
                     'scores': scores, 'rounds': rounds,
                     'xp': sum(r['xp'] for r in rounds), 'coins': sum(r['coins'] for r in rounds)}
            clean.append(entry)
            seen.add(match_id)
        except (ValueError, TypeError, KeyError, OverflowError) as error:
            logging.warning('Ignoring invalid match journal entry: %s', error)
    return clean


def record_match(battle):
    """Called only after finish; a journal read never grants a reward."""
    if not battle.finished or not battle.mode.get('rewards', True):
        return
    profile = battle.progression.profile
    entries = profile.setdefault('match_history', [])
    if any(entry['id'] == battle.match_id for entry in entries):
        return
    rounds = []
    for i, result in enumerate(battle.history):
        rounds.append({
            'round': result.round,
            'player': battle.database[battle.player_deck[i]].name,
            'opponent': battle.database[battle.ai_deck[i]].name,
            'attribute': result.attribute, 'direction': result.direction,
            'chooser': 'AI' if battle.mode['ai_turns'] and i % 2 else 'Player',
            'raw': list(result.raw), 'strength': list(result.strength),
            'winner': result.winner, 'captured': result.captured,
            'xp': result.xp, 'coins': result.coins, 'abilities': list(result.abilities),
        })
    entry = {'id': battle.match_id, 'completed_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
             'mode': battle.mode_name, 'difficulty': battle.ai.difficulty,
             'scores': list(battle.scores), 'rounds': rounds,
             'xp': sum(r['xp'] for r in rounds), 'coins': sum(r['coins'] for r in rounds)}
    entries.append(entry)
    del entries[:-MAX_MATCHES]
