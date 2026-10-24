// The same cases through adhan.js (run with TZ=UTC so local dates are UTC dates, like the C side).
const fs = require('fs'), vm = require('vm');
const ctx = {}; vm.runInNewContext(fs.readFileSync(process.argv[2], 'utf8') + ';this.adhan=adhan', ctx);
const a = ctx.adhan;
const methods = ['NorthAmerica', 'MuslimWorldLeague', 'Egyptian', 'Karachi', 'UmmAlQura', 'Dubai', 'MoonsightingCommittee', 'Kuwait', 'Qatar', 'Singapore', 'Tehran', 'Turkey'];
const places = [[50.4452, -104.6189], [21.4225, 39.8262], [51.5072, -0.1276], [-33.8688, 151.2093], [64.1466, -21.9426], [59.9139, 10.7522], [-54.8019, -68.303], [1.3521, 103.8198]];
const rules = ['middleofthenight', 'seventhofthenight', 'twilightangle', 'auto'];
const out = [];
const s = d => isNaN(d) ? -1 : Math.round(d.getTime() / 1000);
for (const m of methods) for (let p = 0; p < 8; p++) for (let hl = 0; hl < 4; hl++) for (let day = 0; day < 366; day += 9) {
  const date = new Date(2026, 0, 1 + day, 12);
  for (let hanafi = 0; hanafi < 2; hanafi++) {
    const params = a.CalculationMethod[m]();
    params.madhab = hanafi ? a.Madhab.Hanafi : a.Madhab.Shafi;
    const coords = new a.Coordinates(places[p][0], places[p][1]);
    params.highLatitudeRule = rules[hl] === 'auto' ? a.HighLatitudeRule.recommended(coords) : rules[hl];
    const t = new a.PrayerTimes(coords, date, params);
    const ds = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
    out.push(`${m} ${p} ${hl} ${ds} ${hanafi} ${s(t.fajr)} ${s(t.sunrise)} ${s(t.dhuhr)} ${s(t.asr)} ${s(t.maghrib)} ${s(t.isha)}`);
  }
}
console.log(out.join('\n'));
