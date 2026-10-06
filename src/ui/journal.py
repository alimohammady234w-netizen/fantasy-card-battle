from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL, RED


def outcome(scores):
    return 'WIN' if scores[0] > scores[1] else 'LOSS' if scores[0] < scores[1] else 'TIE'


class JournalScreen(Screen):
    """Read-only, paginated presentation of revealed rounds from completed matches."""
    def __init__(self, app):
        super().__init__(app)
        self.filter = 'ALL'
        self.page = 0
        self.round_page = 0
        self.selected = None

    def entries(self):
        return [entry for entry in reversed(self.app.manager.profile['match_history'])
                if self.filter == 'ALL' or outcome(entry['scores']) == self.filter]

    def select(self, match_id):
        self.selected = match_id
        self.round_page = 0

    def cycle_filter(self):
        choices = ('ALL', 'WIN', 'LOSS', 'TIE')
        self.filter = choices[(choices.index(self.filter)+1) % len(choices)]
        self.page = self.round_page = 0
        self.selected = None

    def draw(self):
        ui = self.app.ui
        ui.header('Match journal', 'LAST 50 COMPLETED MATCHES  /  REVEALED INFORMATION ONLY  /  DATES IN UTC')
        entries = self.entries()
        pages = max(1, (len(entries)+7)//8)
        self.page = min(self.page, pages-1)
        ui.button('RESULT: '+self.filter+'  >', (36, 108, 336, 37), self.cycle_filter, size=20)
        if not entries:
            ui.panel((396, 107, 846, 571))
            ui.text('A STORY YET TO BE WRITTEN', 818, 257, 33, GOLD, True, center=True)
            ui.wrap('Finish a Quick or Classic Match to record every revealed round here. Practice and unfinished matches are not recorded. Reading the journal never changes your rewards.', 550, 327, 551, 25, MUTED, 31)
            ui.button('ENTER THE ARENA', (655, 504, 329, 49), lambda: self.app.navigate('play'), True)
            return
        if self.selected not in [entry['id'] for entry in entries]:
            self.select(entries[0]['id'])
        for i, entry in enumerate(entries[self.page*8:(self.page+1)*8]):
            y = 159+i*59
            scores = entry['scores']
            color = TEAL if outcome(scores)=='WIN' else RED if outcome(scores)=='LOSS' else GOLD
            ui.button('', (36, y, 336, 52), lambda key=entry['id']: self.select(key), active=entry['id']==self.selected)
            ui.text(entry['mode'][:22], 49, y+7, 21, TEXT, True)
            ui.text(f"{scores[0]}:{scores[1]}  {outcome(scores)}", 265, y+9, 18, color)
            ui.text(entry['completed_at'][:16].replace('T', ' ')+' UTC', 49, y+31, 17, MUTED)
        ui.button('<', (36, 643, 48, 34), lambda: setattr(self, 'page', self.page-1), enabled=self.page>0)
        ui.text(f'MATCHES {self.page+1}/{pages}', 105, 654, 18, MUTED)
        ui.button('>', (324, 643, 48, 34), lambda: setattr(self, 'page', self.page+1), enabled=self.page<pages-1)
        entry = next(e for e in entries if e['id']==self.selected)
        ui.panel((396, 107, 846, 111))
        scores = entry['scores']
        ui.text(f"{outcome(scores)}   /   {entry['mode']}   /   {entry['difficulty']} AI", 418, 125, 28, GOLD, True)
        ui.text(f"{scores[0]} : {scores[1]} CARDS     +{entry['xp']} XP     +{entry['coins']} ROUND COINS", 418, 169, 22, TEAL)
        rounds = entry['rounds']
        round_pages = max(1, (len(rounds)+4)//5)
        self.round_page = min(self.round_page, round_pages-1)
        for i, row in enumerate(rounds[self.round_page*5:(self.round_page+1)*5]):
            y = 231+i*80
            ui.panel((396, y, 846, 73))
            winner = 'TIE' if row['winner'] is None else 'YOU WIN' if row['winner']==0 else 'AI WINS'
            color = GOLD if row['winner'] is None else TEAL if row['winner']==0 else RED
            ui.text(f"{row['round']:02}   {row['player'][:25]}  /  {row['opponent'][:25]}", 413, y+9, 22, TEXT, True)
            ui.text(winner, 1140, y+11, 19, color, True)
            raw, strength = row['raw'], row['strength']
            ui.text(f"{row['chooser'].upper()}: {row['attribute'].upper()} {row['direction'].upper()}   |   BASE {raw[0]:g}:{raw[1]:g}   >   STRENGTH {strength[0]:g}:{strength[1]:g}   |   +{row['captured']} CARDS", 413, y+33, 18, MUTED)
            effects = ' / '.join(row['abilities']) or 'No active abilities'
            ui.text(effects[:113], 413, y+53, 17, GOLD)
        ui.button('<', (398, 643, 48, 34), lambda: setattr(self, 'round_page', self.round_page-1), enabled=self.round_page>0)
        ui.text(f'ROUNDS {self.round_page*5+1}-{min(len(rounds), (self.round_page+1)*5)} / {len(rounds)}', 470, 654, 18, MUTED)
        ui.button('>', (1193, 643, 48, 34), lambda: setattr(self, 'round_page', self.round_page+1), enabled=self.round_page<round_pages-1)
