/* 3D battlefield: Three.js scene — cinematic table, deck stacks, discard piles, particles,
   and two physical cards (DOM text stays the accessible source; 3D cards mirror it). */
import * as THREE from './three.module.min.js';
const RARITY_COLOR = { Common: '#b9c4cc', Uncommon: '#6fe08a', Rare: '#57b6ff', Epic: '#c07bff', Legendary: '#ffb340', Mythic: '#ff5f8f' };
const LABEL = { power: 'Power', speed: 'Speed', height: 'Height', defense: 'Defense', intelligence: 'Intelligence', stamina: 'Stamina', luck: 'Luck', age: 'Age' };

export class Battle3D {
  constructor(canvas) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.scene = new THREE.Scene(); this.scene.fog = new THREE.FogExp2(0x0b0714, 0.028);
    this.camera = new THREE.PerspectiveCamera(42, 1, 0.1, 100); this.camera.position.set(0, 7.4, 10.5);
    this.scene.add(new THREE.AmbientLight(0x8877bb, 1.1));
    const key = new THREE.PointLight(0xffc873, 220, 40); key.position.set(-4, 7, 3); this.scene.add(key);
    const rim = new THREE.PointLight(0x6fb7ff, 160, 40); rim.position.set(5, 5, -4); this.scene.add(rim);
    // Table: dark wood disc + inlaid gold ring + rune circle texture
    const felt = document.createElement('canvas'); felt.width = felt.height = 1024; const g = felt.getContext('2d');
    const grad = g.createRadialGradient(512, 512, 80, 512, 512, 512); grad.addColorStop(0, '#2c1c46'); grad.addColorStop(.7, '#1a1130'); grad.addColorStop(1, '#0d0819');
    g.fillStyle = grad; g.fillRect(0, 0, 1024, 1024); g.strokeStyle = '#c9a24d'; g.lineWidth = 6;
    g.beginPath(); g.arc(512, 512, 380, 0, 7); g.stroke(); g.lineWidth = 2; g.beginPath(); g.arc(512, 512, 330, 0, 7); g.stroke();
    g.fillStyle = 'rgba(242,213,107,.75)'; g.font = '42px serif'; g.textAlign = 'center';
    '✦✧✦✧✦✧✦✧'.split('').forEach((ch, i) => { const a = i / 8 * Math.PI * 2; g.fillText(ch, 512 + Math.cos(a) * 355, 512 + Math.sin(a) * 355); });
    const table = new THREE.Mesh(new THREE.CylinderGeometry(7.6, 8.1, .55, 64), new THREE.MeshStandardMaterial({ map: new THREE.CanvasTexture(felt), roughness: .55, metalness: .25 }));
    table.position.y = -.3; this.scene.add(table);
    const base = new THREE.Mesh(new THREE.CylinderGeometry(3.4, 4.4, 2.6, 8), new THREE.MeshStandardMaterial({ color: 0x241631, roughness: .8 }));
    base.position.y = -1.9; this.scene.add(base);
    // Ambient particles
    const N = 260, pos = new Float32Array(N * 3);
    for (let i = 0; i < N; i++) { pos[i * 3] = (Math.random() - .5) * 18; pos[i * 3 + 1] = Math.random() * 7; pos[i * 3 + 2] = (Math.random() - .5) * 14; }
    const pgeo = new THREE.BufferGeometry(); pgeo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    this.particles = new THREE.Points(pgeo, new THREE.PointsMaterial({ color: 0xd9b6ff, size: .07, transparent: true, opacity: .75 }));
    this.scene.add(this.particles);
    // Deck stacks + discard piles (zone markers)
    this.zones = {};
    const zoneMat = new THREE.MeshStandardMaterial({ color: 0x17102a, roughness: .4, metalness: .5, emissive: 0x2a1c4d });
    [[-5.4, -2.2, 'aiDeck'], [5.4, 2.2, 'playerDeck'], [-5.4, 2.2, 'playerDiscard'], [5.4, -2.2, 'aiDiscard']].forEach(([x, z, name]) => {
      const m = new THREE.Mesh(new THREE.BoxGeometry(1.5, .5, 2.15), zoneMat); m.position.set(x, .25, z); m.rotation.y = .12; this.scene.add(m); this.zones[name] = m;
    });
    this.playerCard = this._makeCardMesh(-2.35, .06); this.oppCard = this._makeCardMesh(2.35, Math.PI);
    this.oppCard.rotation.y = Math.PI; // opponent card starts face-down
    this.targets = { player: 0, opp: Math.PI };
    this.clock = new THREE.Clock(); this.pointer = { x: 0, y: 0 };
    addEventListener('pointermove', e => { this.pointer.x = (e.clientX / innerWidth - .5); this.pointer.y = (e.clientY / innerHeight - .5); });
    this._resize(); addEventListener('resize', () => this._resize());
    this.renderer.setAnimationLoop(() => this._frame());
  }
  _resize() { const w = this.renderer.domElement.clientWidth || innerWidth, h = this.renderer.domElement.clientHeight || 420; this.renderer.setSize(w, h, false); this.camera.aspect = w / h; this.camera.updateProjectionMatrix(); }
  _makeCardMesh(x, ry) {
    const grp = new THREE.Group();
    const geo = new THREE.BoxGeometry(2.15, .06, 3.15);
    const edge = new THREE.MeshStandardMaterial({ color: 0x120c20, roughness: .35, metalness: .6 });
    const mesh = new THREE.Mesh(geo, [edge, edge, edge, edge, edge, edge]); grp.add(mesh); grp.position.set(x, .18, 0); grp.rotation.y = ry;
    grp.userData.mesh = mesh; this.scene.add(grp); return grp;
  }
  async _cardTexture(card, attrs, backUrl) {
    const c = document.createElement('canvas'); c.width = 512; c.height = 768; const g = c.getContext('2d');
    if (!card) { const img = await loadImage(backUrl); g.drawImage(img, 0, 0, 512, 768); return new THREE.CanvasTexture(c); }
    const img = await loadImage(card.artwork); // relative: works under sub-path hosting too
    g.fillStyle = '#0d0a18'; g.fillRect(0, 0, 512, 768); g.drawImage(img, 14, 64, 484, 430);
    g.strokeStyle = RARITY_COLOR[card.rarity]; g.lineWidth = 12; g.strokeRect(6, 6, 500, 756);
    g.fillStyle = '#f4ead2'; g.font = 'bold 30px Georgia,serif'; g.textAlign = 'center';
    g.fillText(card.name.split(',')[0].slice(0, 26), 256, 42);
    g.font = '19px Georgia,serif'; g.fillStyle = RARITY_COLOR[card.rarity]; g.fillText(`${card.rarity} · ${card.category}`, 256, 522);
    g.font = '17px Arial'; g.fillStyle = '#e8e2f2'; g.textAlign = 'left';
    Object.keys(LABEL).forEach((a, i) => { const x = 30 + (i % 2) * 245, y = 562 + Math.floor(i / 2) * 48; g.fillText(`${LABEL[a]}:  ${attrs[a]}`, x, y); });
    const t = new THREE.CanvasTexture(c); t.anisotropy = 4; return t;
  }
  async setCard(side, card, attrs, backUrl) {
    const grp = side === 'player' ? this.playerCard : this.oppCard;
    const tex = await this._cardTexture(card, attrs, backUrl);
    const mats = grp.userData.mesh.material.slice(); mats[2] = new THREE.MeshStandardMaterial({ map: tex, roughness: .3, metalness: .15 }); // top face
    grp.userData.mesh.material = mats;
    this.targets[side] = side === 'player' ? 0 : (card ? 0 : Math.PI);
    if (side === 'opp' && !card) grp.rotation.y = Math.PI;
  }
  flipOpponent() { this.targets.opp = 0; }
  celebrate(side) { const grp = side === 'player' ? this.playerCard : this.oppCard; grp.userData.celebrate = 1; }
  _frame() {
    const t = this.clock.getElapsedTime();
    for (const [grp, key] of [[this.playerCard, 'player'], [this.oppCard, 'opp']]) {
      grp.rotation.y += (this.targets[key] - grp.rotation.y) * .09;
      const lift = grp.userData.celebrate ? Math.abs(Math.sin(t * 7)) * .55 : Math.sin(t * 1.4 + grp.position.x) * .05;
      grp.position.y = .18 + lift; if (grp.userData.celebrate && lift < .06) grp.userData.celebrate = 0;
    }
    this.particles.rotation.y = t * .02; this.particles.position.y = Math.sin(t * .5) * .15;
    this.camera.position.x += ((this.pointer.x * 1.6) - this.camera.position.x) * .03;
    this.camera.position.y += ((7.4 - this.pointer.y * .8) - this.camera.position.y) * .03;
    this.camera.lookAt(0, 0, 0); this.renderer.render(this.scene, this.camera);
  }
}
function loadImage(src) { return new Promise((res, rej) => { const i = new Image(); i.onload = () => res(i); i.onerror = rej; i.src = src; }); }
