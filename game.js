* {
  box-sizing: border-box;
}

:root {
  --bg-1: #120f1d;
  --bg-2: #1d1a2f;
  --panel: rgba(30, 26, 45, 0.88);
  --panel-alt: rgba(52, 42, 72, 0.9);
  --gold: #f2d56b;
  --gold-strong: #f6b73c;
  --cyan: #8fe7ff;
  --green: #7ef0a1;
  --danger: #ff7575;
  --text: #ebebf5;
  --muted: #c7c2db;
  --shadow: rgba(0, 0, 0, 0.35);
}

body {
  margin: 0;
  min-height: 100vh;
  display: grid;
  place-items: center;
  background: radial-gradient(circle at top, #2b1d45, var(--bg-1) 45%, var(--bg-2));
  color: var(--text);
  font-family: Arial, Helvetica, sans-serif;
}

.app-shell {
  width: min(1200px, 92vw);
  padding: 24px;
  background: rgba(17, 15, 24, 0.9);
  border: 1px solid rgba(255,255,255,0.1);
  border-radius: 24px;
  box-shadow: 0 22px 70px var(--shadow);
}

.topbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
}

.eyebrow {
  margin: 0;
  text-transform: uppercase;
  letter-spacing: 0.18em;
  font-size: 0.72rem;
  color: var(--gold);
}

h1 {
  margin: 6px 0 0;
  font-size: clamp(2rem, 4vw, 3.4rem);
}

.primary-btn,
.battle-btn,
.attribute-btn {
  border: none;
  cursor: pointer;
  transition: transform 150ms ease, filter 150ms ease;
}

.primary-btn:hover,
.battle-btn:hover,
.attribute-btn:hover {
  transform: translateY(-1px);
  filter: brightness(1.06);
}

.primary-btn {
  padding: 12px 18px;
  border-radius: 12px;
  background: linear-gradient(135deg, var(--gold), var(--gold-strong));
  color: #231c1a;
  font-weight: 700;
}

.status-panel {
  display: grid;
  grid-template-columns: repeat(3, minmax(120px, 1fr));
  gap: 18px;
  margin-bottom: 24px;
}

.status-box {
  padding: 18px 14px;
  background: var(--panel);
  border: 1px solid rgba(255,255,255,0.08);
  border-radius: 16px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  color: var(--muted);
}

.status-box strong {
  font-size: 1.6rem;
  color: var(--text);
}

.status-box.highlight {
  background: linear-gradient(135deg, rgba(242, 213, 107, 0.18), rgba(246, 183, 60, 0.08));
  border-color: rgba(242,213,107,0.4);
}

.arena {
  display: grid;
  grid-template-columns: minmax(260px, 1fr) 220px minmax(260px, 1fr);
  gap: 18px;
  margin-bottom: 20px;
}

.card-panel {
  background: var(--panel);
  border: 1px solid rgba(255,255,255,0.08);
  border-radius: 20px;
  padding: 16px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 14px;
  color: var(--muted);
  font-weight: 600;
}

.badge {
  display: inline-block;
  background: rgba(143, 231, 255, 0.12);
  color: var(--cyan);
  padding: 5px 10px;
  border-radius: 999px;
  font-size: 0.75rem;
}

.badge.hostile {
  background: rgba(255,117,117,0.12);
  color: var(--danger);
}

.card {
  min-height: 420px;
  border-radius: 18px;
  padding: 20px 18px;
  position: relative;
  overflow: hidden;
  border: 1px solid rgba(255,255,255,0.12);
  box-shadow: inset 0 0 50px rgba(255,255,255,0.02);
}

.card-player {
  background: linear-gradient(135deg, rgba(143, 231, 255, 0.12), rgba(110, 167, 255, 0.2));
}

.card-ai {
  background: linear-gradient(135deg, rgba(255, 117, 117, 0.12), rgba(160, 104, 255, 0.2));
}

.card::before {
  content: "";
  position: absolute;
  inset: 0;
  background: radial-gradient(circle at top right, rgba(255,255,255,0.12), transparent 40%);
}

.card-inner {
  position: relative;
  z-index: 1;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.card-sigil {
  font-size: 3.2rem;
  line-height: 1;
  margin-bottom: 14px;
}

.card-name {
  font-size: 1.9rem;
  margin: 0;
}

.card-title {
  margin: 8px 0 20px;
  color: var(--muted);
  font-size: 0.96rem;
}

.stat-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: grid;
  gap: 12px;
  margin-top: auto;
}

.stat-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 10px;
  padding: 8px 10px;
  border-radius: 10px;
  background: rgba(255,255,255,0.04);
}

.stat-label {
  color: var(--muted);
}

.stat-value {
  font-weight: 700;
  color: var(--text);
}

.battle-center {
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  gap: 18px;
}

.attribute-picker {
  width: 100%;
  display: grid;
  gap: 10px;
}

.attribute-btn {
  width: 100%;
  padding: 12px 16px;
  border-radius: 12px;
  background: var(--panel-alt);
  color: var(--text);
  border: 1px solid rgba(255,255,255,0.08);
  font-weight: 700;
}

.attribute-btn.selected {
  background: linear-gradient(135deg, rgba(242,213,107,0.28), rgba(246,183,60,0.22));
  border-color: rgba(242,213,107,0.5);
  color: var(--gold);
}

.battle-btn {
  width: 100%;
  padding: 16px 18px;
  border-radius: 14px;
  background: linear-gradient(135deg, #7ef0a1, #63d8c0);
  color: #12241d;
  font-size: 1.1rem;
  font-weight: 800;
}

.result-panel {
  background: var(--panel);
  border: 1px solid rgba(255,255,255,0.08);
  border-radius: 16px;
  padding: 16px 18px;
}

.message {
  font-size: 1.05rem;
  line-height: 1.5;
  color: var(--text);
}

.message.win {
  color: var(--green);
}

.message.lose {
  color: var(--danger);
}

.message.info {
  color: var(--cyan);
}

@media (max-width: 860px) {
  .arena {
    grid-template-columns: 1fr;
  }

  .battle-center {
    order: -1;
  }
}
