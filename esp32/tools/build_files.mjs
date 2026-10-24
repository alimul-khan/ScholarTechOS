// Builds what goes next to the firmware: build/www.bin (the settings page, a LittleFS image) and build/places.bin
// (the offline city list). Run: node tools/build_files.mjs <Android_TV_app/prayerdisplay/src/main/assets>
// The settings page is the Android app's copy of display_template (assets/stos), with a few changes for this screen:
// the 9-digit passphrase instead of a 6-digit PIN (as prayerdisplay.online does), and esp32.css / esp32.js, which hide
// what does not apply to a 480x480 screen.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { execFileSync } from 'node:child_process';
import os from 'node:os';

const assets = process.argv[2];
if (!assets) { console.error('usage: node tools/build_files.mjs <path to prayerdisplay/src/main/assets>'); process.exit(1); }
const here = path.dirname(new URL(import.meta.url).pathname);
const root = path.join(here, '..');
const build = path.join(root, 'build');
const stage = path.join(build, 'www');
fs.rmSync(stage, { recursive: true, force: true });
fs.mkdirSync(stage, { recursive: true });

const SKIP = new Set(['img/splash-prayer.gif', 'splash.html', 'player.html']);
const TEXT = new Set(['.html', '.css', '.js', '.json', '.svg', '.txt']);
let raw = 0, packed = 0, files = 0;
function add(rel, data) {
  const ext = path.extname(rel).toLowerCase();
  const dest = path.join(stage, rel);
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  raw += data.length;
  if (TEXT.has(ext)) {
    const gz = zlib.gzipSync(data, { level: 9 });
    fs.writeFileSync(dest + '.gz', gz);
    packed += gz.length;
  } else {
    fs.writeFileSync(dest, data);
    packed += data.length;
  }
  files++;
}

const src = path.join(assets, 'stos');
(function walk(dir) {
  for (const f of fs.readdirSync(dir)) {
    const p = path.join(dir, f), rel = path.relative(src, p).split(path.sep).join('/');
    if (fs.statSync(p).isDirectory()) { walk(p); continue; }
    if (SKIP.has(rel) || f.startsWith('.')) continue;
    let data = fs.readFileSync(p);
    if (rel === 'js/auth.js') {
      // Same change as prayerdisplay.online (server/lib/screens.js): the screen's passphrase has 9 digits.
      data = Buffer.from(data.toString('utf8')
        .replace(/\{6\}/g, '{9}').replace(/maxlength="6"/g, 'maxlength="9"').replace(/slice\(0, 6\)/g, 'slice(0, 9)')
        .replace(/6 digits/g, '9 digits').replace(/6-digit/g, '9-digit')
        .replace(/(?<![-\w])PIN(?![-\w])/g, 'passphrase')
        .replace('if (a.ok) security();', 'if (a.ok) { /* the passphrase is made on the screen: no Security card */ }'));
    }
    if (rel === 'manage.html') {
      data = Buffer.from(data.toString('utf8')
        .replace('</head>', '  <link rel="stylesheet" href="esp32.css?v=1">\n</head>')
        .replace('</body>', '  <script src="esp32.js?v=1"></script>\n</body>'));
    }
    add(rel, data);
  }
})(src);
for (const f of ['esp32.css', 'esp32.js']) add(f, fs.readFileSync(path.join(root, 'www', f)));
// The start-up logo, as PNG (the screen cannot read WebP).
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'pd-'));
execFileSync('sips', ['-s', 'format', 'png', '--resampleWidth', '300', path.join(src, 'img', 'splash-logo.webp'), '--out', path.join(tmp, 'logo.png')], { stdio: 'ignore' });
add('screen/logo.png', fs.readFileSync(path.join(tmp, 'logo.png')));
fs.writeFileSync(path.join(stage, 'VERSION'), fs.readFileSync(path.join(src, 'VERSION')));

// LittleFS image the size of the www partition (see partitions.csv).
const mk = path.join(os.homedir(), '.platformio', 'packages', 'tool-mklittlefs', 'mklittlefs');
execFileSync(mk, ['-c', stage, '-b', '4096', '-p', '256', '-s', String(0x280000), path.join(build, 'www.bin')], { stdio: 'inherit' });

// places.bin: "PLACES1\n", the length as 8 hex digits and "\n", then the list (one place per line, see Places.java).
const places = fs.readFileSync(path.join(assets, 'places.tsv'));
const head = Buffer.from('PLACES1\n' + places.length.toString(16).padStart(8, '0') + '\n');
if (head.length + places.length > 0x580000) throw new Error('places.tsv is too big for the places partition');
fs.writeFileSync(path.join(build, 'places.bin'), Buffer.concat([head, places]));
console.log(`settings page: ${files} files, ${(raw / 1024).toFixed(0)} KB -> ${(packed / 1024).toFixed(0)} KB; places ${(places.length / 1048576).toFixed(1)} MB`);
