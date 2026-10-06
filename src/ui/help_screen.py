from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL


class HelpScreen(Screen):
    """An in-game rulebook; numeric examples do not award or mutate progression."""
    TABS = ('THE FIRST ROUND', 'ABILITIES & AI', 'DECKS & REWARDS')

    def __init__(self, app):
        super().__init__(app)
        self.tab = 0
        self.direction = 'Greater'
        self.ability_index = 0

    def block(self, x, title, body, footer=''):
        ui = self.app.ui
        ui.panel((x, 197, 382, 423))
        ui.text(title, x+23, 224, 27, GOLD, True)
        ui.wrap(body, x+23, 275, 335, 25, MUTED, 30)
        if footer:
            ui.wrap(footer, x+23, 510, 332, 23, TEAL, 27)

    def draw(self):
        ui, manager = self.app.ui, self.app.manager
        config = manager.config
        ui.header('Field guide', 'LEARN THE RULES  /  NO CARDS OR CURRENCY ARE SPENT HERE')
        for i, title in enumerate(self.TABS):
            ui.button(title, (40+i*406, 121, 382, 47), lambda index=i: setattr(self, 'tab', index),
                      active=self.tab==i, size=22)
        if self.tab==0:
            self.block(40, '01  DRAW & CHOOSE',
                       'You see your champion. The AI card stays hidden. Select one of eight attributes, then choose Greater or Less. There is no universally best attribute: low values can win too.',
                       'Keys 1-8 select an attribute. G = Greater. L = Less.')
            self.block(446, '02  TRY A COMPARISON',
                       'Example without abilities: your Power is 85 and the opponent has 72. Change the rule below and see who wins.')
            ui.text('85   :   72', 637, 402, 46, TEXT, True, center=True)
            ui.button('GREATER', (469, 471, 157, 42), lambda: setattr(self, 'direction', 'Greater'), active=self.direction=='Greater', size=20)
            ui.button('LESS', (642, 471, 157, 42), lambda: setattr(self, 'direction', 'Less'), active=self.direction=='Less', size=20)
            ui.text('YOU WIN' if self.direction=='Greater' else 'AI WINS', 637, 543, 28, TEAL, True, center=True)
            ties = {'carry': 'Ties carry both cards into the next round.', 'none': 'Tied cards are discarded without a winner.', 'random': 'Ties choose a random winner.'}
            self.block(852, '03  REVEAL & RESOLVE',
                       'Abilities resolve after the reveal. Greater uses the attribute value; Less uses 101 minus that value. The higher final comparison strength wins both cards. '+ties[config['tie_rule']],
                       'Captured cards are match points, not permanent collection unlocks.')
        elif self.tab==1:
            self.block(40, 'ABILITY REFERENCE', 'Use the buttons below to browse the abilities loaded from your database.')
            names = list(manager.ability_data)
            if names:
                self.ability_index %= len(names)
                name = names[self.ability_index]
                ui.text(name.upper(), 64, 368, 26, TEXT, True)
                ui.wrap(manager.ability_data[name].get('description', 'No description.'), 64, 412, 330, 23, MUTED, 28)
                ui.button('<', (64, 553, 68, 40), lambda: setattr(self, 'ability_index', (self.ability_index-1)%len(names)))
                ui.text(f'{self.ability_index+1} / {len(names)}', 209, 565, 20, TEAL, center=True)
                ui.button('>', (329, 553, 68, 40), lambda: setattr(self, 'ability_index', (self.ability_index+1)%len(names)))
            else:
                ui.text('No abilities loaded.', 64, 400, 24, MUTED)
            self.block(446, 'TIMING MATTERS',
                       'Silence prevents an opposing ability. Freeze and Poison affect later rounds. Copy and Mirror copy an opposing effect without recursive loops. Toggle your own ability before the reveal.',
                       'HP starts at 100. In these modes, HP is not an elimination condition.')
            self.block(852, 'AN HONEST OPPONENT',
                       'The AI receives only its own card and public information. Normal and Hard favor extreme values; Expert estimates odds from the public database. Classic alternates the choice of attribute and direction.',
                       'When the timer expires, your move is chosen using only your own card.')
        else:
            self.block(40, 'BUILD YOUR DECK',
                       'A valid deck has exactly 30 unique owned cards. Open packs to discover new champions. Search, filter and sort in Deck Builder, then add or remove cards. Choose the deck on the Play screen.',
                       f"Every +1 attribute upgrade costs {config['upgrade_cost']} coins. Attributes cap at 100.")
            self.block(446, 'EARN & IMPROVE',
                       f"A round win gives {config['win_xp']} XP and {config['win_coins']} coins. A loss or tie gives {config['round_xp']} XP. Every {config['combo_xp_every']} consecutive wins gives bonus XP; every {config['combo_coins_every']} gives bonus coins.",
                       'Pack duplicate protection works within each rarity, not across all rarities.')
            quick = config['modes']['Quick Match']['rounds']
            classic = config['modes']['Classic Match']['rounds']
            self.block(852, 'PLAY YOUR WAY',
                       f'Quick Match: {quick} rounds with your choices. Classic: {classic} rounds with alternating turns. Practice awards no progression or records. Finished Quick and Classic battles appear in Match Journal.',
                       'Progress saves automatically. Use PLAY > RESUME to continue a suspended match. The choice timer restarts.')
        ui.button('READY TO PLAY  >', (922, 651, 310, 42), lambda: self.app.navigate('play'), True, size=21)
        ui.text('ESC: MAIN MENU', 44, 666, 18, MUTED)
