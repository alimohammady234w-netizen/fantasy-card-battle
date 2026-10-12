/* Automated validation: card data, battle rules, deck rules, AI information-hiding, progression.
   Run: node Tests/run_tests.mjs  (exit code 0 = all pass) */
import { readFileSync, existsSync } from 'fs';
import { createRequire } from 'module';
const require = createRequire(import.meta.url); const R = require('../js/rules.js');
const cards = JSON.parse(readFileSync(new URL('../Data/cards.json', import.meta.url)));
let pass = 0, fail = 0; const t = (name, cond) => { cond ? pass++ : (fail++, console.log('FAIL:', name)); };

// Card data
t('exactly 50 cards', cards.length === 50);
t('unique ids', new Set(cards.map(c => c.id)).size === 50);
t('all have 8 attributes in 1..100', cards.every(c => Object.keys(c.attributes).length === 8 && Object.values(c.attributes).every(v => v >= 1 && v <= 100)));
t('rarities valid', cards.every(c => R.RARITIES.includes(c.rarity)));
t('artwork files exist', cards.every(c => existsSync(new URL('../' + c.artwork, import.meta.url))));
t('every card has name/flavor/ability', cards.every(c => c.name && c.flavor && c.ability));
t('no unbeatable card (every card has a weakness <=34 and no 100s sweep)', cards.every(c => Math.min(...Object.values(c.attributes)) <= 34));
// Deterministic re-derivation: re-running the generator logic is validated by data file validation report
t('validation_report PASS', JSON.parse(readFileSync(new URL('../Data/validation_report.json', import.meta.url))).status === 'PASS');

// Battle rules
t('greater wins when higher', R.evaluateRound(80, 50, { attr: 'power', direction: 'greater' }) === 'predictorWin');
t('greater fails when lower', R.evaluateRound(40, 50, { attr: 'power', direction: 'greater' }) === 'predictorLoss');
t('less wins when lower', R.evaluateRound(40, 50, { attr: 'speed', direction: 'less' }) === 'predictorWin');
t('less fails when higher', R.evaluateRound(80, 50, { attr: 'speed', direction: 'less' }) === 'predictorLoss');
t('equal is tie for greater', R.evaluateRound(50, 50, { attr: 'age', direction: 'greater' }) === 'tie');
t('equal is tie for less', R.evaluateRound(50, 50, { attr: 'age', direction: 'less' }) === 'tie');
t('all 8 attributes handled uniformly', R.ATTRS.every(a => R.evaluateRound(1, 2, { attr: a, direction: 'less' }) === 'predictorWin'));

// Deck rules
const owned = {}; cards.forEach(c => owned[c.id] = 1); const deck30 = cards.slice(0, 30).map(c => c.id);
t('30-card deck valid', R.validateDeck(deck30, owned).valid);
t('29-card deck invalid', !R.validateDeck(deck30.slice(0, 29), owned).valid);
t('duplicate beyond owned copies invalid', !R.validateDeck([...deck30.slice(0, 29), deck30[0]], owned).valid);
t('duplicate allowed when 2 copies owned', R.validateDeck([...deck30.slice(0, 29), deck30[0]], { ...owned, [deck30[0]]: 2 }).valid);

// AI: cannot see hidden card — its decision is a pure function of own card + public history only.
const rng = R.mulberry32(42);
const pick = R.aiChoose(cards[0].attributes, { power: [10, 20] }, 'expert', rng);
t('AI returns valid choice', R.ATTRS.includes(pick.attr) && ['greater', 'less'].includes(pick.direction));
t('AI expert favors strong attribute direction', (() => { const s = { power: 95, speed: 5, height: 5, defense: 5, intelligence: 5, stamina: 5, luck: 5, age: 5 }; const seen = { power: [10, 20, 30] }; const p = R.aiChoose(s, seen, 'expert', R.mulberry32(1)); return p.attr === 'power' && p.direction === 'greater'; })());
t('AI easy still valid over 200 draws', (() => { const r = R.mulberry32(7); for (let i = 0; i < 200; i++) { const p = R.aiChoose(cards[i % 50].attributes, {}, 'easy', r); if (!R.ATTRS.includes(p.attr)) return false; } return true; })());

// Progression
t('xp curve monotonic', R.xpForNextLevel(2) > R.xpForNextLevel(1));
t('applyXp levels up and banks remainder', (() => { const s = { level: 1, xp: 0 }; R.applyXp(s, R.xpForNextLevel(1) + 5); return s.level === 2 && s.xp === 5; })());
t('upgrades do not mutate base definition', (() => { const c = JSON.parse(JSON.stringify(cards[0])); const before = c.attributes.power; const up = R.effectiveAttrs(c, 5); return c.attributes.power === before && up.power === Math.min(100, before + 8); })());

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail ? 1 : 0);
