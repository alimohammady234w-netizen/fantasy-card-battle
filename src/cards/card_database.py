import logging
from src.cards.card import Card
from src.systems.resources import ROOT, read_json


class CardDatabase:
    def __init__(self, path=None):
        self.cards = {}
        rows = read_json(path or ROOT / 'data/cards.json', [])
        if not isinstance(rows, list):
            rows = []
        for row in rows:
            try:
                card = Card.from_dict(row)
                if card.id in self.cards:
                    raise ValueError('Duplicate card ID')
                self.cards[card.id] = card
            except (ValueError, KeyError, TypeError, AttributeError, OverflowError) as error:
                logging.warning('Skipping invalid card: %s', error)

    def __getitem__(self, card_id):
        return self.cards[int(card_id)]

    def __iter__(self):
        return iter(self.cards.values())

    @property
    def categories(self):
        return sorted({card.category for card in self})
