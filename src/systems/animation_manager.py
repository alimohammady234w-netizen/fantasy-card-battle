import math


class AnimationManager:
    def __init__(self):
        self.time = 0.
        self.entered = 0.

    def update(self, dt, speed=1.):
        self.time += dt * speed

    def reset(self):
        self.entered = self.time

    @property
    def age(self):
        return self.time - self.entered

    @staticmethod
    def ease(value):
        t = max(0., min(1., value))
        return 1 - (1-t)**3

    def pulse(self, rate=3):
        return .5 + .5 * math.sin(self.time * rate)
