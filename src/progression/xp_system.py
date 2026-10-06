class Progression:
    def __init__(self, profile, config, database=None):
        self.profile, self.config = profile, config
        self.database = database

    def card_state(self, card_id):
        card = self.database[card_id] if self.database else None
        return self.profile['card_progress'].setdefault(str(card_id), {
            'level': card.level if card else 1, 'xp': card.xp if card else 0, 'upgrades': {}})

    def threshold(self, level):
        thresholds = self.config.get('card_xp_thresholds', [100, 250, 500])
        return thresholds[level - 1] if level <= len(thresholds) else thresholds[-1] + (level - len(thresholds)) * 250

    def add_coins(self, amount):
        self.profile['coins'] += amount
        self.profile['statistics']['total_coins_earned'] += amount

    def reward(self, card_id, xp, coins=0):
        p = self.profile
        p['xp'] += xp
        p['statistics']['total_xp'] += xp
        self.add_coins(coins)
        state = self.card_state(card_id)
        state['xp'] += xp
        leveled = False
        while state['xp'] >= self.threshold(state['level']):
            state['xp'] -= self.threshold(state['level'])
            state['level'] += 1
            leveled = True
        while p['xp'] >= p['level'] * self.config.get('player_level_xp', 250):
            p['xp'] -= p['level'] * self.config.get('player_level_xp', 250)
            p['level'] += 1
            self.add_coins(self.config.get('level_reward_coins', 500))
            p['gems'] += self.config.get('level_reward_gems', 20)
            leveled = True
        return leveled

    def upgrade(self, card, attribute):
        if str(card.id) not in self.profile['owned_cards']:
            raise ValueError('Card is locked.')
        state = self.card_state(card.id)
        if attribute not in card.stats:
            raise ValueError('Unknown attribute.')
        if card.values(state, self.config.get('level_stat_bonus', 1))[attribute] >= 100:
            raise ValueError('Attribute is already at maximum.')
        cost = self.config.get('upgrade_cost', 100)
        if self.profile['coins'] < cost:
            raise ValueError('Not enough coins.')
        self.profile['coins'] -= cost
        state['upgrades'][attribute] = state['upgrades'].get(attribute, 0) + 1
