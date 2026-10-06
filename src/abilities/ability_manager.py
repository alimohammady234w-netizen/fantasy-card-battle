"""Data-driven effects run on direction-normalized comparison strength.
Effects are planned from the same initial state, then applied together.
"""
import random


class AbilityManager:
    def __init__(self, definitions, rng=None):
        self.definitions = definitions
        self.rng = rng or random.Random()
        self.handlers = {
            'boost': self.boost, 'debuff': self.debuff, 'heal': self.heal,
            'freeze': self.status, 'poison': self.status, 'critical': self.critical,
            'counter': self.counter, 'revive': self.revive, 'dodge': self.dodge,
            'rage': self.rage, 'silence': self.noop, 'mirror': self.noop, 'copy': self.noop}

    def register(self, effect, handler):
        self.handlers[effect] = handler

    def resolve(self, names, strengths, values, states, enabled=(True, True)):
        definitions = [self.definitions.get(name, {}) if enabled[i] else {} for i, name in enumerate(names)]
        # Copy/Mirror never recurse. Two reflectors have no effect.
        original = list(definitions)
        for i in range(2):
            if definitions[i].get('effect') in ('mirror', 'copy'):
                other = original[1-i]
                definitions[i] = other if other.get('effect') not in ('mirror', 'copy') else {}
        silenced = [definitions[1-i].get('effect') == 'silence' for i in range(2)]
        delta, messages = [0., 0.], []
        for i in range(2):
            definition = definitions[i]
            if not definition or silenced[i]:
                if silenced[i]:
                    messages.append(('Player' if i == 0 else 'AI') + ' is silenced')
                continue
            handler = self.handlers.get(definition.get('effect'))
            if handler:
                handler(i, definition, strengths, values, states, delta)
                messages.append(('Player' if i == 0 else 'AI') + ': ' + names[i])
        return [strengths[i] + delta[i] for i in range(2)], messages

    def noop(self, i, d, strengths, values, states, delta):
        pass

    def boost(self, i, d, strengths, values, states, delta):
        delta[i] += d.get('amount', 0)

    def debuff(self, i, d, strengths, values, states, delta):
        delta[1-i] -= d.get('amount', 0)

    def heal(self, i, d, strengths, values, states, delta):
        if states[i]['hp'] == 100:
            delta[i] += 2
        states[i]['hp'] = min(100, states[i]['hp'] + d.get('amount', 12))

    def status(self, i, d, strengths, values, states, delta):
        states[1-i]['statuses'].append({'amount': d.get('amount', 0), 'turns': d.get('duration', 1)})

    def critical(self, i, d, strengths, values, states, delta):
        if self.rng.random() < d.get('chance', .35):
            self.boost(i, d, strengths, values, states, delta)

    def counter(self, i, d, strengths, values, states, delta):
        if strengths[i] < strengths[1-i]:
            self.boost(i, d, strengths, values, states, delta)

    def revive(self, i, d, strengths, values, states, delta):
        if states[i]['lost_last']:
            self.boost(i, d, strengths, values, states, delta)

    def dodge(self, i, d, strengths, values, states, delta):
        if self.rng.random() < d.get('chance', .4):
            self.debuff(i, d, strengths, values, states, delta)

    def rage(self, i, d, strengths, values, states, delta):
        if values[i]['power'] >= d.get('threshold', 80):
            self.boost(i, d, strengths, values, states, delta)
