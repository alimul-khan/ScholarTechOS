/* Colour themes. Any theme can be applied to any template.
 *
 * Variables (all become CSS custom properties on the stage):
 *   bg, text, muted, heading, clock     page background and text drawn directly on it
 *   panel, panelText, panelMuted        regular cards / tables
 *   hero, heroText, heroMuted           strong blocks (table headers, countdown boxes)
 *   accent, onAccent                    highlight for the upcoming prayer
 *   count                               countdown digits on a hero block
 *   ink                                 accent colour on the page background (icons, countdown)
 *   panelInk (optional)                 accent colour inside panels; defaults to ink
 *   line                                borders and ornament strokes
 *   pattern                             geometric pattern colour (with alpha)
 */
window.STOS_THEMES = [
  { id: 'sapphire', name: 'Midnight Sapphire', swatch: ['#062457', '#2fa8ff', '#ffffff'], vars: {
    bg: 'radial-gradient(120% 90% at 50% 0%, #0d4489 0%, #062457 45%, #031332 100%)',
    text: '#ffffff', muted: '#a9c8ee', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(4,24,60,.72)', panelText: '#ffffff', panelMuted: '#9cc2ee',
    hero: 'linear-gradient(180deg,#0c3a7a,#062a5c)', heroText: '#ffffff', heroMuted: '#a9c8ee',
    accent: 'linear-gradient(90deg,#15b4f5,#38dcff)', onAccent: '#031332',
    count: '#36d6ff', ink: '#36d6ff', line: '#2a9cf0', pattern: 'rgba(90,170,255,.16)' } },

  { id: 'ivory-teal', name: 'Ivory & Teal', swatch: ['#f6efe1', '#0e4a4f', '#d9a441'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 40%, #fffaf1 0%, #f4ead6 70%, #eadbbd 100%)',
    text: '#173c3e', muted: '#7a6545', heading: '#0f4448', clock: '#0f4448',
    panel: '#fffcf5', panelText: '#173c3e', panelMuted: '#8a7350',
    hero: 'linear-gradient(180deg,#145a5e,#0b3e42)', heroText: '#fff8ea', heroMuted: '#e0cba0',
    accent: 'linear-gradient(180deg,#f3d27d,#d9a441)', onAccent: '#2a1d05',
    count: '#f0c25e', ink: '#b9782d', line: '#c99a52', pattern: 'rgba(176,132,62,.22)' } },

  { id: 'maroon-gold', name: 'Maroon & Gold', swatch: ['#470a14', '#d4a24a', '#fff3dc'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 30%, #6e1220, #470a14 60%, #2b050b)',
    text: '#fff3dc', muted: '#e3c48e', heading: '#f2cf86', clock: '#fff6e6',
    panel: 'rgba(40,4,10,.55)', panelText: '#fff3dc', panelMuted: '#e0bf85',
    hero: 'linear-gradient(180deg,#5d0e1a,#3a0710)', heroText: '#fff3dc', heroMuted: '#e3c48e',
    accent: 'linear-gradient(180deg,#f6d98e,#d4a24a)', onAccent: '#3a0710',
    count: '#f4cf74', ink: '#f4cf74', line: '#d4a24a', pattern: 'rgba(222,170,90,.2)' } },

  { id: 'emerald-neon', name: 'Charcoal & Mint', swatch: ['#0f1614', '#3fdc9c', '#ffffff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 20%, #1d2a27, #0f1614 60%, #080c0b)',
    text: '#f2fff9', muted: '#a6c9bc', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(20,32,29,.85)', panelText: '#f2fff9', panelMuted: '#9fc5b8',
    hero: 'linear-gradient(180deg,#1a2a26,#111c19)', heroText: '#ffffff', heroMuted: '#a6c9bc',
    accent: 'linear-gradient(180deg,#5ff2b4,#2fd391)', onAccent: '#06231a',
    count: '#4cf0a8', ink: '#4cf0a8', line: '#3fdc9c', pattern: 'rgba(80,240,170,.12)' } },

  { id: 'amethyst', name: 'Royal Amethyst', swatch: ['#33104a', '#c48cf0', '#fbf6ff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 20%, #5a1a7a, #33104a 55%, #1c0729)',
    text: '#ffffff', muted: '#dcc0f2', heading: '#ffffff', clock: '#ffffff',
    panel: '#fbf6ff', panelText: '#2c0f40', panelMuted: '#6d4b85',
    hero: 'linear-gradient(180deg,#6c2896,#45136a)', heroText: '#ffffff', heroMuted: '#dcc0f2',
    accent: 'linear-gradient(180deg,#e7c8ff,#c99af0)', onAccent: '#2c0f40',
    count: '#d9a8ff', ink: '#d9a8ff', panelInk: '#7b3fb0', line: '#c48cf0', pattern: 'rgba(220,170,255,.16)' } },

  { id: 'terracotta', name: 'Terracotta & Cream', swatch: ['#f7ebdc', '#a8471f', '#f0a24e'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 30%, #fffaf3, #f7ebdc 70%, #efdac2)',
    text: '#4a1e0e', muted: '#8a5236', heading: '#5a2210', clock: '#4a1e0e',
    panel: '#fffaf4', panelText: '#4a1e0e', panelMuted: '#9a5c3c',
    hero: 'linear-gradient(180deg,#a8471f,#7a2f12)', heroText: '#fff4e8', heroMuted: '#f6cfa6',
    accent: 'linear-gradient(180deg,#f8c27a,#f0a24e)', onAccent: '#4a1e0e',
    count: '#ffd28a', ink: '#b5501f', line: '#d08050', pattern: 'rgba(200,100,50,.22)' } },

  { id: 'azure', name: 'Sky Azure', swatch: ['#eaf2fd', '#0f3f86', '#4fc3f7'], vars: {
    bg: 'linear-gradient(180deg,#f3f8ff,#dfeafa)',
    text: '#0b2a5c', muted: '#3d5f91', heading: '#0b2a5c', clock: '#0b2a5c',
    panel: '#ffffff', panelText: '#0b2a5c', panelMuted: '#3d5f91',
    hero: 'linear-gradient(180deg,#0f3f86,#082a60)', heroText: '#ffffff', heroMuted: '#b9d3f5',
    accent: 'linear-gradient(180deg,#8fe0ff,#4fc3f7)', onAccent: '#082a60',
    count: '#6fe3ff', ink: '#1557b0', line: '#2f6fc4', pattern: 'rgba(40,90,170,.14)' } },

  { id: 'onyx-gold', name: 'Onyx & Gold', swatch: ['#131210', '#e0a93a', '#fff8ea'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 20%, #24201a, #131210 60%, #0a0908)',
    text: '#fff8ea', muted: '#d6bf8e', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(20,18,15,.85)', panelText: '#fff8ea', panelMuted: '#cfb37a',
    hero: 'linear-gradient(180deg,#2a251d,#18150f)', heroText: '#fff8ea', heroMuted: '#d6bf8e',
    accent: 'linear-gradient(180deg,#f7d27a,#e0a93a)', onAccent: '#1a1206',
    count: '#f3c351', ink: '#f3c351', line: '#c9a24f', pattern: 'rgba(220,180,90,.14)' } },

  { id: 'sage', name: 'Soft Sage', swatch: ['#e9eedf', '#64804b', '#1f2b18'], vars: {
    bg: 'linear-gradient(180deg,#eef2e6,#e3e9d8)',
    text: '#23331c', muted: '#55664a', heading: '#2f4a23', clock: '#1f2b18',
    panel: '#fbfcf7', panelText: '#23331c', panelMuted: '#5c6d50',
    hero: 'linear-gradient(180deg,#7d9a62,#64804b)', heroText: '#ffffff', heroMuted: '#e7f0db',
    accent: 'linear-gradient(180deg,#d3e2bf,#bcd0a2)', onAccent: '#1f2b18',
    count: '#ffffff', ink: '#4f6b38', line: '#b9c8a4', pattern: 'rgba(90,120,60,.12)' } },

  { id: 'teal-coral', name: 'Deep Teal & Coral', swatch: ['#0b3f49', '#f45b4c', '#ffffff'], vars: {
    bg: 'linear-gradient(180deg,#0d4a55,#08343d)',
    text: '#ffffff', muted: '#b6d6da', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(5,40,48,.6)', panelText: '#ffffff', panelMuted: '#a9cdd2',
    hero: 'linear-gradient(180deg,#ff6f5e,#f2564a)', heroText: '#ffffff', heroMuted: '#ffe1dc',
    accent: 'linear-gradient(180deg,#ff7a68,#f45b4c)', onAccent: '#ffffff',
    count: '#ffffff', ink: '#ff8a78', line: '#5bb7c2', pattern: 'rgba(120,210,220,.1)' } },

  { id: 'mono', name: 'Monochrome', swatch: ['#f4f4f2', '#111111', '#cfcfcf'], vars: {
    bg: '#f4f4f2',
    text: '#111111', muted: '#555555', heading: '#111111', clock: '#111111',
    panel: '#ffffff', panelText: '#111111', panelMuted: '#555555',
    hero: '#111111', heroText: '#ffffff', heroMuted: '#bbbbbb',
    accent: '#111111', onAccent: '#ffffff',
    count: '#ffffff', ink: '#111111', line: '#c4c4c4', pattern: 'rgba(0,0,0,.07)' } },

  { id: 'indigo-rose', name: 'Indigo & Rose', swatch: ['#141f40', '#ef9f92', '#ffffff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 0%, #22325e, #141f40 60%, #0c1430)',
    text: '#ffffff', muted: '#b9c3e3', heading: '#f4b4a8', clock: '#ffffff',
    panel: 'rgba(15,24,52,.8)', panelText: '#ffffff', panelMuted: '#b6c0e0',
    hero: 'linear-gradient(180deg,#1b2a55,#121d40)', heroText: '#ffffff', heroMuted: '#b9c3e3',
    accent: 'linear-gradient(180deg,#f8c3b8,#ef9f92)', onAccent: '#2a1430',
    count: '#f6a597', ink: '#f6a597', line: '#e9a497', pattern: 'rgba(240,160,150,.12)' } },

  { id: 'bronze', name: 'Antique Bronze', swatch: ['#24160a', '#c99545', '#fff2dc'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 20%, #3a2410, #24160a 60%, #140c05)',
    text: '#fff2dc', muted: '#d9b98a', heading: '#fff2dc', clock: '#fff6e8',
    panel: 'rgba(30,18,8,.7)', panelText: '#fff2dc', panelMuted: '#d9b98a',
    hero: 'linear-gradient(180deg,#3a2612,#25170a)', heroText: '#fff2dc', heroMuted: '#d9b98a',
    accent: 'linear-gradient(180deg,#f2cf8a,#c99545)', onAccent: '#2a1a08',
    count: '#f0c46c', ink: '#f0c46c', line: '#b58a4a', pattern: 'rgba(220,170,100,.12)' } },

  { id: 'arctic', name: 'Arctic Blue', swatch: ['#ffffff', '#0e6aa3', '#a7e6f5'], vars: {
    bg: 'linear-gradient(180deg,#ffffff,#eef5fc)',
    text: '#0b2550', muted: '#3a5a8a', heading: '#0b2550', clock: '#0b2550',
    panel: '#eaf4fd', panelText: '#0b2550', panelMuted: '#3a5a8a',
    hero: 'linear-gradient(180deg,#0e6aa3,#0a4f80)', heroText: '#ffffff', heroMuted: '#cde6f7',
    accent: 'linear-gradient(180deg,#b9eef9,#a1e2f3)', onAccent: '#0b2550',
    count: '#ffffff', ink: '#0e6aa3', line: '#bcd6ee', pattern: 'rgba(30,90,160,.08)' } },

  { id: 'plum-salmon', name: 'Plum & Salmon', swatch: ['#160e34', '#f47460', '#ffffff'], vars: {
    bg: 'linear-gradient(180deg,#1c1240,#120b2c)',
    text: '#ffffff', muted: '#c7bde8', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(35,24,75,.75)', panelText: '#ffffff', panelMuted: '#c0b5e2',
    hero: 'linear-gradient(180deg,#ff8a75,#f26f5c)', heroText: '#1c1240', heroMuted: '#3a2050',
    accent: 'linear-gradient(180deg,#ff8f7a,#f47460)', onAccent: '#1c1240',
    count: '#1c1240', ink: '#ff8f7a', line: '#ff8f7a', pattern: 'rgba(255,140,120,.1)' } },

  { id: 'parchment', name: 'Parchment & Olive', swatch: ['#efe3c5', '#4f5e2a', '#e5bd45'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 40%, #faf3e1, #efe3c5 70%, #e3d3a8)',
    text: '#2d3215', muted: '#5f5a35', heading: '#2f3a16', clock: '#2a3313',
    panel: '#fbf5e4', panelText: '#2d3215', panelMuted: '#6b6540',
    hero: 'linear-gradient(180deg,#4f5e2a,#3a4720)', heroText: '#fffbe8', heroMuted: '#e3dcb4',
    accent: 'linear-gradient(180deg,#f6dc7e,#e5bd45)', onAccent: '#2d2a10',
    count: '#f6dc7e', ink: '#8a6a12', line: '#c9b27a', pattern: 'rgba(150,120,50,.2)' } },

  { id: 'cobalt', name: 'Cobalt', swatch: ['#123a8f', '#ffffff', '#cfe9ff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 0%, #1d4fb8, #123a8f 55%, #0b2668)',
    text: '#ffffff', muted: '#cfe0ff', heading: '#ffffff', clock: '#ffffff',
    panel: '#ffffff', panelText: '#0b2668', panelMuted: '#3b5591',
    hero: 'linear-gradient(180deg,#0d2f7d,#0a2463)', heroText: '#ffffff', heroMuted: '#cfe0ff',
    accent: 'linear-gradient(180deg,#d9eeff,#bfe0ff)', onAccent: '#0b2668',
    count: '#ffffff', ink: '#bfe0ff', panelInk: '#1d4fb8', line: '#8fb3f0', pattern: 'rgba(150,190,255,.18)' } },

  { id: 'graphite-gold', name: 'Graphite & Champagne', swatch: ['#1f1f1f', '#cfa560', '#ffffff'], vars: {
    bg: 'linear-gradient(180deg,#2b2b2b,#161616)',
    text: '#ffffff', muted: '#d8c39a', heading: '#f4e2bd', clock: '#ffffff',
    panel: 'rgba(12,12,12,.7)', panelText: '#ffffff', panelMuted: '#d8c39a',
    hero: 'linear-gradient(180deg,#232323,#121212)', heroText: '#ffffff', heroMuted: '#d8c39a',
    accent: 'linear-gradient(180deg,#ecd096,#c99d55)', onAccent: '#1a1408',
    count: '#ffffff', ink: '#e2c07f', line: '#c7a160', pattern: 'rgba(220,190,130,.1)' } },

  { id: 'mint', name: 'Mint Lagoon', swatch: ['#dff3eb', '#0f5a52', '#ffffff'], vars: {
    bg: 'linear-gradient(180deg,#e6f6f0,#d3eee4)',
    text: '#0e3b37', muted: '#3e6a63', heading: '#0f3e3a', clock: '#0e3b37',
    panel: '#f7fcfa', panelText: '#0e3b37', panelMuted: '#4c766f',
    hero: 'linear-gradient(180deg,#0f5a52,#0a443e)', heroText: '#ffffff', heroMuted: '#c3e6dd',
    accent: 'linear-gradient(180deg,#126a60,#0c4c45)', onAccent: '#ffffff',
    count: '#ffffff', ink: '#0f5a52', line: '#b6dccf', pattern: 'rgba(15,90,80,.08)' } },

  { id: 'burgundy-ice', name: 'Burgundy & Ice', swatch: ['#4a0818', '#bfe0f7', '#ffffff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 30%, #6a0f24, #4a0818 60%, #300410)',
    text: '#ffffff', muted: '#f1c9d2', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(60,6,20,.55)', panelText: '#ffffff', panelMuted: '#f0c6cf',
    hero: 'linear-gradient(180deg,#d8ecfb,#bcdcf3)', heroText: '#3a0612', heroMuted: '#6b2236',
    accent: 'linear-gradient(180deg,#d6ecfb,#b5d9f2)', onAccent: '#3a0612',
    count: '#3a0612', ink: '#bfe0f7', line: '#bfe0f7', pattern: 'rgba(255,255,255,.14)' } },

  { id: 'forest-gold', name: 'Forest & Gold', swatch: ['#0a3324', '#d9ab4a', '#fff8e6'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 20%, #0f4a33, #0a3324 60%, #052016)',
    text: '#fff8e6', muted: '#e2cf9c', heading: '#fff8e6', clock: '#fff8e6',
    panel: 'rgba(5,35,24,.7)', panelText: '#fff8e6', panelMuted: '#e2cf9c',
    hero: 'linear-gradient(180deg,#0e4532,#08301f)', heroText: '#fff8e6', heroMuted: '#e2cf9c',
    accent: 'linear-gradient(180deg,#f6d886,#d9ab4a)', onAccent: '#1d1405',
    count: '#f2cc6b', ink: '#f2cc6b', line: '#cfa451', pattern: 'rgba(220,180,90,.16)' } },

  { id: 'night-cyan', name: 'Night & Cyan', swatch: ['#070d13', '#29d3f5', '#ffffff'], vars: {
    bg: 'radial-gradient(120% 100% at 50% 50%, #0e1c26, #070d13 70%)',
    text: '#ffffff', muted: '#9fb4c2', heading: '#ffffff', clock: '#ffffff',
    panel: 'rgba(12,22,30,.8)', panelText: '#ffffff', panelMuted: '#93aab9',
    hero: 'rgba(16,30,40,.95)', heroText: '#ffffff', heroMuted: '#9fb4c2',
    accent: 'linear-gradient(90deg,rgba(41,211,245,.28),rgba(41,211,245,.12))', onAccent: '#29d3f5',
    count: '#29d3f5', ink: '#29d3f5', line: '#2a8fae', pattern: 'rgba(40,200,240,.18)' } }
];
