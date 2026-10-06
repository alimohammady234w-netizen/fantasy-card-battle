class DeckManager:
    def __init__(self, profile, database, size=30):
        self.profile, self.database, self.size = profile, database, size

    @property
    def decks(self):
        return self.profile['decks']

    def validate(self, ids):
        return (len(ids) == self.size and len(set(ids)) == self.size and
                all(i in self.database.cards and str(i) in self.profile['owned_cards'] for i in ids))

    def create(self, name):
        name = name.strip()[:40]
        if not name or name in self.decks:
            raise ValueError('Choose a unique deck name.')
        self.decks[name] = []
        self.profile['selected_deck'] = name

    def rename(self, old, new):
        new = new.strip()[:40]
        if not new or new in self.decks:
            raise ValueError('Choose a unique deck name.')
        self.decks[new] = self.decks.pop(old)
        self.profile['selected_deck'] = new

    def delete(self, name):
        if len(self.decks) == 1:
            raise ValueError('Keep at least one deck.')
        del self.decks[name]
        self.profile['selected_deck'] = next(iter(self.decks))

    def toggle(self, name, card_id):
        deck = self.decks[name]
        if card_id in deck:
            deck.remove(card_id)
        elif str(card_id) not in self.profile['owned_cards']:
            raise ValueError('Unlock this card in a pack first.')
        elif len(deck) >= self.size:
            raise ValueError('Deck is full. Remove a card first.')
        else:
            deck.append(card_id)
