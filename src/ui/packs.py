from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL


class PackScreen(Screen):
    def __init__(self, app):
        super().__init__(app)
        self.rewards = None
        self.age = 0.
        self.page = 0
        self.catalog_page = 0

    def update(self, dt):
        self.age += dt*self.app.manager.profile['settings']['animation_speed']

    def open_pack(self, name):
        if self.rewards is not None:
            raise ValueError('Return to the vault before opening another pack.')
        self.rewards = self.app.manager.packs.open(name)
        self.app.persist()
        self.app.sound.play('pack_open')
        self.age, self.page = 0., 0

    def draw(self):
        ui, manager = self.app.ui, self.app.manager
        ui.header('The vault', 'DISCOVER NEW CHAMPIONS  /  DUPLICATE PROTECTION WITHIN EACH RARITY')
        if self.rewards is not None:
            ui.text('YOUR NEW CHAMPIONS', 640, 139, 32, GOLD, True, center=True)
            rewards = self.rewards[self.page*5:(self.page+1)*5]
            for i, (card, duplicate) in enumerate(rewards):
                x = 45+i*245
                offset = int(50*(1-self.app.animation.ease(max(0, self.age-i*.15)/.5)))
                self.app.card_view.draw(card, (x, 218+offset, 215, 291), compact=True, hidden=self.age<i*.15)
                if self.age>=i*.15:
                    ui.text(f'DUPLICATE  /  +{manager.packs.duplicate_coins} COINS' if duplicate else 'NEW CHAMPION', x+107, 535, 18, MUTED if duplicate else TEAL, center=True)
            if len(self.rewards)>5:
                ui.button('MORE CARDS  >' if self.page==0 else '<  FIRST CARDS', (470, 579, 340, 39), lambda: setattr(self, 'page', 1-self.page))
            ui.button('RETURN TO VAULT', (470, 637, 340, 47), lambda: setattr(self, 'rewards', None), True)
            return
        packs = list(manager.pack_data.items())
        pages = max(1, (len(packs)+5)//6)
        self.catalog_page = min(self.catalog_page, pages-1)
        if not packs:
            ui.text('NO PACKS AVAILABLE', 640, 300, 32, GOLD, True, center=True)
            ui.text('Check data/packs.json and game.log. Your currency has not been changed.', 640, 350, 23, MUTED, center=True)
        for i, (name, pack) in enumerate(packs[self.catalog_page*6:(self.catalog_page+1)*6]):
            x, y = 42+(i%3)*412, 127+(i//3)*252
            ui.panel((x, y, 383, 231))
            ui.text(f'0{i+1}  /  {name.upper()}', x+23, y+24, 31, GOLD, True)
            ui.text(f"{pack['count']} CARDS", x+23, y+68, 21, TEXT)
            weights = pack.get('weights', {k: v['weight'] for k,v in manager.rarities.items()})
            total = sum(weights.values()) or 1
            details = '  /  '.join(f'{k[:3]} {100*v/total:g}%' for k,v in weights.items() if v)
            ui.wrap(details, x+23, y+103, 339, 18, MUTED, 23)
            currency = pack.get('currency', 'gems')
            ui.button(f"OPEN  /  {pack['price']} {currency.upper()}", (x+23, y+175, 337, 39),
                      lambda n=name: self.open_pack(n), True, enabled=manager.profile[currency]>=pack['price'], size=21)
        if pages > 1:
            ui.button('<', (42, 655, 46, 32), lambda: setattr(self, 'catalog_page', self.catalog_page-1), enabled=self.catalog_page>0)
            ui.button('>', (1190, 655, 46, 32), lambda: setattr(self, 'catalog_page', self.catalog_page+1), enabled=self.catalog_page<pages-1)
        ui.text(f'Duplicates become +{manager.packs.duplicate_coins} coins. Finish matches and level up to earn gems.', 640, 667, 21, MUTED, center=True)
