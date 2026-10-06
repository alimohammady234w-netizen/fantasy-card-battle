import random


class PackSystem:
    def __init__(self, database, profile, packs, rarities, rng=None, duplicate_coins=50):
        self.database, self.profile = database, profile
        self.packs, self.rarities = packs, rarities
        self.rng = rng or random.Random()
        self.duplicate_coins = duplicate_coins

    def open(self, name):
        pack = self.packs[name]
        currency = pack.get('currency', 'gems')
        if self.profile[currency] < pack['price']:
            raise ValueError('Not enough ' + currency + '.')
        pools = {rarity: [c for c in self.database if c.rarity == rarity] for rarity in self.rarities}
        weights = pack.get('weights', {k: v['weight'] for k, v in self.rarities.items()})
        available = [k for k in pools if pools[k] and weights.get(k, 0) > 0]
        if not available:
            raise ValueError('No eligible cards in the database.')
        self.profile[currency] -= pack['price']
        rewards = []
        for _ in range(pack['count']):
            rarity = self.rng.choices(available, weights=[weights[k] for k in available])[0]
            pool = pools[rarity]
            fresh = [c for c in pool if str(c.id) not in self.profile['owned_cards']]
            card = self.rng.choice(fresh or pool)
            key = str(card.id)
            duplicate = key in self.profile['owned_cards']
            self.profile['owned_cards'][key] = self.profile['owned_cards'].get(key, 0) + 1
            if not duplicate:
                self.profile['unlocked_cards'].append(card.id)
            else:
                self.profile['coins'] += self.duplicate_coins
                self.profile['statistics']['total_coins_earned'] += self.duplicate_coins
            rewards.append((card, duplicate))
        self.profile['statistics']['total_packs_opened'] += 1
        return rewards
