/* STOS display engine: config storage, prayer-time logic, shared components
 * and the scaled 1920x1080 stage. Plain script (no modules) so the pages
 * also work when opened straight from disk. */
(function () {
  'use strict';

  var STAGE_W = 1920, STAGE_H = 1080;
  var STORAGE_KEY = 'stos-display-config-v1';
  var PRAYERS = [
    { id: 'fajr', name: 'Fajr' }, { id: 'dhuhr', name: 'Dhuhr' }, { id: 'asr', name: 'Asr' },
    { id: 'maghrib', name: 'Maghrib' }, { id: 'isha', name: 'Isha' }
  ];

  var DEFAULTS = {
    templateId: 'sanctuary',
    themes: {},                       // templateId -> themeId chosen for that template
    mosque: { name: 'Your Mosque Name', city: 'Your city', address: 'Your address', logo: '', showBrand: true },
    times: {
      fajr: { azan: '05:45', iqamah: '06:00' },
      dhuhr: { azan: '13:00', iqamah: '13:30' },
      asr: { azan: '16:15', iqamah: '16:30' },
      maghrib: { azan: '18:20', iqamah: '18:25' },
      isha: { azan: '20:00', iqamah: '20:15' },
      sunrise: '07:15', sunset: '18:20', jumua1: '13:15', jumua2: '14:15'
    },
    options: {
      clock24: false, seconds: true, hijriOffset: 0,
      orientation: 'auto',            // auto | landscape | portrait | portrait-cw | portrait-ccw
      messages: 'Please silence your phone\nWelcome to our mosque'
    },
    // Notices screen. 'match' follows the prayer display's template / theme.
    noticeDesign: { templateId: 'match', themeId: 'match' },
    // Combined screen (prayer panel + rotating notices). 'match' follows the prayer display.
    combinedDesign: { templateId: 'match', themeId: 'match', arrangement: 'prayer-left' },
    notices: [
      { id: 'n1', enabled: true, layout: 'image-right', tag: 'Announcement', title: 'Jumu‘ah Khutbah',
        body: 'The khutbah begins at 1:15 PM. Please arrive early and keep the car park entrance clear.',
        meta: 'Every Friday · Main prayer hall', imageId: 'samples/notices/jumuah.svg', fit: 'cover', duration: 12, from: '', until: '' },
      { id: 'n2', enabled: true, layout: 'image-left', tag: 'Event', title: 'Weekly Halaqah',
        body: 'Join us for tafsir and open discussion after Isha. Everyone is welcome.',
        meta: 'Saturdays after Isha · Community room', imageId: 'samples/notices/halaqah.svg', fit: 'cover', duration: 12, from: '', until: '' },
      { id: 'n3', enabled: true, layout: 'image-left', tag: 'Donate', title: 'Support Your Masjid',
        body: 'Your sadaqah keeps our doors open. Donate at the front desk or online.',
        meta: 'Jazakum Allahu khairan', imageId: 'samples/notices/donate.svg', fit: 'cover', duration: 12, from: '', until: '' }
    ]
  };

  /* ---------- config ---------- */

  function clone(o) { return JSON.parse(JSON.stringify(o)); }
  function merge(base, over) {
    if (!over || typeof over !== 'object') return base;
    Object.keys(over).forEach(function (k) {
      var v = over[k];
      if (v && typeof v === 'object' && !Array.isArray(v) && base[k] && typeof base[k] === 'object') merge(base[k], v);
      else if (v !== undefined) base[k] = v;
    });
    return base;
  }
  function loadConfig() {
    try {
      var raw = localStorage.getItem(STORAGE_KEY);
      if (raw) return merge(clone(DEFAULTS), JSON.parse(raw));
    } catch (e) { /* storage blocked or corrupt: fall back to defaults */ }
    return clone(DEFAULTS);
  }
  function saveConfig(cfg) {
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify(cfg)); return true; }
    catch (e) { return false; }
  }

  function getTemplate(id) {
    var list = window.STOS_TEMPLATES;
    for (var i = 0; i < list.length; i++) if (list[i].id === id) return list[i];
    return list[0];
  }
  function getTheme(id) {
    var list = window.STOS_THEMES;
    for (var i = 0; i < list.length; i++) if (list[i].id === id) return list[i];
    return null;
  }
  // The theme for a template: the user's pick, else the template's original.
  // Template + theme used by the notices screen.
  function noticeDesign(cfg) {
    var d = cfg.noticeDesign || {};
    var tplId = d.templateId && d.templateId !== 'match' ? getTemplate(d.templateId).id : getTemplate(cfg.templateId).id;
    var theme = d.themeId && d.themeId !== 'match' ? getTheme(d.themeId) : null;
    return { templateId: tplId, themeId: (theme || themeFor(cfg, tplId)).id };
  }
  // Template, theme and arrangement used by the combined screen.
  function combinedDesign(cfg) {
    var d = cfg.combinedDesign || {};
    var tplId = d.templateId && d.templateId !== 'match' ? getTemplate(d.templateId).id : getTemplate(cfg.templateId).id;
    var theme = d.themeId && d.themeId !== 'match' ? getTheme(d.themeId) : null;
    return { templateId: tplId, themeId: (theme || themeFor(cfg, tplId)).id, arrangement: d.arrangement === 'prayer-right' ? 'prayer-right' : 'prayer-left' };
  }
  // Template + theme for one notice: its own choice, else the default notice design.
  // A notice with its own template but no theme uses that template's theme.
  function noticeDesignFor(cfg, n) {
    var d = noticeDesign(cfg);
    if (!n || (!n.templateId && !n.themeId)) return d;
    var tplId = n.templateId ? getTemplate(n.templateId).id : d.templateId;
    var theme = n.themeId ? getTheme(n.themeId) : null;
    if (!theme) theme = n.templateId ? themeFor(cfg, tplId) : getTheme(d.themeId);
    return { templateId: tplId, themeId: theme.id, custom: true };
  }
  // Notices that should be on screen today, in order.
  function activeNotices(cfg, now) {
    var d = now || new Date();
    var today = d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
    return (cfg.notices || []).filter(function (n) {
      return n.enabled !== false && (!n.from || n.from <= today) && (!n.until || n.until >= today);
    });
  }

  function themeFor(cfg, templateId) {
    var t = getTemplate(templateId);
    return getTheme(cfg.themes && cfg.themes[t.id]) || getTheme(t.theme) || window.STOS_THEMES[0];
  }

  /* ---------- time ---------- */

  function toMin(hhmm) {
    var m = /^(\d{1,2}):(\d{2})/.exec(hhmm || '');
    return m ? (+m[1]) * 60 + (+m[2]) : null;
  }
  function fmtMin(min, h24) {
    if (min == null) return '--:--';
    var h = Math.floor(min / 60) % 24, m = min % 60, mm = (m < 10 ? '0' : '') + m;
    if (h24) return (h < 10 ? '0' : '') + h + ':' + mm;
    return ((h % 12) || 12) + ':' + mm + ' ' + (h < 12 ? 'AM' : 'PM');
  }
  function pad(n) { return (n < 10 ? '0' : '') + n; }

  var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  var DAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];

  var hijriFmt = null;
  try {
    hijriFmt = new Intl.DateTimeFormat('en-GB-u-ca-islamic-umalqura', { day: 'numeric', month: 'long', year: 'numeric' });
    if (hijriFmt.resolvedOptions().calendar.indexOf('islamic') !== 0) hijriFmt = null;
  } catch (e) { hijriFmt = null; }

  function hijriString(date, offsetDays) {
    if (!hijriFmt) return '';
    var d = new Date(date.getTime() + (offsetDays || 0) * 86400000);
    var parts = hijriFmt.formatToParts(d), day = '', month = '', year = '';
    parts.forEach(function (p) {
      if (p.type === 'day') day = p.value;
      else if (p.type === 'month') month = p.value;
      else if (p.type === 'year' || p.type === 'relatedYear') year = year || p.value;
    });
    return (day + ' ' + month + ' ' + year).trim() + ' AH';
  }

  // Everything that changes over time, computed from config + current time.
  function computeState(cfg, now) {
    var nowMin = now.getHours() * 60 + now.getMinutes() + now.getSeconds() / 60;
    var events = [];
    PRAYERS.forEach(function (p) {
      var t = cfg.times[p.id] || {};
      ['azan', 'iqamah'].forEach(function (kind) {
        var m = toMin(t[kind]);
        if (m != null) events.push({ prayer: p, kind: kind, min: m });
      });
    });
    events.sort(function (a, b) { return a.min - b.min; });
    var next = null, dayOffset = 0;
    for (var i = 0; i < events.length; i++) if (events[i].min > nowMin) { next = events[i]; break; }
    if (!next && events.length) { next = events[0]; dayOffset = 1; }   // after Isha: tomorrow's Fajr

    var h = now.getHours(), o = cfg.options;
    var s = {
      hm: o.clock24 ? pad(h) + ':' + pad(now.getMinutes()) : ((h % 12) || 12) + ':' + pad(now.getMinutes()),
      sec: o.seconds ? ':' + pad(now.getSeconds()) : '',
      ap: o.clock24 ? '' : (h < 12 ? 'AM' : 'PM'),
      dateLong: DAYS[now.getDay()] + ', ' + now.getDate() + ' ' + MONTHS[now.getMonth()] + ' ' + now.getFullYear(),
      dateShort: now.getDate() + ' ' + MONTHS[now.getMonth()] + ' ' + now.getFullYear(),
      hijri: hijriString(now, +o.hijriOffset || 0),
      active: next ? next.prayer.id : '',
      nextName: '', nextKind: '', nextAt: '', countdown: '--:--:--'
    };
    if (next) {
      var secsLeft = Math.max(0, Math.round((next.min + dayOffset * 1440 - nowMin) * 60));
      s.nextName = next.prayer.name.toUpperCase() + ' ' + next.kind.toUpperCase();
      s.nextKind = next.kind === 'azan' ? 'Azan' : 'Iqamah';
      s.nextAt = s.nextKind + ' at ' + fmtMin(next.min, o.clock24);
      s.nextTime = fmtMin(next.min, o.clock24);
      s.countdown = pad(Math.floor(secsLeft / 3600)) + ':' + pad(Math.floor(secsLeft / 60) % 60) + ':' + pad(secsLeft % 60);
    }
    return s;
  }

  /* ---------- markup helpers ---------- */

  function esc(v) {
    return String(v == null ? '' : v).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  var ICONS = {
    mosque: '<svg viewBox="0 0 64 64" fill="currentColor"><path d="M32 3l1.6 5.2a2 2 0 1 1-3.2 0z"/><path d="M32 11c-3 4-12 7.5-15.5 14C14 30 14.5 34 15 36h34c.5-2 1-6-1.5-11C44 18.5 35 15 32 11z"/><path fill-rule="evenodd" d="M13 38h38v22H13zm15 22V50a4 4 0 0 1 8 0v10z"/><path d="M4 26h6v34H4zM54 26h6v34h-6zM7 15l3.5 9h-7zM57 15l3.5 9h-7z"/></svg>',
    sunrise: '<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round"><path d="M16 42a16 16 0 0 1 32 0"/><path d="M32 14v7M12 23l5 5M52 23l-5 5M4 42h6M54 42h6M4 50h56M16 58h32"/><path d="M26 10l6-6 6 6"/></svg>',
    sunset: '<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round"><path d="M16 42a16 16 0 0 1 32 0z" fill="currentColor" fill-opacity=".25"/><path d="M32 4v12M12 23l5 5M52 23l-5 5M4 42h6M54 42h6M4 50h56M16 58h32"/><path d="M26 11l6 6 6-6"/></svg>',
    people: '<svg viewBox="0 0 64 64" fill="currentColor"><circle cx="32" cy="17" r="8"/><circle cx="14" cy="22" r="6"/><circle cx="50" cy="22" r="6"/><path d="M18 52c0-9 6-16 14-16s14 7 14 16zM2 52c0-8 5-13 12-13 3 0 5 1 7 2-3 3-5 7-5 11zM62 52c0-8-5-13-12-13-3 0-5 1-7 2 3 3 5 7 5 11z"/></svg>',
    phoneOff: '<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"><rect x="19" y="6" width="26" height="52" rx="5"/><path d="M29 50h6M8 8l48 48"/></svg>',
    calendar: '<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="4" stroke-linejoin="round"><rect x="8" y="12" width="48" height="44" rx="5"/><path d="M8 24h48M20 6v12M44 6v12" stroke-linecap="round"/><path d="M18 33h6M29 33h6M40 33h6M18 44h6M29 44h6M40 44h6" stroke-width="5"/></svg>',
    moon: '<svg viewBox="0 0 64 64" fill="currentColor"><path d="M40 6a26 26 0 1 0 18 40A22 22 0 0 1 40 6z"/></svg>',
    bell: '<svg viewBox="0 0 64 64" fill="currentColor"><path d="M32 4a4 4 0 0 1 4 4v2c9 2 15 10 15 20v12l5 6v3H8v-3l5-6V30c0-10 6-18 15-20V8a4 4 0 0 1 4-4zM25 55h14a7 7 0 0 1-14 0z"/></svg>'
  };

  // Geometric eight-point-star tile used (as a mask) for the ornamental backgrounds.
  var PATTERN_SVG = "<svg xmlns='http://www.w3.org/2000/svg' width='120' height='120' viewBox='0 0 120 120' fill='none' stroke='black' stroke-width='2'>" +
    "<rect x='31' y='31' width='58' height='58'/><rect x='31' y='31' width='58' height='58' transform='rotate(45 60 60)'/>" +
    "<circle cx='60' cy='60' r='13'/><path d='M0 0L31 31M120 0L89 31M0 120L31 89M120 120L89 89M60 0V19M60 120V101M0 60H19M120 60H101'/></svg>";
  var PATTERN_URL = 'url("data:image/svg+xml,' + encodeURIComponent(PATTERN_SVG) + '")';

  var ARCHES = {
    ogee: 'M3 1000V330Q3 250 90 250H330C400 250 440 180 470 90Q490 30 500 6Q510 30 530 90C560 180 600 250 670 250H910Q997 250 997 330V1000Z',
    pointed: 'M3 1000V400C3 250 250 215 380 140C450 95 490 50 500 6C510 50 550 95 620 140C750 215 997 250 997 400V1000Z',
    dome: 'M3 1000V300Q3 210 100 200Q300 180 410 80Q470 26 500 6Q530 26 590 80Q700 180 900 200Q997 210 997 300V1000Z'
  };

  var ARCH_MASKS = {};
  Object.keys(ARCHES).forEach(function (k) {
    var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" preserveAspectRatio="none"><path d="' + ARCHES[k] + '"/></svg>';
    ARCH_MASKS[k] = "url('data:image/svg+xml," + encodeURIComponent(svg) + "')";
  });

  // Mosque skyline silhouette (domes + minarets) generated once.
  var SKYLINE = (function () {
    var d = '', base = 300;
    var domes = [[140, 70], [420, 110], [760, 90], [1160, 120], [1520, 80], [1800, 100]];
    var minarets = [[60, 210], [300, 250], [600, 200], [960, 270], [1340, 230], [1680, 210], [1880, 190]];
    domes.forEach(function (o) {
      var x = o[0], r = o[1], top = base - r * 1.9;
      d += 'M' + (x - r) + ' ' + base + 'V' + (base - r * .7) + 'C' + (x - r) + ' ' + (top + r * .5) + ' ' + (x - 6) + ' ' + (top + r * .25) + ' ' + x + ' ' + top +
        'C' + (x + 6) + ' ' + (top + r * .25) + ' ' + (x + r) + ' ' + (top + r * .5) + ' ' + (x + r) + ' ' + (base - r * .7) + 'V' + base + 'Z';
      d += 'M' + (x - 2) + ' ' + (top - 26) + 'h4v26h-4z';
    });
    minarets.forEach(function (o) {
      var x = o[0], hgt = o[1];
      d += 'M' + (x - 10) + ' ' + base + 'V' + (base - hgt) + 'h20V' + base + 'Z';
      d += 'M' + (x - 15) + ' ' + (base - hgt * .7) + 'h30v8h-30z';
      d += 'M' + (x - 10) + ' ' + (base - hgt) + 'L' + x + ' ' + (base - hgt - 40) + 'L' + (x + 10) + ' ' + (base - hgt) + 'Z';
    });
    d += 'M0 ' + base + 'V' + (base - 30) + 'H1920V' + base + 'Z';
    return '<svg viewBox="0 0 1920 300" preserveAspectRatio="none"><path d="' + d + '"/></svg>';
  })();

  var LANTERN = '<svg viewBox="0 0 60 200" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M30 0v70"/><path d="M22 78h16l-8-8zM16 84h28l-4 10H20z" fill="currentColor"/>' +
    '<path d="M18 94h24l6 40-6 30H18l-6-30z" fill="currentColor" fill-opacity=".18"/><path d="M30 94v70M18 94l12 70M42 94l-12 70"/><ellipse cx="30" cy="132" rx="9" ry="16" fill="currentColor" fill-opacity=".9"/>' +
    '<path d="M16 164h28l-4 10H20zM26 174h8l-4 14z" fill="currentColor"/></svg>';

  /* ---------- component builders (bound to a config) ---------- */

  function builders(cfg) {
    var m = cfg.mosque, t = cfg.times, o = cfg.options, h24 = o.clock24;
    var c = {};

    c.cfg = cfg;
    c.icon = function (name) { return ICONS[name] || ''; };
    c.mediaUrl = function (id) { return id ? media.cached(id) : ''; };
    c.time = function (hhmm) { return esc(fmtMin(toMin(hhmm), h24)); };

    c.logo = function (cls) {
      var inner = m.logo ? '<img src="' + esc(m.logo) + '" alt="">' : ICONS.mosque;
      return '<div class="logo ' + (cls || '') + '">' + inner + '</div>';
    };
    // Empty slot that keeps header layouts balanced; the STOS credit itself lives in the footer.
    c.brand = function (cls) { return '<div class="brand ' + (cls || '') + '"></div>'; };
    c.name = function (cls) { return '<h1 class="mname ' + (cls || '') + '" data-fit>' + esc(m.name) + '</h1>'; };
    c.sub = function (cls) {
      var bits = [m.city, m.address].filter(Boolean).map(esc);
      return bits.length ? '<p class="msub ' + (cls || '') + '">' + bits.join('<span class="dot">•</span>') + '</p>' : '';
    };
    c.greg = function (style) { return '<span data-k="' + (style === 'short' ? 'dateShort' : 'dateLong') + '"></span>'; };
    c.hijri = function () { return '<span data-k="hijri"></span>'; };
    c.datesLine = function (cls, style) {
      return '<p class="dline ' + (cls || '') + '">' + c.greg(style) + '<span class="bar">|</span>Hijri: ' + c.hijri() + '</p>';
    };

    // o: { sub, dates: 'short'|'long', cls }
    c.header = function (opt) {
      opt = opt || {};
      return '<header class="hd ' + (opt.cls || '') + '">' + c.logo() +
        '<div class="hd-title">' + c.name() + (opt.sub ? c.sub() : '') + (opt.dates ? c.datesLine('', opt.dates) : '') + '</div>' +
        c.brand() + '</header>';
    };
    c.chip = function (kind, cls) {
      return '<div class="chip ' + (cls || '') + '">' + (kind === 'hijri' ? ICONS.moon + '<span>Hijri: ' + c.hijri() + '</span>' : ICONS.calendar + c.greg('long')) + '</div>';
    };
    c.datebar = function (cls) {
      return '<div class="datebar ' + (cls || '') + '"><div class="chip">' + ICONS.calendar + c.greg('long') + '</div><i class="sep"></i>' +
        '<div class="chip">' + ICONS.moon + '<span>Hijri: ' + c.hijri() + '</span></div></div>';
    };

    // v: inline | split | stack
    c.clock = function (v, cls) {
      return '<div class="clock clock-' + (v || 'inline') + ' ' + (cls || '') + '" data-fit><span class="c-main"><span data-k="hm"></span><span class="c-s" data-k="sec"></span></span><span class="c-ap" data-k="ap"></span></div>';
    };

    // v: box | pill | flat | bell | big | banner
    c.countdown = function (v, cls) {
      var label = '<div class="cd-label">NEXT: <b data-k="nextName"></b></div>';
      var time = '<div class="cd-time" data-k="countdown"></div>';
      var at = '<div class="cd-at" data-k="nextAt"></div>';
      var body;
      if (v === 'bell') body = '<i class="cd-icon">' + ICONS.bell + '</i><div class="cd-stack">' + label + time + '</div>';
      else if (v === 'big') body = '<div class="cd-label">NEXT:</div><div class="cd-name" data-k="nextName"></div>' + time +
        '<div class="cd-at"><span data-k="nextKind"></span> at</div><div class="cd-bigt" data-k="nextTime"></div>';
      else if (v === 'banner') body = '<div class="cd-label">NEXT: <b data-k="nextName"></b></div><i class="cd-div"></i>' + time;
      else if (v === 'pill' || v === 'flat') body = label + time + at;
      else body = label + time + at;
      return '<div class="cd cd-' + (v || 'box') + ' ' + (cls || '') + '">' + body + '</div>';
    };

    c.table = function (cls) {
      var rows = PRAYERS.map(function (p) {
        var x = t[p.id] || {};
        return '<div class="pt-row" data-p="' + p.id + '"><span>' + p.name.toUpperCase() + '</span><span>' + c.time(x.azan) + '</span><span>' + c.time(x.iqamah) + '</span></div>';
      }).join('');
      return '<div class="pt ' + (cls || '') + '"><div class="pt-row pt-head"><span>PRAYER</span><span>AZAN</span><span>IQAMAH</span></div>' + rows + '</div>';
    };

    // Horizontal timetable: prayers as columns.
    c.hrow = function (cls) {
      var cols = PRAYERS.map(function (p) {
        var x = t[p.id] || {};
        return '<div class="ph-col" data-p="' + p.id + '"><span class="ph-n">' + p.name.toUpperCase() + '</span><span>' + c.time(x.azan) + '</span><span>' + c.time(x.iqamah) + '</span></div>';
      }).join('');
      return '<div class="ph ' + (cls || '') + '"><div class="ph-col ph-lab"><span></span><span>AZAN</span><span>IQAMAH</span></div>' + cols + '</div>';
    };

    // v: stack | split ; opt.sunrise adds a sunrise card after Fajr
    c.cards = function (v, cls, opt) {
      opt = opt || {};
      var list = [];
      PRAYERS.forEach(function (p) {
        var x = t[p.id] || {};
        list.push('<div class="pc-card" data-p="' + p.id + '"><div class="pc-name">' + p.name.toUpperCase() + '</div><div class="pc-times">' +
          '<div><small>AZAN</small><b>' + c.time(x.azan) + '</b></div><div><small>IQAMAH</small><b>' + c.time(x.iqamah) + '</b></div></div></div>');
        if (opt.sunrise && p.id === 'fajr') list.push('<div class="pc-card pc-sun"><div class="pc-name">SUNRISE</div><div class="pc-times"><div><b>' + c.time(t.sunrise) + '</b></div></div></div>');
      });
      return '<div class="pc pc-' + (v || 'stack') + ' ' + (cls || '') + '" style="--n:' + list.length + '">' + list.join('') + '</div>';
    };

    // v: tiles | bar | list ; opt.jicon: people | mosque
    c.info = function (v, cls, opt) {
      opt = opt || {};
      var j = opt.jicon || 'people';
      var items = [['sunrise', 'SUNRISE', t.sunrise], ['sunset', 'SUNSET', t.sunset], [j, 'JUMUA 1', t.jumua1], [j, 'JUMUA 2', t.jumua2]];
      return '<div class="it it-' + (v || 'tiles') + ' ' + (cls || '') + '">' + items.map(function (i) {
        return '<div class="it-tile"><i>' + ICONS[i[0]] + '</i><div><small>' + i[1] + '</small><b>' + c.time(i[2]) + '</b></div></div>';
      }).join('') + '</div>';
    };
    c.sunbar = function (cls) {
      return '<div class="sunbar ' + (cls || '') + '"><i>' + ICONS.sunrise + '</i><small>SUNRISE</small><b>' + c.time(t.sunrise) + '</b></div>';
    };

    c.footer = function (cls) {
      var msgs = String(o.messages || '').split('\n').map(function (s) { return s.trim(); }).filter(Boolean).map(esc);
      var credit = m.showBrand ? '<span class="ft-credit"><img class="ft-mark" src="img/stos-mark.png?v=2" alt="">Powered by <b><span class="b1">S</span><span class="b2">T</span><span class="b3">O</span><span class="b4">S</span></b></span>' : '';
      return '<footer class="ft ' + (m.showBrand ? 'has-credit ' : '') + (cls || '') + '"><i>' + ICONS.phoneOff + '</i><span class="ft-msg" data-fit>' + msgs.join('<span class="dot">•</span>') + '</span>' + credit + '</footer>';
    };

    /* ornaments */
    c.pattern = function (where) {
      if (where === 'corners') return '<div class="orn pat pat-c tl"></div><div class="orn pat pat-c tr"></div><div class="orn pat pat-c bl"></div><div class="orn pat pat-c br"></div>';
      if (where === 'sides') return '<div class="orn pat pat-s l"></div><div class="orn pat pat-s r"></div>';
      return '<div class="orn pat pat-full"></div>';
    };
    // Arch-shaped panel: the fill is a masked layer (so gradient surfaces work), the outline an SVG stroke.
    c.arch = function (shape, inner, cls) {
      var d = ARCHES[shape || 'ogee'];
      return '<div class="arch arch-' + (shape || 'ogee') + ' ' + (cls || '') + '"><div class="arch-bg" style="-webkit-mask-image:' + ARCH_MASKS[shape || 'ogee'] + ';mask-image:' + ARCH_MASKS[shape || 'ogee'] + '"></div>' +
        '<svg class="arch-svg" viewBox="0 0 1000 1000" preserveAspectRatio="none"><path d="' + d + '" vector-effect="non-scaling-stroke"/></svg><div class="arch-in">' + inner + '</div></div>';
    };
    c.lantern = function (cls) { return '<div class="orn lantern ' + (cls || '') + '">' + LANTERN + '</div>'; };
    c.skyline = function (cls) { return '<div class="orn skyline ' + (cls || '') + '">' + SKYLINE + '</div>'; };
    return c;
  }

  /* ---------- theme -> css vars ---------- */

  function hexToRgba(hex, a) {
    var m = /^#?([0-9a-f]{6})$/i.exec(hex || '');
    if (!m) return hex;
    var n = parseInt(m[1], 16);
    return 'rgba(' + (n >> 16) + ',' + ((n >> 8) & 255) + ',' + (n & 255) + ',' + a + ')';
  }
  function applyTheme(el, theme) {
    var v = theme.vars;
    Object.keys(v).forEach(function (k) { el.style.setProperty('--' + k, v[k]); });
    el.style.setProperty('--lineSoft', hexToRgba(v.line, .35));
    el.style.setProperty('--panelInk', v.panelInk || v.ink);
    el.style.setProperty('--pat-url', PATTERN_URL);
  }

  /* ---------- mounting ---------- */

  var live = [];
  var ticker = null;

  // [data-fit] shrinks until it fits its width; [data-fitbox] until it fits width and height.
  function fitText(stage) {
    function shrink(el, tooBig) {
      el.style.fontSize = '';
      var size = parseFloat(getComputedStyle(el).fontSize), guard = 0;
      while (tooBig(el) && size > 12 && guard++ < 80) { size *= 0.95; el.style.fontSize = size + 'px'; }
    }
    var i, els = stage.querySelectorAll('[data-fit]');
    for (i = 0; i < els.length; i++) shrink(els[i], function (el) { return el.scrollWidth > el.clientWidth + 1; });
    els = stage.querySelectorAll('[data-fitbox]');
    for (i = 0; i < els.length; i++) shrink(els[i], function (el) { return el.scrollHeight > el.clientHeight + 1 || el.scrollWidth > el.clientWidth + 1; });
  }

  /* ---------- media (notice images) ----------
   * Images live in IndexedDB (localStorage is too small for photos); the
   * config only stores their ids. Object URLs are cached for rendering. */
  // Images bundled with the app (default notice pictures) are referenced by path, e.g. samples/notices/halaqah.svg.
  function isSample(id) { return typeof id === 'string' && id.indexOf('samples/') === 0; }
  var media = (function () {
    var DB = 'stos-media', STORE = 'images', dbp = null, urls = {};
    function db() {
      if (!dbp) dbp = new Promise(function (res, rej) {
        var r = indexedDB.open(DB, 1);
        r.onupgradeneeded = function () { r.result.createObjectStore(STORE); };
        r.onsuccess = function () { res(r.result); };
        r.onerror = function () { rej(r.error); };
      });
      return dbp;
    }
    function tx(mode, fn) {
      return db().then(function (d) {
        return new Promise(function (res, rej) {
          var t = d.transaction(STORE, mode), req = fn(t.objectStore(STORE));
          t.oncomplete = function () { res(req && req.result); };
          t.onerror = function () { rej(t.error); };
        });
      });
    }
    return {
      put: function (blob, id) {
        id = id || 'img-' + Date.now().toString(36) + Math.random().toString(36).slice(2, 7);
        return tx('readwrite', function (s) { return s.put(blob, id); }).then(function () { return id; });
      },
      get: function (id) { return tx('readonly', function (s) { return s.get(id); }); },
      del: function (id) {
        if (urls[id]) { URL.revokeObjectURL(urls[id]); delete urls[id]; }
        return tx('readwrite', function (s) { return s.delete(id); });
      },
      clear: function () { return tx('readwrite', function (s) { return s.clear(); }); },
      keys: function () { return tx('readonly', function (s) { return s.getAllKeys(); }); },
      cached: function (id) { return isSample(id) ? id : urls[id] || ''; },
      // Load the given ids into the URL cache (missing ones are skipped).
      preload: function (ids) {
        return Promise.all((ids || []).filter(function (id) { return id && !urls[id] && !isSample(id); }).map(function (id) {
          return tx('readonly', function (s) { return s.get(id); }).then(function (blob) {
            if (blob) urls[id] = URL.createObjectURL(blob);
          });
        })).catch(function () { /* storage unavailable: render without images */ });
      }
    };
  })();

  function updateStage(inst, now) {
    var s = computeState(inst.cfg, now || new Date());
    var nodes = inst.stage.querySelectorAll('[data-k]');
    for (var i = 0; i < nodes.length; i++) {
      var v = s[nodes[i].getAttribute('data-k')];
      if (v !== undefined && nodes[i].textContent !== v) nodes[i].textContent = v;
    }
    if (inst.active !== s.active) {
      inst.active = s.active;
      var ps = inst.stage.querySelectorAll('[data-p]');
      for (var j = 0; j < ps.length; j++) ps[j].classList.toggle('is-active', ps[j].getAttribute('data-p') === s.active);
    }
    if (inst.dateKey !== s.dateLong) {        // text widths changed: refit headings once
      inst.dateKey = s.dateLong;
      fitText(inst.stage);
    }
  }

  /* Orientation: landscape is 1920x1080, portrait 1080x1920 (9:16).
   * 'auto' follows the host's shape; portrait-cw / portrait-ccw draw the portrait
   * layout rotated, for screens mounted on their side that still output landscape. */
  var ORIENTS = { landscape: [0, 'landscape'], portrait: [0, 'portrait'], 'portrait-cw': [90, 'portrait'], 'portrait-ccw': [-90, 'portrait'] };
  function resolveOrient(inst) {
    var mode = inst.orientOverride || (inst.cfg.options && inst.cfg.options.orientation) || 'auto';
    if (!ORIENTS[mode]) {
      var box = inst.host.getBoundingClientRect();
      mode = box.height > box.width ? 'portrait' : 'landscape';
    }
    return { layout: ORIENTS[mode][1], rotate: ORIENTS[mode][0] };
  }

  function scaleStage(inst) {
    var box = inst.host.getBoundingClientRect();
    if (!box.width || !box.height) return;
    var o = resolveOrient(inst);
    if (o.layout !== inst.layout) {             // auto mode crossed between shapes
      inst.stage.classList.remove('landscape', 'portrait');
      inst.stage.classList.add(o.layout);
      inst.layout = o.layout;
      fitText(inst.stage);
    }
    var w = o.layout === 'portrait' ? STAGE_H : STAGE_W, h = o.layout === 'portrait' ? STAGE_W : STAGE_H;
    var fw = o.rotate ? h : w, fh = o.rotate ? w : h;   // footprint on screen
    var k = Math.min(box.width / fw, box.height / fh);
    var x = (box.width - fw * k) / 2, y = (box.height - fh * k) / 2;
    if (o.rotate === 90) x += fw * k;
    if (o.rotate === -90) y += fh * k;
    inst.stage.style.transform = 'translate(' + x + 'px,' + y + 'px)' + (o.rotate ? ' rotate(' + o.rotate + 'deg)' : '') + ' scale(' + k + ')';
  }

  /* mount(host, { config, templateId?, themeId?, orientation?, live?, notice?, slide? })
   * Passing `notice` (a notice object, or null for the empty state) renders the notice design;
   * add `combined: true` for the combined prayer + notices design.
   * Renders a template into host, scaled to fit. Returns { update(cfg, opts), destroy() }. */
  function mount(host, opts) {
    host.classList.add('stos-host');
    var inst = { host: host, stage: document.createElement('div'), cfg: null, live: opts.live !== false };
    host.innerHTML = '';
    host.appendChild(inst.stage);

    function draw(cfg, o) {
      o = o || {};
      inst.cfg = cfg;
      var tpl = getTemplate(o.templateId || cfg.templateId);
      var theme = getTheme(o.themeId) || themeFor(cfg, tpl.id);
      inst.templateId = tpl.id; inst.themeId = theme.id;
      inst.orientOverride = o.orientation;
      inst.layout = resolveOrient(inst).layout;
      inst.stage.className = 'stage tpl-' + tpl.id + ' font-' + (tpl.font || 'sans') + ' ' + inst.layout;
      inst.stage.removeAttribute('style');
      applyTheme(inst.stage, theme);
      host.style.background = theme.vars.bg;    // fills letterbox bars on non-16:9 screens
      // Notice mode renders the template's notice design instead of its prayer layout.
      if (o.notice !== undefined) {
        inst.stage.classList.add('mode-notice');
        if (o.combined) inst.stage.classList.add('mode-combined');
        inst.stage.innerHTML = (o.combined ? window.STOS_renderCombined : window.STOS_renderNotice)(tpl, builders(cfg), o.notice, o.slide || {});
      } else {
        inst.stage.innerHTML = tpl.render(builders(cfg));
      }
      inst.active = null; inst.dateKey = null;
      scaleStage(inst);
      updateStage(inst, o.now);
    }
    draw(opts.config, opts);

    if ('ResizeObserver' in window) {
      inst.ro = new ResizeObserver(function () { scaleStage(inst); });
      inst.ro.observe(host);
    } else {
      window.addEventListener('resize', function () { scaleStage(inst); });
    }
    if (inst.live) {
      live.push(inst);
      if (!ticker) ticker = setInterval(function () {
        var now = new Date();
        live.forEach(function (i) { updateStage(i, now); });
      }, 500);
    }
    // Web fonts change text widths; refit once they arrive.
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(function () { fitText(inst.stage); });

    return {
      update: function (cfg, o) { draw(cfg, o); },
      destroy: function () {
        if (inst.ro) inst.ro.disconnect();
        live = live.filter(function (i) { return i !== inst; });
        host.innerHTML = '';
      },
      get templateId() { return inst.templateId; },
      get themeId() { return inst.themeId; }
    };
  }

  window.STOS = {
    STORAGE_KEY: STORAGE_KEY, DEFAULTS: DEFAULTS, PRAYERS: PRAYERS,
    loadConfig: loadConfig, saveConfig: saveConfig, clone: clone, merge: merge,
    getTemplate: getTemplate, getTheme: getTheme, themeFor: themeFor,
    computeState: computeState, mount: mount, esc: esc,
    noticeDesign: noticeDesign, noticeDesignFor: noticeDesignFor, combinedDesign: combinedDesign, activeNotices: activeNotices, media: media
  };
})();
