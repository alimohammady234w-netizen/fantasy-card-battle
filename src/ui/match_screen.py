import math
import pygame
from src.ai.ai_player import AIPlayer
from src.ui.menu import Screen
from src.ui.widgets import GOLD, MUTED, TEXT, TEAL, RED, EDGE
from src.systems.resources import ATTRIBUTES


class MatchScreen(Screen):
    def __init__(self, app):
        super().__init__(app)
        self.battle = app.manager.battle
        self.attribute = self.battle.result.attribute if self.battle.result else None
        self.use_ability = True
        self.elapsed, self.reveal_time = 0., 0.
        self.pending = None
        self.revealing = False
        self.timer = app.manager.profile['settings']['timer']

    def choose(self, direction):
        if self.revealing or self.battle.result:
            return
        if self.attribute is None and not self.battle.ai_turn:
            raise ValueError('Select an attribute first.')
        self.pending = (self.attribute, direction)
        self.revealing, self.reveal_time = True, 0.
        self.app.sound.play('flip')

    def update(self, dt):
        self.elapsed += dt
        if self.revealing:
            self.reveal_time += dt * self.app.manager.profile['settings']['animation_speed']
            if self.reveal_time >= .30 and not self.battle.result:
                result = self.battle.resolve(*self.pending, self.use_ability)
                self.attribute = result.attribute
                self.app.persist()
                self.app.sound.play('reveal')
                for card in self.battle.cards:
                    self.app.sound.play_file(card.sound)
                if result.abilities:
                    self.app.sound.play('ability')
                self.app.sound.play('tie' if result.winner is None else 'win' if result.winner==0 else 'lose')
                if result.leveled:
                    self.app.sound.play('level_up')
            if self.reveal_time >= .65:
                self.revealing = False
        elif not self.battle.result:
            if self.battle.ai_turn and self.elapsed >= 1.5:
                self.choose('Greater')
            elif self.timer and self.elapsed >= self.timer:
                decision = AIPlayer('Normal').choose(self.battle.values()[0])
                self.attribute = decision.attribute
                self.choose(decision.direction)

    def next_round(self):
        self.battle.next_round()
        self.app.persist()
        if self.battle.finished:
            self.app.navigate('result')
        else:
            self.attribute, self.pending = None, None
            self.elapsed, self.reveal_time = 0., 0.
            self.revealing = False
            self.app.animation.reset()

    def event(self, event):
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_RETURN and self.battle.result and not self.revealing:
                self.next_round()
            elif not self.battle.result and not self.revealing and not self.battle.ai_turn:
                if pygame.K_1 <= event.key <= pygame.K_8:
                    self.attribute = ATTRIBUTES[event.key-pygame.K_1]
                elif event.key == pygame.K_g and self.attribute:
                    self.choose('Greater')
                elif event.key == pygame.K_l and self.attribute:
                    self.choose('Less')

    def draw(self):
        app, ui, battle = self.app, self.app.ui, self.battle
        result = battle.result
        ui.button('<  LEAVE', (35, 29, 100, 36), app.home, size=19)
        ui.text('THE ARENA', 155, 31, 30, GOLD, True)
        ui.text(f'ROUND  {battle.index+1:02} / {battle.round_limit:02}', 520, 35, 24, TEXT, True)
        ui.text(f'{battle.scores[0]:02}   :   {battle.scores[1]:02}', 820, 25, 44, TEXT, True)
        ui.text(f'COMBO  x{battle.combos[0]}', 1100, 39, 23, TEAL, True)
        pygame.draw.line(app.canvas, EDGE, (36, 83), (1243, 83))
        ui.text('YOUR CHAMPION', 43, 111, 20, TEAL, True)
        ui.text('OFFLINE OPPONENT', 643, 111, 20, MUTED, True)
        player, ai = battle.cards
        slide = int(25 * (1-app.animation.ease(app.animation.age/.4)))
        outcome = ('win' if result.winner==0 else 'lose' if result.winner==1 else None) if result else None
        app.card_view.draw(player, (39, 149+slide, 277, 445), highlight=self.attribute, outcome=outcome, values_override=battle.values()[0])
        # Flip narrows the card to its edge then reveals it. Hidden art never shows metadata.
        enemy_rect = pygame.Rect(639, 149+slide, 277, 445)
        if self.revealing:
            width = max(4, int(277*abs(math.cos(min(1, self.reveal_time/.60)*math.pi))))
            enemy_rect = pygame.Rect(639+(277-width)//2, 149, width, 445)
            if width < 230:
                ui.panel(enemy_rect, (24, 48, 60), GOLD, 4)
            else:
                app.card_view.draw(ai, enemy_rect, hidden=not bool(result), highlight=self.attribute, progress=False, values_override=battle.values()[1])
        else:
            ai_outcome = ('win' if result.winner==1 else 'lose' if result.winner==0 else None) if result else None
            app.card_view.draw(ai, enemy_rect, hidden=not bool(result), highlight=self.attribute, outcome=ai_outcome, progress=False, values_override=battle.values()[1])
        ui.text('HP '+str(battle.states[0]['hp']), 44, 608, 17, MUTED)
        ui.text('HP '+str(battle.states[1]['hp']), 646, 608, 17, MUTED)
        ui.text('01  CHOOSE ATTRIBUTE', 342, 154, 20, GOLD, True)
        enabled = not result and not self.revealing and not battle.ai_turn
        for i, attribute in enumerate(ATTRIBUTES):
            ui.button(f'{i+1}   {attribute.upper()}', (339, 191+i*36, 274, 30),
                      lambda a=attribute: setattr(self, 'attribute', a), active=self.attribute==attribute,
                      enabled=enabled, size=19)
        ui.text('02  SET THE RULE', 342, 496, 20, GOLD, True)
        ui.button('GREATER  >', (339, 526, 132, 44), lambda: self.choose('Greater'), True,
                  enabled=enabled and self.attribute is not None, size=19)
        ui.button('<  LESS', (481, 526, 132, 44), lambda: self.choose('Less'),
                  enabled=enabled and self.attribute is not None, size=19)
        ui.button('ABILITY: '+('ON' if self.use_ability else 'OFF'), (339, 584, 274, 31),
                  lambda: setattr(self, 'use_ability', not self.use_ability), active=self.use_ability,
                  enabled=not result and not self.revealing, size=18)
        ui.panel((942, 105, 303, 515))
        ui.text('BATTLE LOG', 963, 127, 22, GOLD, True)
        ui.text('PUBLIC INFORMATION', 963, 153, 16, MUTED)
        if not result:
            ui.text('AI IS CHOOSING...' if battle.ai_turn else 'YOUR MOVE', 965, 205, 24, TEAL, True)
            ui.wrap('Choose a strong attribute for Greater, or a low attribute for Less. Abilities modify comparison strength after reveal.', 965, 248, 250, 23, MUTED, 27)
            remaining = max(0, self.timer-self.elapsed) if self.timer else 0
            ui.text('NO TIMER' if not self.timer else f'{math.ceil(remaining):02} SECONDS', 965, 422, 28, GOLD, True)
            ui.bar((965, 463, 254, 7), remaining/self.timer if self.timer else 1)
            ui.text(f'PENDING POT: {len(battle.pot)+2} CARDS', 965, 503, 19, MUTED)
            ui.text(f"AI: {app.manager.profile['settings']['difficulty'].upper()}", 965, 538, 19, MUTED)
        else:
            winner = 'TIE' if result.winner is None else 'YOU WIN' if result.winner==0 else 'AI WINS'
            ui.text(f'ROUND {result.round:02}  /  {winner}', 963, 191, 24, TEAL if result.winner==0 else GOLD, True)
            ui.text(f'{result.attribute.upper()}  •  {result.direction.upper()}', 963, 232, 20, TEXT, True)
            ui.text(f'BASE VALUES     {result.raw[0]} : {result.raw[1]}', 963, 270, 21, MUTED)
            ui.text(f'STRENGTH        {result.strength[0]:g} : {result.strength[1]:g}', 963, 303, 21, TEXT)
            y = 345
            for message in result.abilities[:4]:
                y = ui.wrap(message, 963, y, 250, 19, GOLD, 22)+7
            ui.text(f'+{result.captured} CARDS TO WINNER', 963, 456, 20, TEAL)
            ui.text(f'+{result.xp} XP   /   +{result.coins} COINS', 963, 486, 21, GOLD)
            if result.leveled:
                ui.text('LEVEL UP!', 963, 518, 23, TEAL, True)
            ui.text(f'LAST ROUNDS: '+''.join('W ' if r.winner==0 else 'L ' if r.winner==1 else '= ' for r in battle.history[-6:]), 963, 573, 18, MUTED)
        if result and not self.revealing:
            label = 'VIEW RESULTS  >' if battle.index+1==battle.round_limit else 'NEXT ROUND  >'
            ui.button(label, (944, 640, 300, 48), self.next_round, True)
            ui.text('ENTER TO CONTINUE', 730, 658, 18, MUTED)
        else:
            ui.text('AI SELECTION TURN' if battle.ai_turn else 'KEYS 1–8: ATTRIBUTE    /    G: GREATER    /    L: LESS', 40, 662, 20, MUTED)
        ui.text('Match-start attributes. Higher strength wins in either direction.', 40, 636, 18, MUTED)


class ResultScreen(Screen):
    def draw(self):
        ui, battle = self.app.ui, self.app.manager.battle
        won = battle.scores[0] > battle.scores[1]
        tie = battle.scores[0] == battle.scores[1]
        ui.text('THE ARENA HAS SPOKEN', 640, 80, 21, GOLD, True, center=True)
        ui.text('STALEMATE' if tie else 'VICTORY' if won else 'DEFEAT', 640, 133, 84, TEXT, True, center=True)
        ui.text(f'{battle.scores[0]:02}   :   {battle.scores[1]:02}', 640, 241, 76, TEAL if won else GOLD, True, center=True)
        ui.text('YOUR CAPTURED CARDS                    OPPONENT', 640, 328, 19, MUTED, center=True)
        for i, (title, value) in enumerate([('ROUNDS', len(battle.history)), ('XP EARNED', sum(r.xp for r in battle.history)),
                                          ('ROUND COINS', sum(r.coins for r in battle.history))]):
            x = 227+i*283
            ui.panel((x, 399, 261, 108))
            ui.text(title, x+130, 418, 18, MUTED, center=True)
            ui.text(value, x+130, 450, 38, GOLD, True, center=True)
        ui.text('Progress saved automatically. Practice awards no rewards.', 640, 540, 22, MUTED, center=True)
        if battle.pot:
            ui.text(f'{len(battle.pot)} cards remained tied and were not awarded.', 640, 573, 20, MUTED, center=True)
        ui.button('PLAY AGAIN', (218, 621, 269, 51), lambda: self.app.navigate('play'), True)
        ui.button('MATCH JOURNAL', (507, 621, 269, 51), lambda: self.app.navigate('journal'))
        ui.button('MAIN MENU', (796, 621, 269, 51), self.app.home)
