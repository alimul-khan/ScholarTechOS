// Makes the C tables the firmware needs from the display app and from this Mac:
//   src/gen/themes.c     colour themes, and each template's own theme (js/themes.js, js/templates.js)
//   src/gen/zones.c      time-zone names -> POSIX rules (the last line of each /usr/share/zoneinfo file)
//   src/gen/hijri.c      Umm al-Qura month starts (from Node's Intl, the same calendar the web pages use)
//   src/gen/countries.c  country code -> name (js/countries.js), for the place search
// Run: node tools/gen_tables.mjs <path to the display app, e.g. Android_TV_app/prayerdisplay/src/main/assets/stos>
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';

const app = process.argv[2];
if (!app) { console.error('usage: node tools/gen_tables.mjs <display app folder>'); process.exit(1); }
const out = path.join(path.dirname(new URL(import.meta.url).pathname), '..', 'src', 'gen');
fs.mkdirSync(out, { recursive: true });
const win = {};
vm.runInNewContext(['themes.js', 'templates.js', 'countries.js'].map(f => fs.readFileSync(path.join(app, 'js', f), 'utf8')).join('\n'), { window: win });
const q = s => JSON.stringify(String(s));

// ---- themes ----
function colours(v) {
  const found = [];
  String(v).replace(/#([0-9a-f]{6}|[0-9a-f]{3})\b|rgba?\(([^)]+)\)/gi, (m, hex, rgb) => {
    if (hex) {
      if (hex.length === 3) hex = hex.split('').map(c => c + c).join('');
      found.push({ c: parseInt(hex, 16), a: 255 });
    } else {
      const p = rgb.split(',').map(x => parseFloat(x));
      found.push({ c: (p[0] << 16) | (p[1] << 8) | p[2], a: Math.round((p.length > 3 ? p[3] : 1) * 255) });
    }
  });
  return found;
}
function paint(v) {           // {c1, c2, opa, grad}: grad 0 none, 1 vertical, 2 horizontal, 3 radial
  const cs = colours(v);
  if (!cs.length) return '{0x000000, 0x000000, 255, 0}';
  const a = cs[0], b = cs[cs.length - 1];
  let grad = 0;
  if (/radial-gradient/.test(v)) grad = 3; else if (/linear-gradient\(\s*90deg/.test(v)) grad = 2; else if (/linear-gradient/.test(v)) grad = 1;
  const hx = n => '0x' + n.toString(16).padStart(6, '0');
  return `{${hx(a.c)}, ${hx(b.c)}, ${a.a}, ${grad}}`;
}
const KEYS = ['bg', 'text', 'muted', 'heading', 'clock', 'panel', 'panelText', 'panelMuted', 'hero', 'heroText', 'heroMuted',
  'accent', 'onAccent', 'count', 'ink', 'panelInk', 'line', 'pattern'];
let c = '/* Made by tools/gen_tables.mjs from js/themes.js and js/templates.js. Do not edit. */\n#include "themes.h"\n\n';
c += 'const theme_t THEMES[] = {\n';
for (const t of win.STOS_THEMES) {
  const v = Object.assign({}, t.vars);
  if (!v.panelInk) v.panelInk = v.ink;
  c += `  {${q(t.id)}, ${q(t.name)}, {\n` + KEYS.map(k => `    ${paint(v[k])}, /* ${k} */`).join('\n') + '\n  }},\n';
}
c += '};\nconst int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);\n\n';
c += 'const template_theme_t TEMPLATE_THEMES[] = {\n' + win.STOS_TEMPLATES.map(t => `  {${q(t.id)}, ${q(t.theme)}},`).join('\n') + '\n};\n';
c += 'const int TEMPLATE_COUNT = sizeof(TEMPLATE_THEMES) / sizeof(TEMPLATE_THEMES[0]);\n\n';
c += 'const template_theme_t TEMPLATE_ALIASES[] = {\n' + Object.entries(win.STOS_TEMPLATE_ALIASES).map(([a, b]) => `  {${q(a)}, ${q(b)}},`).join('\n') + '\n};\n';
c += 'const int TEMPLATE_ALIAS_COUNT = sizeof(TEMPLATE_ALIASES) / sizeof(TEMPLATE_ALIASES[0]);\n';
fs.writeFileSync(path.join(out, 'themes.c'), c);

// ---- time zones ----
const zones = [];
const root = '/usr/share/zoneinfo';
(function walk(dir) {
  for (const f of fs.readdirSync(dir)) {
    const p = path.join(dir, f), rel = path.relative(root, p);
    if (/^(posix|right|Etc|SystemV)$/.test(rel) || /[a-z]+\.(tab|zi)$|^\+VERSION|leap|tzdata|posixrules|Factory/.test(f)) continue;
    const st = fs.statSync(p);
    if (st.isDirectory()) { walk(p); continue; }
    const buf = fs.readFileSync(p);
    if (buf.slice(0, 4).toString() !== 'TZif') continue;
    const text = buf.toString('latin1').trimEnd(), last = text.slice(text.lastIndexOf('\n') + 1);
    if (/^[A-Za-z<]/.test(last) && last.length < 64) zones.push([rel, last]);
  }
})(root);
for (let h = -14; h <= 12; h++) zones.push(['Etc/GMT' + (h > 0 ? '+' + h : h < 0 ? h : ''), `<${h > 0 ? '-' : '+'}${String(Math.abs(h)).padStart(2, '0')}>${h}`]);
zones.push(['UTC', 'UTC0']);
zones.sort((a, b) => a[0] < b[0] ? -1 : 1);
c = '/* Made by tools/gen_tables.mjs from this Mac\'s /usr/share/zoneinfo. Do not edit. */\n#include "tzdb.h"\n\nconst tz_entry_t TZ_DB[] = {\n';
c += zones.map(([n, p]) => `  {${q(n)}, ${q(p)}},`).join('\n') + '\n};\nconst int TZ_DB_COUNT = sizeof(TZ_DB) / sizeof(TZ_DB[0]);\n';
fs.writeFileSync(path.join(out, 'zones.c'), c);

// ---- Hijri (Umm al-Qura) ----
const fmt = new Intl.DateTimeFormat('en-GB-u-ca-islamic-umalqura', { timeZone: 'UTC', day: 'numeric', month: 'numeric', year: 'numeric' });
const starts = [];
let first = null;
for (let ms = Date.UTC(2015, 0, 1); ms < Date.UTC(2077, 0, 1); ms += 86400000) {
  const parts = Object.fromEntries(fmt.formatToParts(new Date(ms)).map(p => [p.type, p.value]));
  if (+parts.day !== 1) continue;
  const jdn = Math.round(ms / 86400000 + 2440587.5 + 0.5);
  if (!first) first = { year: parseInt(parts.year || parts.relatedYear, 10), month: +parts.month };
  starts.push(jdn);
}
c = '/* Made by tools/gen_tables.mjs from Node\'s Umm al-Qura calendar. Do not edit. */\n#include <stdint.h>\n\n';
c += `const int HIJRI_FIRST_YEAR = ${first.year};\nconst int HIJRI_FIRST_MONTH = ${first.month};\n`;
c += `const int HIJRI_MONTHS_COUNT = ${starts.length};\n/* Julian day number of the 1st of each month, from the first month above. */\nconst uint32_t HIJRI_MONTH_START[] = {\n`;
for (let i = 0; i < starts.length; i += 10) c += '  ' + starts.slice(i, i + 10).join(', ') + ',\n';
c += '};\n';
fs.writeFileSync(path.join(out, 'hijri.c'), c);

// ---- countries ----
const list = String(win.STOS_COUNTRIES.map ? win.STOS_COUNTRIES.map(x => x.name + ':' + x.code).join(',') : win.STOS_COUNTRIES)
  .split(',').map(s => s.split(':')).filter(p => p.length === 2);
c = '/* Made by tools/gen_tables.mjs from js/countries.js. Do not edit. */\n#include "places.h"\n\nconst country_t COUNTRIES[] = {\n';
c += list.map(([n, k]) => `  {${q(k)}, ${q(n)}},`).join('\n') + '\n};\nconst int COUNTRY_COUNT = sizeof(COUNTRIES) / sizeof(COUNTRIES[0]);\n';
fs.writeFileSync(path.join(out, 'countries.c'), c);
console.log(`themes ${win.STOS_THEMES.length}, templates ${win.STOS_TEMPLATES.length}, zones ${zones.length}, hijri months ${starts.length} from ${first.year}/${first.month}, countries ${list.length}`);
