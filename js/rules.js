/* Fantasy Card Battle — pure game rules (no DOM). Shared by the web game and Node tests.
   The UE 5.8 C++ implementation (Unreal/.../FCBBattleRules) mirrors these functions exactly. */
(function (global) {
  'use strict';
  const ATTRS = ['power', 'speed', 'height', 'defense', 'intelligence', 'stamina', 'luck', 'age'];
  const ATTR_LABEL = { power: 'Power', speed: 'Speed', height: 'Height', defense: 'Defense', intelligence: 'Intelligence', stamina: 'Stamina', luck: 'Luck', age: 'Age' };
  const RARITIES = ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary', 'Mythic'];
  const DECK_SIZE = 30, CARD_LEVEL_MAX = 10, CARD_BONUS_PER_LEVEL = 2;

  function mulberry32(seed) { let a = seed >>> 0; return function () { a |= 0; a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
  function shuffle(arr, rng) { const a = arr.slice(); for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(rng() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; } return a; }

  /* 'greater' predicts player's value strictly > opponent's; 'less' strictly <. Equal => tie (neither). */
  function evaluateRound(playerValue, opponentValue, prediction) {
    if (!ATTRS.includes(prediction.attr)) throw new Error('Invalid attribute: ' + prediction.attr);
    if (playerValue === opponentValue) return 'tie';
    const isGreater = playerValue > opponentValue;
    return (prediction.direction === 'greater') === isGreater ? 'predictorWin' : 'predictorLoss';
  }
  function effectiveAttrs(card, level) { // upgraded instance values; base definition is never mutated
    const bonus = (Math.min(CARD_LEVEL_MAX, Math.max(1, level || 1)) - 1) * CARD_BONUS_PER_LEVEL, out = {};
    for (const a of ATTRS) out[a] = Math.min(100, card.attributes[a] + bonus); return out;
  }
  function validateDeck(deckIds, ownedCopies, size = DECK_SIZE) {
    const errors = [];
    if (!Array.isArray(deckIds) || deckIds.length !== size) errors.push(`Deck must contain exactly ${size} cards (currently ${deckIds ? deckIds.length : 0}).`);
    const counts = {}; for (const id of deckIds || []) counts[id] = (counts[id] || 0) + 1;
    for (const [id, n] of Object.entries(counts)) if (n > (ownedCopies[id] || 0)) errors.push(`Not enough owned copies of ${id}: deck uses ${n}, you own ${ownedCopies[id] || 0}.`);
    return { valid: errors.length === 0, errors };
  }
  /* AI: sees ONLY its own card + publicly revealed opponent attribute history. Never hidden cards. */
  function aiChoose(ownAttrs, seenOpponentValues, difficulty, rng) {
    const pick = (arr) => arr[Math.floor(rng() * arr.length)];
    if (difficulty === 'easy') return { attr: rng() < 0.75 ? pick(ATTRS) : ATTRS.reduce((b, a) => ownAttrs[a] > ownAttrs[b] ? a : b, ATTRS[0]), direction: rng() < 0.5 ? 'greater' : 'less' };
    let best = null;
    for (const attr of ATTRS) {
      const seen = seenOpponentValues[attr] || [], v = ownAttrs[attr];
      let pGreater;
      if (!seen.length) pGreater = v / 100; // prior: uniform 1..100 field
      else if (difficulty === 'normal') pGreater = v > (seen.reduce((x, y) => x + y, 0) / seen.length) ? 0.72 : 0.28;
      else pGreater = seen.filter(x => v > x).length / seen.length; // empirical probability from public history
      if (difficulty === 'expert' && seen.length >= 4) pGreater = (seen.filter(x => v > x).length + v / 100) / (seen.length + 1); // calibrated blend
      const p = Math.max(pGreater, 1 - pGreater), cand = { attr, direction: pGreater >= 0.5 ? 'greater' : 'less', confidence: p };
      if (!best || p > best.confidence) best = cand;
    }
    if (difficulty === 'normal' && rng() < 0.18) best.direction = best.direction === 'greater' ? 'less' : 'greater'; // human-like error
    if (difficulty === 'hard' && rng() < 0.07) best.direction = best.direction === 'greater' ? 'less' : 'greater';
    return best;
  }
  const xpForNextLevel = (level) => Math.round(100 * Math.pow(level, 1.5));
  function applyXp(state, gained) { // state {level,xp}; returns levels gained (overflow-safe)
    let levels = 0; state.xp = Math.max(0, state.xp + gained);
    while (state.level < 99 && state.xp >= xpForNextLevel(state.level)) { state.xp -= xpForNextLevel(state.level); state.level++; levels++; }
    return levels;
  }
  const api = { ATTRS, ATTR_LABEL, RARITIES, DECK_SIZE, CARD_LEVEL_MAX, mulberry32, shuffle, evaluateRound, effectiveAttrs, validateDeck, aiChoose, xpForNextLevel, applyXp };
  if (typeof module !== 'undefined') module.exports = api; global.FCB_RULES = api;
})(typeof window !== 'undefined' ? window : globalThis);
