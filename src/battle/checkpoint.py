"""Versioned JSON checkpoints. No pickle, executable objects, or reward replay.

The profile and its active checkpoint are written in one atomic save transaction.
A fingerprint invalidates suspended games if authored cards/rules change.
"""
import copy
import hashlib
import json
import math
import random
from dataclasses import asdict
from src.systems.resources import ATTRIBUTES


def fingerprint(database, config, definitions):
    content = {'cards': [asdict(c) for c in sorted(database, key=lambda c: c.id)],
               'config': config, 'abilities': definitions}
    return hashlib.sha256(json.dumps(content, sort_keys=True, allow_nan=False).encode()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError('Invalid checkpoint: ' + message)


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def pair(value, low, high, whole=False):
    require(isinstance(value, list) and len(value) == 2, 'pair shape')
    types = (int,) if whole else (int, float)
    require(all(type(v) in types and low <= v <= high and math.isfinite(v) for v in value), 'pair values')
    return list(value)


class CheckpointCodec:
    VERSION = 1

    @classmethod
    def encode(cls, battle):
        if battle.finished:
            return None
        return {'version': cls.VERSION, 'fingerprint': fingerprint(battle.database, battle.config, battle.abilities.definitions),
                'id': battle.match_id, 'mode': battle.mode_name, 'difficulty': battle.ai.difficulty,
                'player_deck': list(battle.player_deck), 'ai_deck': list(battle.ai_deck),
                'player_values': copy.deepcopy(battle.player_values),
                'index': battle.index, 'scores': list(battle.scores), 'combos': list(battle.combos),
                'states': copy.deepcopy(battle.states), 'pot': list(battle.pot),
                'captured': copy.deepcopy(battle.captured),
                'history': [{**asdict(r), 'raw': list(r.raw), 'strength': list(r.strength)} for r in battle.history],
                'rng': [battle.rng.getstate()[0], list(battle.rng.getstate()[1]), battle.rng.getstate()[2]]}

    @classmethod
    def decode(cls, payload, database, config, definitions, progression):
        # Imports stay local to keep rules and save validation free of import cycles.
        from src.battle.battle_manager import BattleManager, RoundResult
        require(isinstance(payload, dict), 'expected object')
        p = copy.deepcopy(payload)
        require(type(p.get('version')) is int and p['version'] == cls.VERSION, 'unsupported version')
        require(p.get('fingerprint') == fingerprint(database, config, definitions), 'cards or rules changed')
        match_id = p.get('id')
        require(isinstance(match_id, str) and len(match_id) == 32 and all(c in '0123456789abcdef' for c in match_id), 'match id')
        require(not any(e['id'] == match_id for e in progression.profile.get('match_history', [])), 'match already completed')
        mode, difficulty = p.get('mode'), p.get('difficulty')
        require(isinstance(mode, str) and mode in config['modes'], 'mode')
        require(difficulty in ('Easy', 'Normal', 'Hard', 'Expert'), 'difficulty')
        decks = []
        for key in ('player_deck', 'ai_deck'):
            deck = p.get(key)
            require(isinstance(deck, list) and len(deck) == 30, 'deck size')
            require(all(type(i) is int and i in database.cards for i in deck), 'unknown card')
            require(len(set(deck)) == 30, 'duplicate card')
            decks.append(deck)
        require(all(str(i) in progression.profile['owned_cards'] for i in decks[0]), 'unowned player card')
        limit = min(30, config['modes'][mode]['rounds'])
        index = p.get('index')
        require(integer(index, 0, limit-1), 'round index')
        values = p.get('player_values')
        require(isinstance(values, dict) and set(values) == {str(i) for i in decks[0]}, 'frozen card values')
        for row in values.values():
            require(isinstance(row, dict) and set(row) == set(ATTRIBUTES), 'attribute keys')
            require(all(integer(v, 1, 100) for v in row.values()), 'attribute range')
        rows = p.get('history')
        require(isinstance(rows, list) and len(rows) in (index, index+1), 'history length')
        history = []
        captured, pot, combos = [[], []], [], [0, 0]
        for i, row in enumerate(rows):
            require(isinstance(row, dict) and integer(row.get('round'), 1, limit) and row['round'] == i+1, 'history order')
            require(row.get('attribute') in ATTRIBUTES and row.get('direction') in ('Greater', 'Less'), 'comparison rule')
            raw = pair(row.get('raw'), 1, 100, whole=True)
            require(raw == [values[str(decks[0][i])][row['attribute']], database[decks[1][i]].values()[row['attribute']]], 'base values mismatch')
            strength = pair(row.get('strength'), -1000000, 1000000)
            winner = row.get('winner')
            require(winner is None or (type(winner) is int and winner in (0, 1)), 'winner')
            if strength[0] != strength[1]:
                require(winner == (0 if strength[0] > strength[1] else 1), 'winner mismatch')
            else:
                require(winner in (0, 1) if config['tie_rule'] == 'random' else winner is None, 'tie rule mismatch')
            for key in ('xp', 'coins'):
                require(integer(row.get(key), 0, 1000000000), 'round reward')
            require(type(row.get('leveled')) is bool, 'level flag')
            messages = row.get('abilities')
            require(isinstance(messages, list) and len(messages) <= 4
                    and all(isinstance(m, str) and len(m) <= 1000 for m in messages), 'ability log')
            pot.extend([decks[0][i], decks[1][i]])
            expected = 0
            if winner is not None:
                expected = len(pot)
                captured[winner].extend(pot)
                pot = []
                combos[winner] += 1
                combos[1-winner] = 0
            else:
                combos = [0, 0]
                if config['tie_rule'] != 'carry':
                    pot = []
            require(integer(row.get('captured'), 0, 60) and row['captured'] == expected, 'round captures')
            history.append(RoundResult(i+1, row['attribute'], row['direction'], tuple(raw), tuple(strength),
                                       winner, expected, row['xp'], row['coins'], list(messages), row['leveled']))
        scores = pair(p.get('scores'), 0, 60, whole=True)
        require(scores == [len(c) for c in captured], 'score mismatch')
        require(p.get('captured') == captured and p.get('pot') == pot, 'captured cards mismatch')
        require(pair(p.get('combos'), 0, 30, whole=True) == combos, 'combo mismatch')
        states = p.get('states')
        require(isinstance(states, list) and len(states) == 2, 'combat states')
        clean_states = []
        for state in states:
            require(isinstance(state, dict) and integer(state.get('hp'), 0, 100), 'HP')
            require(type(state.get('lost_last')) is bool, 'loss flag')
            statuses = state.get('statuses')
            require(isinstance(statuses, list) and len(statuses) <= 60, 'status count')
            clean_statuses = []
            for status in statuses:
                require(isinstance(status, dict) and integer(status.get('turns'), 1, 30), 'status duration')
                amount = status.get('amount')
                require(type(amount) in (int, float) and 0 <= amount <= 1000 and math.isfinite(amount), 'status amount')
                clean_statuses.append({'amount': amount, 'turns': status['turns']})
            clean_states.append({'hp': state['hp'], 'lost_last': state['lost_last'], 'statuses': clean_statuses})
        rng_data = p.get('rng')
        require(isinstance(rng_data, list) and len(rng_data) == 3 and rng_data[0] == 3, 'random version')
        stream = rng_data[1]
        require(isinstance(stream, list) and len(stream) == 625, 'random stream')
        require(all(integer(v, 0, 2**32-1) for v in stream[:-1]) and integer(stream[-1], 0, 624), 'random stream values')
        gaussian = rng_data[2]
        require(gaussian is None or (type(gaussian) in (int, float) and math.isfinite(gaussian)), 'random cache')
        rng = random.Random()
        # Construct normally, then restore all state without resolve/reward/finish calls.
        battle = BattleManager(database, decks[0], config, definitions, progression, mode, difficulty, rng)
        rng.setstate((3, tuple(stream), gaussian))
        battle.match_id = match_id
        battle.player_deck, battle.ai_deck = decks
        battle.player_values = values
        battle.index, battle.scores, battle.combos = index, scores, combos
        battle.captured, battle.pot, battle.states = captured, pot, clean_states
        battle.history = history
        battle.result = history[-1] if len(history) == index+1 else None
        return battle
