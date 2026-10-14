/* AI-vs-AI balance evidence: both sides predict on alternate rounds using only own card + public history. */
import { createRequire } from 'module'; const require = createRequire(import.meta.url);
const R = require('../js/rules.js'); const cards = require('../Data/cards.json');
function match(diffA, diffB, seed) {
  const rng = R.mulberry32(seed);
  const dA = R.shuffle(cards, rng).slice(0, 30), dB = R.shuffle(cards, rng).slice(0, 30);
  const seenA = {}, seenB = {}; let sA = 0, sB = 0;
  for (let round = 1; round <= 30; round++) {
    const a = dA.pop(), b = dB.pop(), aPred = round % 2 === 1;
    const pick = aPred ? R.aiChoose(a.attributes, seenA, diffA, rng) : R.aiChoose(b.attributes, seenB, diffB, rng);
    const mine = aPred ? a.attributes[pick.attr] : b.attributes[pick.attr], theirs = aPred ? b.attributes[pick.attr] : a.attributes[pick.attr];
    const r = R.evaluateRound(mine, theirs, pick);
    if (r === 'predictorWin') (aPred ? sA++ : sB++); else if (r === 'predictorLoss') (aPred ? sB++ : sA++);
    R.ATTRS.forEach(x => { (seenA[x] ||= []).push(b.attributes[x]); (seenB[x] ||= []).push(a.attributes[x]); });
  }
  return sA > sB ? 'A' : sB > sA ? 'B' : 'D';
}
for (const [a, b] of [['expert','easy'],['expert','normal'],['hard','easy'],['normal','easy'],['expert','hard']]) {
  let wA = 0, wB = 0, d = 0; for (let i = 0; i < 200; i++) { const r = match(a, b, 1000 + i); r === 'A' ? wA++ : r === 'B' ? wB++ : d++; }
  console.log(`${a} vs ${b}: ${wA} - ${wB} (draws ${d}) over 200 matches`);
}
