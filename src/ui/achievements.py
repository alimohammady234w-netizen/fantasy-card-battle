"""Paginated, read-only progress display with explicit, atomically saved claims."""
from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL


class AchievementsScreen(Screen):
    FILTERS = ('ALL', 'READY', 'CLAIMED', 'LOCKED')

    def __init__(self, app):
        super().__init__(app)
        self.status_filter = 'ALL'
        self.group = 'All groups'
        self.page = 0

    @property
    def service(self):
        return self.app.manager.achievements

    def items(self):
        return [(key, definition, self.service.progress(key))
                for key, definition in self.service.definitions.items()
                if (self.group == 'All groups' or definition['group'] == self.group)
                and (self.status_filter == 'ALL' or self.service.progress(key).status == self.status_filter)]

    def cycle_filter(self):
        self.status_filter = self.FILTERS[(self.FILTERS.index(self.status_filter)+1) % len(self.FILTERS)]
        self.page = 0

    def cycle_group(self):
        groups = ['All groups']+sorted({row['group'] for row in self.service.definitions.values()})
        self.group = groups[(groups.index(self.group)+1) % len(groups)]
        self.page = 0

    def claim(self, achievement_id):
        rewards = self.app.manager.claim_achievement(achievement_id)
        self.app.sound.play('level_up')
        self.app.notify(f"Reward saved: +{rewards['coins']} coins / +{rewards['gems']} gems")

    def draw(self):
        ui = self.app.ui
        service = self.service
        ui.header('Achievements', 'LIFETIME MILESTONES  /  OFFLINE PROGRESS  /  EACH REWARD CLAIMED ONCE')
        total, claimed, ready = len(service.definitions), service.claimed_count, service.ready_count
        ui.panel((40, 108, 1194, 82))
        ui.text(f'{claimed:02} / {total:02} CLAIMED', 63, 126, 31, GOLD, True)
        ui.text(f'{ready} REWARD'+('S' if ready != 1 else '')+' READY', 440, 130, 27, TEAL, True)
        ui.text('Play. Progress. Claim your reward.', 843, 128, 23, MUTED)
        ui.bar((844, 163, 364, 6), claimed/max(1, total))
        ui.button('STATUS: '+self.status_filter+'  >', (40, 207, 251, 36), self.cycle_filter, size=20)
        ui.button(self.group+'  >', (307, 207, 245, 36), self.cycle_group, size=20)
        ui.text('C = COINS    /    G = GEMS', 989, 218, 18, MUTED)
        items = self.items()
        pages = max(1, (len(items)+5)//6)
        self.page = min(self.page, pages-1)
        if not items:
            ui.panel((40, 261, 1194, 362))
            ui.text('NO MILESTONES HERE YET', 637, 355, 34, GOLD, True, center=True)
            message = ('Restore data/achievements.json to enable achievements.' if not total else
                       'Try another filter, or complete matches, open packs and improve your collection.')
            ui.wrap(message, 342, 417, 600, 25, MUTED, 30)
        for index, (key, row, state) in enumerate(items[self.page*6:(self.page+1)*6]):
            x, y = 40+(index%3)*406, 261+(index//3)*190
            color = TEAL if state.ready else GOLD if state.claimed else MUTED
            ui.panel((x, y, 382, 172), border=TEAL if state.ready else None)
            ui.text(row['title'][:25], x+21, y+16, 26, TEXT, True)
            ui.text(state.status, x+299, y+20, 16, color, True)
            # Keep externally-authored descriptions inside their allocated area.
            old_clip = self.app.canvas.get_clip()
            self.app.canvas.set_clip((x+21, y+46, 340, 53))
            ui.wrap(row['description'], x+21, y+46, 340, 20, MUTED, 20)
            self.app.canvas.set_clip(old_clip)
            ui.text(f'{min(state.current, state.target):,} / {state.target:,}', x+21, y+102, 20, color)
            ui.bar((x+21, y+123, 339, 5), state.ratio, color)
            reward = row['rewards']
            ui.text(f"+{reward['coins']:,} C / +{reward['gems']:,} G", x+21, y+144, 18, GOLD)
            ui.button('CLAIM' if state.ready else 'CLAIMED' if state.claimed else 'LOCKED',
                      (x+250, y+135, 111, 28), lambda identity=key: self.claim(identity),
                      primary=state.ready, enabled=state.ready, size=18)
        ui.button('<', (40, 653, 48, 35), lambda: setattr(self, 'page', self.page-1), enabled=self.page>0)
        ui.text(f'PAGE {self.page+1} / {pages}    /    {len(items)} MILESTONES', 109, 665, 19, MUTED)
        ui.text('Rewards are saved before they are credited.', 690, 665, 19, MUTED)
        ui.button('>', (1186, 653, 48, 35), lambda: setattr(self, 'page', self.page+1), enabled=self.page<pages-1)
