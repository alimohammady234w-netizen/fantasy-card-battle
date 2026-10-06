import pygame
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL, EDGE


class Screen:
    def __init__(self, app):
        self.app = app

    def event(self, event):
        pass

    def update(self, dt):
        pass


class MenuScreen(Screen):
    def draw(self):
        app, ui, manager = self.app, self.app.ui, self.app.manager
        ui.text('A R C A N A', 56, 40, 31, GOLD, True)
        ui.text('REALMS IN CONFLICT', 58, 76, 16, MUTED)
        p = manager.profile
        ui.text(f"LEVEL {p['level']:02}   /   {p['coins']:,} COINS   /   {p['gems']:,} GEMS", 822, 52, 21, GOLD)
        ready = manager.achievements.ready_count
        ui.button(f'ACHIEVEMENTS  /  {ready} READY', (858, 79, 364, 25),
                  lambda: app.navigate('achievements'), active=ready>0, size=17)
        pygame.draw.line(app.canvas, EDGE, (56, 110), (1224, 110))
        ui.text('STRATEGY IS YOUR', 66, 151, 18, TEAL, True)
        ui.text('Greatest', 62, 185, 76, TEXT, True)
        ui.text('power.', 62, 247, 76, GOLD, True)
        ui.wrap('Across fifteen realms, every champion has a weakness. Choose your attribute. Change the odds.', 66, 328, 390, 24, MUTED, 29)
        ui.button('PLAY   /   SAVED MATCH AVAILABLE' if manager.can_resume else 'PLAY   /   ENTER THE ARENA', (66, 427, 378, 53), lambda: app.navigate('play'), True)
        for i, (label, route) in enumerate([('DECK BUILDER', 'decks'), ('COLLECTION', 'collection'),
                                           ('UPGRADES', 'upgrades'), ('PACKS', 'packs'),
                                           ('SETTINGS', 'settings'), ('EXIT', 'exit')]):
            ui.button(label, (66+(i%2)*195, 492+(i//2)*49, 183, 39),
                      lambda r=route: app.navigate(r), size=20)
        ui.button('MATCH JOURNAL', (66, 642, 183, 28), lambda: app.navigate('journal'), size=18)
        ui.button('HOW TO PLAY', (261, 642, 183, 28), lambda: app.navigate('help'), size=18)
        cards = list(manager.database)
        if cards:
            offset = int(30*(1-app.animation.ease(app.animation.age/.6)))
            app.card_view.draw(cards[min(29, len(cards)-1)], (526, 192+offset, 267, 445))
            app.card_view.draw(cards[min(34, len(cards)-1)], (843, 155+offset, 282, 445), selected=True)
            ui.text(f'{len(cards)} CHAMPIONS    /    {len(manager.database.categories)} REALMS    /    INFINITE POSSIBILITIES', 568, 654, 18, MUTED)
        else:
            ui.wrap('Card database is unavailable. Restore data/cards.json to start playing.', 600, 300, 420, 28)
        ui.text('SINGLE PLAYER  •  OFFLINE', 66, 679, 16, MUTED)
        ui.text('v1.0   /   THE FIRST AGE', 1050, 679, 16, MUTED)


class PlayScreen(Screen):
    def __init__(self, app):
        super().__init__(app)
        self.mode = 'Quick Match'

    def draw(self):
        ui, manager = self.app.ui, self.app.manager
        ui.header('Enter the arena', '01  SELECT MODE     /     02  SELECT DECK     /     03  BATTLE')
        descriptions = {'Quick Match': 'Ten rounds. You choose every comparison. Fast battles, full rewards.',
                        'Classic Match': 'Thirty rounds. Alternate selection with the AI. Every card matters.',
                        'Practice': 'Ten rounds to experiment. No rewards or changes to your statistics.'}
        for i, mode in enumerate(manager.config['modes']):
            x = 48+i*405
            ui.panel((x, 137, 380, 224))
            ui.text(f'0{i+1}', x+24, 160, 45, GOLD, True)
            ui.text(mode.upper(), x+24, 216, 28, TEXT, True)
            ui.wrap(descriptions.get(mode, 'A custom arena mode.'), x+24, 255, 320)
            ui.button('SELECTED' if mode==self.mode else 'SELECT MODE', (x+20, 320, 340, 32),
                      lambda m=mode: setattr(self, 'mode', m), active=mode==self.mode, size=18)
        name = manager.profile['selected_deck']
        ids = manager.decks.decks[name]
        valid = manager.decks.validate(ids)
        ui.text('YOUR DECK', 55, 401, 19, TEAL, True)
        ui.text(name, 55, 435, 36, TEXT, True)
        ui.text(f"{len(ids)} / 30 CARDS  •  {'VALID DECK' if valid else 'INVALID DECK'}", 55, 480, 22, GOLD)
        ui.button('CHANGE DECK', (55, 530, 215, 42), self.cycle_deck)
        ui.button('EDIT DECK', (288, 530, 180, 42), lambda: self.app.navigate('decks'))
        difficulty = manager.profile['settings']['difficulty']
        ui.text('NEW MATCH OPPONENT', 690, 413, 20, TEAL, True)
        ui.text(f'{difficulty} AI', 690, 450, 39, TEXT, True)
        ui.wrap('The opponent sees only its own card and public information. No hidden-card access.', 690, 500, 430)
        if manager.can_resume:
            battle = manager.battle
            ui.text(f'SAVED: {battle.mode_name} / {battle.ai.difficulty} AI', 55, 587, 19, TEAL)
            ui.text('Unresolved choices restart their timer on resume.', 55, 682, 18, MUTED)
            ui.button(f'RESUME  /  ROUND {battle.index+1:02}', (55, 614, 330, 56), self.app.resume_match, active=True)
            ui.button('DISCARD', (401, 622, 120, 40), self.discard, size=20)
        ui.button('START MATCH  >', (870, 614, 350, 56), self.start, True, enabled=valid)

    def cycle_deck(self):
        p = self.app.manager.profile
        names = list(p['decks'])
        p['selected_deck'] = names[(names.index(p['selected_deck'])+1)%len(names)]
        self.app.persist()

    def discard(self):
        self.app.confirm('Discard saved match? Completed-round rewards remain.', self.discard_confirmed)

    def discard_confirmed(self):
        if not self.app.manager.discard_match():
            self.app.notify('Save failed. Check folder permissions. See game.log.')

    def start(self):
        if self.app.manager.can_resume:
            self.app.confirm('Replace saved match? Completed-round rewards remain.', self.begin)
        else:
            self.begin()

    def begin(self):
        self.app.manager.start(self.mode)
        self.app.persist()
        self.app.navigate('battle')
