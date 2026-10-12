/* Daily prayer times from the chosen source, for every screen and preview.
 *
 *   cfg.prayerSource  'manual'      the fixed times in cfg.times (as before)
 *                     'calculated'  from cfg.calc (latitude, longitude, method, Asr rule)
 *                     'calendar'    from an uploaded timetable, cfg.calendar['YYYY-MM-DD']
 *   cfg.iqamahRules   per prayer: { mode: 'offset', minutes } or { mode: 'fixed', time }
 *                     used when the source gives no iqamah time
 *
 * Jumua times always come from cfg.times. A calendar date that is missing falls back to the
 * calculation (if a location is set), otherwise to the fixed times.
 * Load after core.js (and vendor/adhan.js); it hooks STOS.mount so templates need no changes. */
(function () {
  var STOS = window.STOS;
  var PRAYERS = ['fajr', 'dhuhr', 'asr', 'maghrib', 'isha'];
  var COLUMNS = ['fajr', 'sunrise', 'dhuhr', 'asr', 'maghrib', 'isha'];

  var METHODS = [
    ['NorthAmerica', 'ISNA (North America)'], ['MuslimWorldLeague', 'Muslim World League'],
    ['Egyptian', 'Egyptian General Authority'], ['Karachi', 'University of Islamic Sciences, Karachi'],
    ['UmmAlQura', 'Umm al-Qura, Makkah'], ['Dubai', 'Dubai'], ['MoonsightingCommittee', 'Moonsighting Committee'],
    ['Kuwait', 'Kuwait'], ['Qatar', 'Qatar'], ['Singapore', 'Singapore'], ['Tehran', 'Tehran'], ['Turkey', 'Turkey (Diyanet)']
  ];
  var DEFAULT_RULES = {
    fajr: { mode: 'offset', minutes: 20, time: '' }, dhuhr: { mode: 'offset', minutes: 15, time: '' },
    asr: { mode: 'offset', minutes: 15, time: '' }, maghrib: { mode: 'offset', minutes: 5, time: '' },
    isha: { mode: 'offset', minutes: 15, time: '' }
  };
  STOS.DEFAULTS.prayerSource = STOS.DEFAULTS.prayerSource || 'manual';
  STOS.DEFAULTS.calc = STOS.DEFAULTS.calc || { lat: '', lng: '', method: 'NorthAmerica', madhab: 'shafi', highLat: 'auto' };
  STOS.DEFAULTS.iqamahRules = STOS.DEFAULTS.iqamahRules || JSON.parse(JSON.stringify(DEFAULT_RULES));
  // Dim the screen during the congregation: from iqamah for N minutes (per prayer; 0 = off).
  STOS.DEFAULTS.prayerCurtain = STOS.DEFAULTS.prayerCurtain || {
    enabled: true, darkness: 70, test: '',
    minutes: { fajr: 10, dhuhr: 10, asr: 10, maghrib: 10, isha: 10, jumua: 10 }
  };

  function pad(n) { return (n < 10 ? '0' : '') + n; }
  function dateKey(d) { return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()); }
  function hhmm(d) { return d && isFinite(d) ? pad(d.getHours()) + ':' + pad(d.getMinutes()) : ''; }
  function toMin(t) { var m = /^(\d{1,2}):(\d{2})$/.exec(t || ''); return m ? +m[1] * 60 + +m[2] : null; }
  function fromMin(m) { m = ((m % 1440) + 1440) % 1440; return pad(Math.floor(m / 60)) + ':' + pad(m % 60); }

  function hasLocation(cfg) {
    var c = cfg.calc || {};
    return c.lat !== '' && c.lng !== '' && isFinite(+c.lat) && isFinite(+c.lng) && Math.abs(+c.lat) <= 90 && Math.abs(+c.lng) <= 180;
  }

  // Azan times (and sunrise/sunset) calculated for one day at the configured location.
  function calculate(cfg, date) {
    var a = window.adhan, c = cfg.calc || {};
    if (!a || !hasLocation(cfg)) return null;
    var params = (a.CalculationMethod[c.method] || a.CalculationMethod.NorthAmerica)();
    params.madhab = c.madhab === 'hanafi' ? a.Madhab.Hanafi : a.Madhab.Shafi;
    var coords = new a.Coordinates(+c.lat, +c.lng);
    params.highLatitudeRule = c.highLat && c.highLat !== 'auto' ? c.highLat : a.HighLatitudeRule.recommended(coords);
    var t = new a.PrayerTimes(coords, new Date(date.getFullYear(), date.getMonth(), date.getDate(), 12), params);
    return { fajr: hhmm(t.fajr), sunrise: hhmm(t.sunrise), dhuhr: hhmm(t.dhuhr), asr: hhmm(t.asr),
             maghrib: hhmm(t.maghrib), isha: hhmm(t.isha), sunset: hhmm(t.maghrib) };
  }

  // Times for one date: { times (shaped like cfg.times), source, note }.
  function resolve(cfg, date) {
    date = date || new Date();
    var times = JSON.parse(JSON.stringify(cfg.times || {}));
    var source = cfg.prayerSource || 'manual';
    if (source === 'manual') return { times: times, source: 'manual', note: '' };

    var row = source === 'calendar' ? (cfg.calendar || {})[dateKey(date)] : null;
    var azan = row || calculate(cfg, date), used = row ? 'calendar' : azan ? 'calculated' : 'manual', note = '';
    if (source === 'calendar' && !row) note = 'No calendar entry for ' + dateKey(date) + (azan ? ': using calculated times.' : ': using the fixed times.');
    if (source === 'calculated' && !azan) note = 'Set the mosque location to calculate times: using the fixed times.';
    if (!azan) return { times: times, source: 'manual', note: note };

    var rules = cfg.iqamahRules || DEFAULT_RULES;
    PRAYERS.forEach(function (p) {
      var t = times[p] = times[p] || {};
      if (azan[p]) t.azan = azan[p];
      var given = row && row[p + 'Iqamah'];
      var r = rules[p] || DEFAULT_RULES[p];
      if (given) t.iqamah = given;
      else if (r.mode === 'fixed' && toMin(r.time) != null) t.iqamah = r.time;
      else if (toMin(t.azan) != null) t.iqamah = fromMin(toMin(t.azan) + (+r.minutes || 0));
    });
    if (azan.sunrise) times.sunrise = azan.sunrise;
    if (azan.sunset || azan.maghrib) times.sunset = azan.sunset || azan.maghrib;
    return { times: times, source: used, note: note };
  }

  /* ---------- calendar upload (CSV) ----------
   * One row per date; a header row names the columns (any order, case-insensitive):
   *   date, fajr, sunrise, dhuhr, asr, maghrib, isha   (required)
   *   sunset, fajr_iqamah, dhuhr_iqamah, asr_iqamah, maghrib_iqamah, isha_iqamah   (optional)
   * Dates as YYYY-MM-DD; times as 24-hour 05:30 or 12-hour 5:30 AM. */
  function parseTime(v) {
    var m = /^\s*(\d{1,2})[:.](\d{2})(?::\d{2})?\s*([AaPp])?\.?\s*[Mm]?\.?\s*$/.exec(v || '');
    if (!m) return null;
    var h = +m[1], min = +m[2];
    if (m[3]) { var pm = /p/i.test(m[3]); if (h === 12) h = 0; if (pm) h += 12; }
    return h < 24 && min < 60 ? pad(h) + ':' + pad(min) : null;
  }
  function parseCSV(text) {
    var lines = String(text || '').replace(/^﻿/, '').split(/\r?\n/).filter(function (l) { return l.trim(); });
    if (lines.length < 2) return { rows: {}, count: 0, errors: ['The file needs a header row and at least one date.'] };
    var sep = /\t/.test(lines[0]) ? '\t' : /;/.test(lines[0]) && !/,/.test(lines[0]) ? ';' : ',';
    var head = lines[0].split(sep).map(function (h) { return h.trim().toLowerCase().replace(/[\s-]+/g, '_'); });
    var col = function (name) { var i = head.indexOf(name); return i < 0 ? head.indexOf(name.replace('_', '')) : i; };
    var missing = ['date'].concat(COLUMNS).filter(function (c) { return col(c) < 0; });
    if (missing.length) return { rows: {}, count: 0, errors: ['Missing column(s): ' + missing.join(', ') + '.'] };
    var rows = {}, errors = [], count = 0;
    lines.slice(1).forEach(function (line, i) {
      var cells = line.split(sep).map(function (c) { return c.trim().replace(/^"|"$/g, ''); });
      var date = cells[col('date')];
      var dm = /^(\d{4})-(\d{2})-(\d{2})$/.exec(date || ''), real = dm && new Date(+dm[1], +dm[2] - 1, +dm[3]);
      if (!real || real.getMonth() !== +dm[2] - 1 || real.getDate() !== +dm[3]) { errors.push('Row ' + (i + 2) + ': "' + (date || '') + '" is not a real date (use YYYY-MM-DD).'); return; }
      var r = {}, bad = [];
      COLUMNS.concat(['sunset']).forEach(function (c) {
        if (col(c) < 0 || !cells[col(c)]) { if (c !== 'sunset') bad.push(c); return; }
        var t = parseTime(cells[col(c)]); if (t) r[c] = t; else bad.push(c);
      });
      PRAYERS.forEach(function (p) {
        var j = col(p + '_iqamah'); if (j < 0) j = head.indexOf(p + 'iqamah');
        if (j >= 0 && cells[j]) { var t = parseTime(cells[j]); if (t) r[p + 'Iqamah'] = t; else bad.push(p + ' iqamah'); }
      });
      if (bad.length) { errors.push('Row ' + (i + 2) + ' (' + date + '): unreadable ' + bad.join(', ') + '.'); return; }
      rows[date] = r; count++;
    });
    return { rows: rows, count: count, errors: errors };
  }

  // A calendar template (CSV) for a month, pre-filled with calculated times when a location is set.
  function templateCSV(cfg, year, month) {
    var out = ['date,fajr,sunrise,dhuhr,asr,maghrib,isha,fajr_iqamah,dhuhr_iqamah,asr_iqamah,maghrib_iqamah,isha_iqamah'];
    var days = new Date(year, month + 1, 0).getDate();
    for (var d = 1; d <= days; d++) {
      var date = new Date(year, month, d), t = calculate(cfg, date) || {};
      out.push([dateKey(date), t.fajr, t.sunrise, t.dhuhr, t.asr, t.maghrib, t.isha, '', '', '', '', ''].map(function (v) { return v || ''; }).join(','));
    }
    return out.join('\n') + '\n';
  }

  /* ---------- hook rendering: every mount gets the day's times ---------- */
  function withTimes(cfg) {
    if (!cfg) return cfg;
    var c = JSON.parse(JSON.stringify(cfg));
    c.times = resolve(cfg, new Date()).times;
    return c;
  }
  var views = [];
  var mount = STOS.mount;
  STOS.mount = function (host, opts) {
    var rec = { cfg: opts.config, opts: opts };
    var view = mount(host, Object.assign({}, opts, { config: withTimes(opts.config) }));
    var update = view.update, destroy = view.destroy;
    rec.redraw = function () { update(withTimes(rec.cfg), rec.opts); };
    view.update = function (cfg, o) { rec.cfg = cfg; rec.opts = o; update(withTimes(cfg), o); };
    view.destroy = function () { views = views.filter(function (v) { return v !== rec; }); destroy(); };
    views.push(rec);
    return view;
  };
  // A new day brings new times: redraw every view just after midnight.
  var today = dateKey(new Date());
  setInterval(function () {
    if (dateKey(new Date()) === today) return;
    today = dateKey(new Date());
    views.forEach(function (v) { v.redraw(); });
  }, 20000);

  STOS.prayerTimes = {
    METHODS: METHODS, DEFAULT_RULES: DEFAULT_RULES, PRAYERS: PRAYERS,
    resolve: resolve, calculate: calculate, hasLocation: hasLocation,
    parseCSV: parseCSV, templateCSV: templateCSV, dateKey: dateKey
  };
})();
