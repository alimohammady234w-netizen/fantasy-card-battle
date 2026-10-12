import { Battle3D } from './battle3d.js';
const R = window.FCB_RULES, $ = s => document.querySelector(s), el = (t, c, txt) => { const e = document.createElement(t); if (c) e.className = c; if (txt != null) e.textContent = txt; return e; };
const KEY = 'fcb_save_v1', BACKUP = 'fcb_save_v1_backup';
let CARDS = [], BY_ID = {}, save, scene3d = null, battle = null, audioCtx = null;

function newSave() {
  const owned = {}; CARDS.forEach(c => owned[c.id] = { copies: 1, level: 1, xp: 0 });
  const starter = CARDS.slice(0, R.DECK_SIZE).map(c => c.id);
  return { schema: 1, player: { level: 1, xp: 0, wins: 0, losses: 0, ties: 0, matchesWon: 0, matchesPlayed: 0 },
    owned, decks: [{ id: 'deck-1', name: 'Starter Deck', cards: starter }], activeDeckId: 'deck-1',
    settings: { difficulty: 'normal', sound: 0.7, effects: true, reducedMotion: false }, rewardedMatches: [], nextDeckNum: 2 };
}
function persist() { try { const prev = localStorage.getItem(KEY); if (prev) localStorage.setItem(BACKUP, prev); localStorage.setItem(KEY, JSON.stringify(save)); } catch (e) { toast('Save failed: storage unavailable.'); } }
function loadSave() {
  try { const raw = localStorage.getItem(KEY); if (!raw) return newSave();
    const s = JSON.parse(raw); if (s.schema !== 1 || !s.owned || !s.decks) throw new Error('schema');
    CARDS.forEach(c => { if (!s.owned[c.id]) s.owned[c.id] = { copies: 1, level: 1, xp: 0 }; }); return s; // migrate: new cards granted
  } catch (e) { try { const b = JSON.parse(localStorage.getItem(BACKUP)); if (b && b.schema === 1) return b; } catch (_) {} return newSave(); }
}
function toast(msg) { const t = $('#toast'); t.textContent = msg; t.classList.add('show'); setTimeout(() => t.classList.remove('show'), 3200); }
function sfx(kind) { // synthesized audio (no external assets); Settings volume applies
  if (!save.settings.sound) return; audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)();
  const notes = { select: [520], lock: [392, 523], win: [523, 659, 784], lose: [330, 262], tie: [440, 440], flip: [300, 480], levelup: [523, 659, 784, 1046] }[kind] || [440];
  notes.forEach((f, i) => { const o = audioCtx.createOscillator(), g = audioCtx.createGain(); o.frequency.value = f; o.type = 'triangle';
    g.gain.setValueAtTime(save.settings.sound * .12, audioCtx.currentTime + i * .09); g.gain.exponentialRampToValueAtTime(.001, audioCtx.currentTime + i * .09 + .22);
    o.connect(g).connect(audioCtx.destination); o.start(audioCtx.currentTime + i * .09); o.stop(audioCtx.currentTime + i * .09 + .25); });
}
function show(screen) { document.querySelectorAll('.screen').forEach(s => s.classList.toggle('active', s.id === 'screen-' + screen)); window.scrollTo(0, 0); if (screen === 'collection') renderCollection(); if (screen === 'decks') renderDecks(); if (screen === 'progress') renderProgress(); if (screen === 'settings') renderSettings(); }

