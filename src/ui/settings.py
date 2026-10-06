from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEAL


class SettingsScreen(Screen):
    OPTIONS = {'volume': [0., .2, .4, .6, .8, 1.], 'music_volume': [0., .2, .4, .6, .8, 1.],
               'sound_volume': [0., .2, .4, .6, .8, 1.], 'fullscreen': [False, True],
               'resolution': [[1280, 720], [1920, 1080]], 'animation_speed': [.5, 1., 1.5, 2.],
               'difficulty': ['Easy', 'Normal', 'Hard', 'Expert'], 'timer': [0, 10, 15, 30, 60],
               'language': ['English']}

    def change(self, key):
        settings = self.app.manager.profile['settings']
        values = self.OPTIONS[key]
        current = settings[key]
        index = values.index(current) if current in values else -1
        settings[key] = values[(index+1)%len(values)]
        if key in ('resolution', 'fullscreen'):
            self.app.set_display()
        self.app.sound.refresh_volumes()
        self.app.persist()

    def draw(self):
        ui, manager = self.app.ui, self.app.manager
        ui.header('Settings & records', 'CHANGES ARE SAVED AUTOMATICALLY')
        settings = manager.profile['settings']
        for i, key in enumerate(self.OPTIONS):
            y = 120+i*59
            ui.text(key.replace('_', ' ').upper(), 60, y+12, 22, MUTED)
            value = settings[key]
            if key.endswith('volume') or key=='volume':
                label = f'{int(value*100)}%'
            elif key=='fullscreen':
                label = 'ON' if value else 'OFF'
            elif key=='resolution':
                label = f'{value[0]} x {value[1]}'
            elif key=='timer':
                label = f'{value} SECONDS' if value else 'OFF'
            else:
                label = str(value).upper()
            ui.button(label+'  >' if key!='language' else label, (317, y, 292, 42), lambda k=key: self.change(k), size=22)
        ui.panel((687, 120, 545, 545))
        ui.text('YOUR JOURNEY', 716, 145, 27, GOLD, True)
        ui.button('ACHIEVEMENTS', (991, 142, 215, 35), lambda: self.app.navigate('achievements'), size=19)
        for i, (key, value) in enumerate(manager.profile['statistics'].items()):
            ui.text(key.replace('_', ' ').title(), 716, 202+i*37, 22, MUTED)
            ui.text(f'{value:,}', 1120, 202+i*37, 23, TEAL, True)
        ui.text('English UI • Additional languages can be added through localization.', 60, 683, 18, MUTED)
