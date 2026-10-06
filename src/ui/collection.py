import pygame
from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL, RED
from src.systems.resources import ATTRIBUTES


class LibraryScreen(Screen):
    """Shared searchable collection, deck editing and upgrade workbench."""
    def __init__(self, app, mode='collection'):
        super().__init__(app)
        self.mode = mode
        self.query, self.category, self.rarity = '', 'All categories', 'All rarities'
        self.sort = 'Name'
        self.page = 0
        self.selected = next(iter(app.manager.database.cards), None)
        self.searching = False

    def filtered(self):
        manager = self.app.manager
        cards = [c for c in manager.database if (str(c.id) in manager.profile['owned_cards'] or self.mode == 'collection')
                 and (not self.query or (str(c.id) in manager.profile['owned_cards'] and self.query.lower() in c.name.lower()))
                 and (self.category == 'All categories' or c.category == self.category)
                 and (self.rarity == 'All rarities' or c.rarity == self.rarity)]
        if self.sort == 'Name':
            cards.sort(key=lambda c: c.name)
        elif self.sort == 'Rarity':
            names = list(manager.rarities)
            cards.sort(key=lambda c: names.index(c.rarity) if c.rarity in names else 0, reverse=True)
        else:
            cards.sort(key=lambda c: manager.profile['card_progress'].get(str(c.id), {}).get('level', 1), reverse=True)
        return cards

    def cycle(self, key, values):
        setattr(self, key, values[(values.index(getattr(self, key))+1)%len(values)])
        self.page = 0

    def event(self, event):
        if self.searching and event.type == pygame.KEYDOWN:
            if event.key in (pygame.K_RETURN, pygame.K_ESCAPE):
                self.searching = False
            elif event.key == pygame.K_BACKSPACE:
                self.query = self.query[:-1]
            self.page = 0
        elif self.searching and event.type == pygame.TEXTINPUT:
            self.query = (self.query + event.text)[:40]
            self.page = 0
        elif event.type == pygame.MOUSEWHEEL:
            self.page = max(0, min(max(0, (len(self.filtered())-1)//8), self.page-event.y))

    def draw(self):
        ui, manager = self.app.ui, self.app.manager
        p = manager.profile
        titles = {'collection': 'Card collection', 'decks': 'Deck workshop', 'upgrades': 'Upgrade forge'}
        ui.header(titles[self.mode], f"{len(p['owned_cards'])} / {len(manager.database.cards)} CHAMPIONS UNLOCKED")
        ui.button(('Search: ' + self.query + ('|' if self.searching else ''))[:34], (36, 106, 282, 38),
                  lambda: setattr(self, 'searching', not self.searching), active=self.searching, size=20)
        ui.button(self.category, (330, 106, 190, 38), lambda: self.cycle('category', ['All categories']+manager.database.categories), size=19)
        ui.button(self.rarity, (530, 106, 161, 38), lambda: self.cycle('rarity', ['All rarities']+list(manager.rarities)), size=19)
        ui.button('Sort: '+self.sort, (701, 106, 164, 38), lambda: self.cycle('sort', ['Name', 'Rarity', 'Level']), size=19)
        deck_name = p['selected_deck']
        deck = manager.decks.decks[deck_name]
        if self.mode == 'decks':
            ui.button(deck_name[:23]+'  >', (36, 157, 245, 37), self.next_deck, size=20)
            ui.button('NEW', (292, 157, 82, 37), lambda: self.app.prompt('New deck name', lambda value: self.deck_action('create', value)), size=19)
            ui.button('RENAME', (384, 157, 107, 37), lambda: self.app.prompt('Rename deck', lambda value: self.deck_action('rename', value)), size=19)
            ui.button('DELETE', (501, 157, 104, 37), lambda: self.app.confirm('Delete this deck?', lambda: self.deck_action('delete')), size=19)
            ui.text(f"{len(deck)}/30  {'VALID DECK' if manager.decks.validate(deck) else 'INVALID DECK'}", 628, 167, 20, TEAL if manager.decks.validate(deck) else RED)
        else:
            ui.text('Select a champion to inspect its attributes and ability.', 40, 171, 21, MUTED)
        cards = self.filtered()
        pages = max(1, (len(cards)+7)//8)
        self.page = min(self.page, pages-1)
        for index, card in enumerate(cards[self.page*8:(self.page+1)*8]):
            x, y = 38+(index%4)*208, 214+(index//4)*211
            rect = pygame.Rect(x, y, 192, 194)
            locked = str(card.id) not in p['owned_cards']
            self.app.card_view.draw(card, rect, hidden=locked, compact=True, selected=card.id==self.selected)
            if self.mode=='decks' and card.id in deck:
                ui.panel((x+130, y+12, 47, 24), (24, 75, 66), TEAL, 5)
                ui.text('IN', x+144, y+16, 16, TEAL, True)
            ui.buttons.append((rect, lambda c=card: setattr(self, 'selected', c.id)))
        ui.button('<', (39, 654, 48, 34), lambda: setattr(self, 'page', self.page-1), enabled=self.page>0)
        ui.text(f'PAGE {self.page+1} / {pages}    •    {len(cards)} CARDS', 111, 664, 19, MUTED)
        ui.button('>', (805, 654, 48, 34), lambda: setattr(self, 'page', self.page+1), enabled=self.page<pages-1)
        ui.panel((897, 105, 347, 584))
        if self.selected is None:
            ui.wrap('No cards available. Restore data/cards.json.', 922, 147, 280)
            return
        card = manager.database[self.selected]
        locked = str(card.id) not in p['owned_cards']
        if locked:
            ui.text('UNDISCOVERED', 923, 141, 28, GOLD, True)
            ui.wrap('Open packs to unlock this champion. Its identity and attributes remain hidden.', 923, 199, 280)
            ui.button('EXPLORE PACKS', (924, 584, 290, 48), lambda: self.app.navigate('packs'), True)
            return
        state = manager.progression.card_state(card.id)
        ui.text(card.name, 919, 128, 28, TEXT, True)
        ui.text(f'{card.rarity.upper()}   /   {card.category}', 919, 163, 18, GOLD)
        ui.wrap(card.description, 919, 193, 295, 20, MUTED, 22)
        ui.text(f"LEVEL {state['level']}   /   {state['xp']} XP", 919, 268, 20, TEAL, True)
        ui.bar((919, 297, 299, 5), state['xp']/manager.progression.threshold(state['level']))
        values = card.values(state, manager.config.get('level_stat_bonus', 1))
        for i, attribute in enumerate(ATTRIBUTES):
            y = 318+i*28
            ui.text(attribute.upper(), 919, y, 18, MUTED)
            ui.text(values[attribute], 1090, y, 21, TEXT, True)
            if self.mode == 'upgrades':
                ui.button('+1', (1152, y-4, 61, 24), lambda a=attribute: self.upgrade(card, a), size=18, enabled=values[attribute]<100)
        ui.text(card.ability.upper(), 919, 551, 22, GOLD, True)
        ui.wrap(manager.ability_data.get(card.ability, {}).get('description', 'No ability effect.'), 919, 578, 295, 19, MUTED, 21)
        if self.mode == 'decks':
            ui.button('REMOVE FROM DECK' if card.id in deck else 'ADD TO DECK', (919, 631, 301, 37), lambda: self.toggle(card.id), True, size=20)
        elif self.mode == 'upgrades':
            ui.text(f"EACH +1 COSTS {manager.config.get('upgrade_cost', 100)} COINS", 919, 652, 18, GOLD)
        else:
            ui.text(f"OWNED: {p['owned_cards'][str(card.id)]}    /    MAX ATTRIBUTE: 100", 919, 652, 17, MUTED)

    def next_deck(self):
        p = self.app.manager.profile
        names = list(p['decks'])
        p['selected_deck'] = names[(names.index(p['selected_deck'])+1)%len(names)]
        self.app.persist()

    def deck_action(self, action, value=None):
        manager = self.app.manager
        name = manager.profile['selected_deck']
        if action=='create':
            manager.decks.create(value)
        elif action=='rename':
            manager.decks.rename(name, value)
        else:
            manager.decks.delete(name)
        self.app.persist()

    def toggle(self, card_id):
        manager = self.app.manager
        manager.decks.toggle(manager.profile['selected_deck'], card_id)
        self.app.persist()

    def upgrade(self, card, attribute):
        self.app.manager.progression.upgrade(card, attribute)
        self.app.persist()
        self.app.notify(f'{attribute.title()} +1  /  Upgrade complete')
        self.app.sound.play('level_up')