/* ---------- Collection / detail / upgrade ---------- */
let collSel = null;
function rarityClass(r) { return 'r-' + r.toLowerCase(); }
function cardTile(card, onclick) {
  const o = save.owned[card.id], t = el('button', 'tile ' + rarityClass(card.rarity));
  t.innerHTML = `<img loading="lazy" src="${card.artwork}" alt="${card.name} artwork"><span class="tile-name">${card.name.split(',')[0]}</span>
    <span class="tile-meta">${card.rarity} · Lv ${o.level}</span>`; t.onclick = () => onclick(card); return t;
}
function renderCollection() {
  const q = $('#collSearch').value.toLowerCase(), cat = $('#collCat').value, rar = $('#collRarity').value, sort = $('#collSort').value;
  let list = CARDS.filter(c => c.name.toLowerCase().includes(q) && (!cat || c.category === cat) && (!rar || c.rarity === rar));
  list.sort((a, b) => sort === 'name' ? a.name.localeCompare(b.name) : sort === 'rarity' ? R.RARITIES.indexOf(b.rarity) - R.RARITIES.indexOf(a.rarity) : (b.attributes[sort] || 0) - (a.attributes[sort] || 0));
  const grid = $('#collGrid'); grid.innerHTML = ''; list.slice(0, 120).forEach(c => grid.appendChild(cardTile(c, openDetail))); // cap DOM nodes for large collections
  $('#collCount').textContent = `${list.length} cards` + (list.length > 120 ? ' (showing first 120 — refine search)' : '');
  if (collSel) openDetail(collSel, true);
}
function openDetail(card, keep) {
  collSel = card; const o = save.owned[card.id], attrs = R.effectiveAttrs(card, o.level), d = $('#cardDetail');
  d.innerHTML = `<img src="${card.artwork}" alt=""><h3>${card.name}</h3><p class="badge ${rarityClass(card.rarity)}">${card.rarity} · ${card.category}</p>
    <p>${card.description}</p><p class="flavor">“${card.flavor}”</p><p>Owned copies: <b>${o.copies}</b> · Level <b>${o.level}</b> · XP ${o.xp}/${R.xpForNextLevel(o.level)}</p>
    <div class="attr-table">${R.ATTRS.map(a => `<div><span>${R.ATTR_LABEL[a]}</span><b>${attrs[a]}</b><small> (base ${card.attributes[a]})</small></div>`).join('')}</div>
    <p class="hint">Ability: <b>${card.ability}</b> (flavor identity in this build — attributes decide rounds).</p>`;
  const up = el('button', 'btn gold', o.level >= R.CARD_LEVEL_MAX ? 'Max Level' : `Upgrade — needs ${R.xpForNextLevel(o.level)} XP (have ${o.xp})`);
  up.disabled = o.level >= R.CARD_LEVEL_MAX || o.xp < R.xpForNextLevel(o.level);
  up.onclick = () => { o.xp -= R.xpForNextLevel(o.level); o.level++; persist(); sfx('levelup'); toast(`${card.name} reached level ${o.level}! +${R.CARD_LEVEL_MAX ? 2 : 2} to all attributes (cap 100).`); renderCollection(); };
  d.appendChild(up); if (!keep) d.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

/* ---------- Deck builder ---------- */
let editDeckId = null;
function ownedCopies() { const m = {}; for (const [id, o] of Object.entries(save.owned)) m[id] = o.copies; return m; }
function renderDecks() {
  if (!editDeckId || !save.decks.find(d => d.id === editDeckId)) editDeckId = save.activeDeckId;
  const sel = $('#deckSelect'); sel.innerHTML = save.decks.map(d => `<option value="${d.id}" ${d.id === editDeckId ? 'selected' : ''}>${d.name} (${d.cards.length}/${R.DECK_SIZE})</option>`).join('');
  const deck = save.decks.find(d => d.id === editDeckId), v = R.validateDeck(deck.cards, ownedCopies());
  $('#deckStatus').textContent = v.valid ? '✓ Valid 30-card deck — ready for battle.' : '⚠ ' + v.errors.join(' ');
  $('#deckStatus').className = v.valid ? 'ok' : 'warn';
  $('#deckList').innerHTML = deck.cards.map((id, i) => `<li>${BY_ID[id].name} <button data-rm="${i}" aria-label="Remove">✕</button></li>`).join('') || '<li class="hint">Empty deck — add cards from the collection below.</li>';
  document.querySelectorAll('[data-rm]').forEach(b => b.onclick = () => { deck.cards.splice(+b.dataset.rm, 1); persist(); renderDecks(); });
  const pool = $('#deckPool'); pool.innerHTML = '';
  CARDS.filter(c => (save.owned[c.id].copies - deck.cards.filter(x => x === c.id).length) > 0).slice(0, 120)
    .forEach(c => { const t = cardTile(c, () => { if (deck.cards.length >= R.DECK_SIZE) return toast('Deck is full (30). Remove a card first.'); deck.cards.push(c.id); persist(); renderDecks(); }); pool.appendChild(t); });
  $('#setActiveBtn').textContent = save.activeDeckId === deck.id ? '★ Active battle deck' : 'Set as battle deck';
}

/* ---------- Progression / settings ---------- */
function renderProgress() { const p = save.player;
  $('#progressBody').innerHTML = `<div class="stat-cards"><div><span>Player Level</span><b>${p.level}</b></div><div><span>XP</span><b>${p.xp} / ${R.xpForNextLevel(p.level)}</b></div>
  <div><span>Matches</span><b>${p.matchesPlayed}</b></div><div><span>Match Wins</span><b>${p.matchesWon}</b></div><div><span>Rounds W / L / T</span><b>${p.wins} / ${p.losses} / ${p.ties}</b></div>
  <div><span>Cards Owned</span><b>${Object.values(save.owned).reduce((n, o) => n + o.copies, 0)}</b></div></div>
  <progress max="${R.xpForNextLevel(p.level)}" value="${p.xp}"></progress>
  <h3>Strongest upgraded cards</h3><div class="tiles small">${CARDS.map(c => ({ c, o: save.owned[c.id] })).sort((a, b) => b.o.level - a.o.level).slice(0, 10).map(({ c, o }) => `<div class="mini">${c.name.split(',')[0]}<span>Lv ${o.level} · ${o.xp} XP</span></div>`).join('')}</div>
  <button class="btn danger" id="resetSaveBtn">Reset all progress (deletes local save)</button>`;
  $('#resetSaveBtn').onclick = () => { if (confirm('Delete your local save and start over?')) { localStorage.removeItem(KEY); localStorage.removeItem(BACKUP); save = newSave(); persist(); toast('Progress reset. New game initialized.'); renderProgress(); } };
}
function renderSettings() { const s = save.settings; $('#setDifficulty').value = s.difficulty; $('#setSound').value = s.sound; $('#setFx').checked = s.effects; $('#setMotion').checked = s.reducedMotion; }

/* ---------- Battle ---------- */
function startMatch() {
  const deck = save.decks.find(d => d.id === save.activeDeckId) || save.decks[0];
  const v = R.validateDeck(deck.cards, ownedCopies()); if (!v.valid) { toast('Cannot start: ' + v.errors[0]); show('decks'); return; }
  const rng = R.mulberry32((Date.now() ^ Math.floor(Math.random() * 1e9)) >>> 0); // ordinary play: fresh entropy (seeded RNG is used in tests)
  const aiPool = R.shuffle(CARDS.map(c => c.id), rng).slice(0, R.DECK_SIZE);
  battle = { id: 'm' + Date.now(), rng, round: 0, scoreP: 0, scoreA: 0, ties: 0, phase: 'choose', predictor: 'player',
    pDeck: R.shuffle(deck.cards, rng), aDeck: aiPool, pCard: null, aCard: null, selAttr: null, selDir: null,
    seenByAI: {}, history: [], resolved: false, finished: false };
  show('battle'); if (!scene3d) scene3d = new Battle3D($('#gl')); else scene3d._resize();
  $('#summary').classList.add('hidden'); nextRound();
}
async function nextRound() {
  if (!battle.pDeck.length || !battle.aDeck.length) return finishMatch();
  battle.round++; battle.resolved = false; battle.phase = 'choose'; battle.selAttr = null; battle.selDir = null;
  battle.predictor = battle.round % 2 === 1 ? 'player' : 'ai'; // predictor alternates; the predictor's card is the "mine" card in rules terms
  battle.pCard = BY_ID[battle.pDeck.pop()]; battle.aCard = BY_ID[battle.aDeck.pop()];
  // AI decides now, using only its own card + public history of revealed player attributes (never the hidden player card)
  const aiAttrs = R.effectiveAttrs(battle.aCard, 1);
  battle.aiPick = battle.predictor === 'ai' ? R.aiChoose(aiAttrs, battle.seenByAI, save.settings.difficulty, battle.rng) : null;
  const pAttrs = attrsOf(battle.pCard, true);
  await scene3d.setCard('player', battle.pCard, pAttrs); await scene3d.setCard('opp', null, null, 'Assets/Artwork/UI/card-back.png');
  renderBattle(`Round ${battle.round}: ${battle.predictor === 'player' ? 'YOUR prediction — pick an attribute and Greater/Less, then Lock In.' : 'AI is predicting on its hidden card… press Reveal when ready.'}`);
  if (battle.predictor === 'ai') { $('#attrGrid').innerHTML = '<p class="hint">The AI has locked its prediction (based only on its own card and past revealed rounds). Your card is face-up; the AI card stays hidden until reveal.</p>'; }
}
function attrsOf(card, isPlayer) { return isPlayer ? R.effectiveAttrs(card, save.owned[card.id].level) : R.effectiveAttrs(card, 1); }
function renderBattle(msg) {
  const pAttrs = attrsOf(battle.pCard, true);
  $('#roundLabel').textContent = `Round ${battle.round}`; $('#scoreLabel').textContent = `You ${battle.scoreP} — ${battle.scoreA} AI · Ties ${battle.ties}`;
  $('#deckLabel').textContent = `Decks: you ${battle.pDeck.length} · AI ${battle.aDeck.length} remaining`;
  $('#pCardName').textContent = battle.pCard.name; $('#pCardMeta').textContent = `${battle.pCard.rarity} · ${battle.pCard.category} · Lv ${save.owned[battle.pCard.id].level}`;
  $('#pCardArt').src = battle.pCard.artwork; $('#aCardArt').src = 'Assets/Artwork/UI/card-back.png'; $('#aCardName').textContent = 'Hidden Card';
  $('#aCardAttrs').innerHTML = '<p class="hint">Revealed after the prediction is locked.</p>';
  $('#pCardAttrs').innerHTML = R.ATTRS.map(a => `<div class="attr ${battle.selAttr === a ? 'sel' : ''}"><span>${R.ATTR_LABEL[a]}</span><b>${pAttrs[a]}</b></div>`).join('');
  if (battle.predictor === 'player') {
    $('#attrGrid').innerHTML = R.ATTRS.map(a => `<button class="btn attr-btn ${battle.selAttr === a ? 'sel' : ''}" data-attr="${a}">${R.ATTR_LABEL[a]} <b>${pAttrs[a]}</b></button>`).join('');
    document.querySelectorAll('[data-attr]').forEach(b => b.onclick = () => { battle.selAttr = b.dataset.attr; sfx('select'); renderBattle(msg); });
  }
  document.querySelectorAll('[data-dir]').forEach(b => b.classList.toggle('sel', battle.selDir === b.dataset.dir));
  $('#lockBtn').textContent = battle.predictor === 'player' ? '🔒 Lock In & Reveal' : '👁 Reveal AI Card';
  $('#lockBtn').disabled = battle.phase !== 'choose' || (battle.predictor === 'player' && (!battle.selAttr || !battle.selDir));
  $('#battleMsg').textContent = msg; $('#history').innerHTML = battle.history.slice(-8).reverse().map(h => `<li>${h}</li>`).join('');
}
async function lockAndReveal() {
  if (!battle || battle.phase !== 'choose' || battle.resolved) return; // resolve exactly once
  if (battle.predictor === 'player' && (!battle.selAttr || !battle.selDir)) return;
  battle.phase = 'revealing'; $('#lockBtn').disabled = true; sfx('lock');
  const pAttrs = attrsOf(battle.pCard, true), aAttrs = attrsOf(battle.aCard, false);
  await scene3d.setCard('opp', battle.aCard, aAttrs); scene3d.flipOpponent(); sfx('flip');
  const pick = battle.predictor === 'player' ? { attr: battle.selAttr, direction: battle.selDir } : battle.aiPick;
  const mine = battle.predictor === 'player' ? pAttrs[pick.attr] : aAttrs[pick.attr];
  const theirs = battle.predictor === 'player' ? aAttrs[pick.attr] : pAttrs[pick.attr];
  const result = R.evaluateRound(mine, theirs, pick); battle.resolved = true;
  R.ATTRS.forEach(a => { (battle.seenByAI[a] = battle.seenByAI[a] || []).push(pAttrs[a]); }); // player card is now public history
  let outcome, cls;
  if (result === 'tie') { battle.ties++; save.player.ties++; outcome = `🤝 TIE on ${R.ATTR_LABEL[pick.attr]} (${mine} vs ${theirs}) — no point awarded.`; cls = 'tie'; }
  else if ((result === 'predictorWin') === (battle.predictor === 'player')) { battle.scoreP++; save.player.wins++; outcome = `✅ You ${result === 'predictorWin' ? 'predicted correctly' : '— the AI predicted wrong'}! ${R.ATTR_LABEL[pick.attr]}: yours ${pAttrs[pick.attr]} vs AI ${aAttrs[pick.attr]}. Point to you.`; cls = 'win'; }
  else { battle.scoreA++; save.player.losses++; outcome = `❌ Point to AI. ${R.ATTR_LABEL[pick.attr]}: yours ${pAttrs[pick.attr]} vs AI ${aAttrs[pick.attr]}${battle.predictor === 'ai' ? ` (AI predicted ${pick.direction})` : ` (you predicted ${pick.direction})`}.`; cls = 'lose'; }
  // Card XP for the player's card that just played (instance progression; definitions untouched)
  const o = save.owned[battle.pCard.id]; o.xp += result === 'tie' ? 12 : ((cls === 'win') ? 20 : 8);
  battle.history.push(`R${battle.round}: ${battle.pCard.name.split(',')[0]} vs ${battle.aCard.name.split(',')[0]} — ${outcome.replace(/<[^>]+>/g, '')}`);
  $('#aCardArt').src = battle.aCard.artwork; $('#aCardName').textContent = battle.aCard.name;
  $('#aCardAttrs').innerHTML = R.ATTRS.map(a => `<div class="attr ${a === pick.attr ? 'sel' : ''}"><span>${R.ATTR_LABEL[a]}</span><b>${aAttrs[a]}</b></div>`).join('');
  $('#pCardAttrs').innerHTML = R.ATTRS.map(a => `<div class="attr ${a === pick.attr ? 'sel' : ''}"><span>${R.ATTR_LABEL[a]}</span><b>${pAttrs[a]}</b></div>`).join('');
  scene3d.celebrate(cls === 'lose' ? 'opp' : 'player'); sfx(cls); persist();
  $('#battleMsg').innerHTML = `<span class="${cls}">${outcome}</span>`; $('#lockBtn').textContent = (battle.pDeck.length && battle.aDeck.length) ? '▶ Next Round' : '🏁 See Match Result';
  $('#lockBtn').disabled = false; battle.phase = 'resolved';
  $('#scoreLabel').textContent = `You ${battle.scoreP} — ${battle.scoreA} AI · Ties ${battle.ties}`;
}
function finishMatch() {
  if (!battle || battle.finished) return; battle.finished = true;
  const won = battle.scoreP > battle.scoreA, draw = battle.scoreP === battle.scoreA, p = save.player;
  p.matchesPlayed++; if (won) p.matchesWon++;
  let rewardNote = 'Rewards already applied for this match.';
  if (!save.rewardedMatches.includes(battle.id)) { // idempotent rewards
    save.rewardedMatches.push(battle.id);
    const xp = won ? 120 : draw ? 85 : 60, gains = R.applyXp(p, xp); let dup = '';
    if (won && battle.rng() < 0.5) { const c = CARDS[Math.floor(battle.rng() * CARDS.length)]; save.owned[c.id].copies++; dup = ` Bonus copy: ${c.name}!`; }
    rewardNote = `+${xp} player XP${gains ? ` — LEVEL UP! You are now level ${p.level}` : ''}.${dup}`; if (gains) sfx('levelup');
  }
  persist(); const s = $('#summary'); s.classList.remove('hidden');
  s.innerHTML = `<h2>${won ? '🏆 VICTORY' : draw ? '🤝 DRAW' : '💀 DEFEAT'}</h2><p>Final score — You <b>${battle.scoreP}</b> · AI <b>${battle.scoreA}</b> · Ties ${battle.ties} · Rounds ${battle.round}</p><p>${rewardNote}</p>
    <button class="btn gold" id="againBtn">Play Again</button> <button class="btn" id="menuBtn2">Main Menu</button>`;
  $('#againBtn').onclick = startMatch; $('#menuBtn2').onclick = () => show('menu'); sfx(won ? 'win' : 'lose');
}

/* ---------- Boot / wiring ---------- */
async function boot() {
  CARDS = await (await fetch('Data/cards.json')).json(); CARDS.forEach(c => BY_ID[c.id] = c);
  save = loadSave(); if (!localStorage.getItem(KEY)) persist();
  const cats = [...new Set(CARDS.map(c => c.category))];
  $('#collCat').innerHTML = '<option value="">All categories</option>' + cats.map(c => `<option>${c}</option>`).join('');
  $('#collSort').innerHTML = '<option value="name">Name</option><option value="rarity">Rarity</option>' + R.ATTRS.map(a => `<option value="${a}">${R.ATTR_LABEL[a]}</option>`).join('');
  document.querySelectorAll('[data-nav]').forEach(b => b.onclick = () => { const t = b.dataset.nav; if (t === 'battle') startMatch(); else if (t === 'exit') { document.body.innerHTML = '<main class="exit"><h1>Thanks for playing!</h1><p>Your progress is saved locally in this browser. You can close this tab.</p></main>'; } else show(t); });
  ['collSearch'].forEach(id => $('#' + id).addEventListener('input', renderCollection));
  ['collCat', 'collRarity', 'collSort'].forEach(id => $('#' + id).addEventListener('change', renderCollection));
  $('#deckSelect').onchange = e => { editDeckId = e.target.value; renderDecks(); };
  $('#newDeckBtn').onclick = () => { const d = { id: 'deck-' + save.nextDeckNum, name: 'Deck ' + save.nextDeckNum++, cards: [] }; save.decks.push(d); editDeckId = d.id; persist(); renderDecks(); };
  $('#renameDeckBtn').onclick = () => { const d = save.decks.find(x => x.id === editDeckId), n = prompt('Deck name:', d.name); if (n) { d.name = n.slice(0, 40); persist(); renderDecks(); } };
  $('#deleteDeckBtn').onclick = () => { if (save.decks.length <= 1) return toast('You must keep at least one deck.'); save.decks = save.decks.filter(x => x.id !== editDeckId); if (save.activeDeckId === editDeckId) save.activeDeckId = save.decks[0].id; editDeckId = save.activeDeckId; persist(); renderDecks(); };
  $('#setActiveBtn').onclick = () => { save.activeDeckId = editDeckId; persist(); renderDecks(); toast('Battle deck updated.'); };
  $('#autoFillBtn').onclick = () => { const d = save.decks.find(x => x.id === editDeckId); const need = R.DECK_SIZE - d.cards.length; const avail = CARDS.filter(c => save.owned[c.id].copies - d.cards.filter(x => x === c.id).length > 0 && !d.cards.includes(c.id)); d.cards.push(...avail.slice(0, need).map(c => c.id)); persist(); renderDecks(); };
  $('#clearDeckBtn').onclick = () => { save.decks.find(x => x.id === editDeckId).cards = []; persist(); renderDecks(); };
  document.querySelectorAll('[data-dir]').forEach(b => b.onclick = () => { if (battle) { battle.selDir = b.dataset.dir; sfx('select'); renderBattle($('#battleMsg').textContent); } });
  $('#lockBtn').onclick = () => { if (battle && battle.phase === 'resolved') (battle.pDeck.length && battle.aDeck.length) ? nextRound() : finishMatch(); else lockAndReveal(); };
  $('#quitMatchBtn').onclick = () => { if (confirm('Abandon this match? Round progress in this match will be lost (earned card XP is kept).')) { battle = null; show('menu'); } };
  $('#setDifficulty').onchange = e => { save.settings.difficulty = e.target.value; persist(); };
  $('#setSound').oninput = e => { save.settings.sound = +e.target.value; persist(); };
  $('#setFx').onchange = e => { save.settings.effects = e.target.checked; document.body.classList.toggle('no-fx', !e.target.checked); persist(); };
  $('#setMotion').onchange = e => { save.settings.reducedMotion = e.target.checked; document.body.classList.toggle('reduced-motion', e.target.checked); persist(); };
  document.body.classList.toggle('no-fx', !save.settings.effects); show('menu');
}
boot().catch(e => { document.body.innerHTML = `<main class="exit"><h1>Failed to load game data</h1><p>${e.message}</p><p>Serve this folder over HTTP (see README) — opening via file:// blocks data loading.</p></main>`; });
