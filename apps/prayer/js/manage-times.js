/* Settings page, "Prayer times" tab: where the times come from (fixed, calculated or an uploaded
 * calendar), the location and method, iqamah rules, and today's resulting times.
 * Load after manage.js. Saves go through STOS.saveConfig; a storage event then lets manage.js
 * pick up the change so both stay in step. */
(function () {
  var PT = STOS.prayerTimes;
  var panel = document.querySelector('[data-panel="times"]');
  var timesForm = document.getElementById('times-form');
  if (!PT || !panel || !timesForm) return;
  var esc = STOS.esc;
  var NAMES = { fajr: 'Fajr', dhuhr: 'Dhuhr', asr: 'Asr', maghrib: 'Maghrib', isha: 'Isha' };

  var box = document.createElement('div');
  box.className = 'form pt-box';
  box.innerHTML =
    '<div class="field wide"><span>Where the times come from</span>' +
    '  <div class="seg" role="group" aria-label="Prayer time source">' +
    '    <button type="button" data-src="manual">Fixed times</button>' +
    '    <button type="button" data-src="calculated">Calculated</button>' +
    '    <button type="button" data-src="calendar">Uploaded calendar</button></div>' +
    '  <small id="pt-src-help"></small></div>' +

    '<div class="pt-sec wide" data-for="calculated calendar">' +
    '  <div class="field-row pt-loc">' +
    '    <label class="field"><span>Latitude</span><input name="lat" type="number" step="0.0001" min="-90" max="90" placeholder="52.1332"></label>' +
    '    <label class="field"><span>Longitude</span><input name="lng" type="number" step="0.0001" min="-180" max="180" placeholder="-106.6700"></label>' +
    '    <label class="field"><span>Method</span><select name="method">' +
    PT.METHODS.map(function (m) { return '<option value="' + m[0] + '">' + esc(m[1]) + '</option>'; }).join('') + '</select></label>' +
    '    <label class="field"><span>Asr</span><select name="madhab"><option value="shafi">Standard (Shafi, Maliki, Hanbali)</option><option value="hanafi">Hanafi</option></select></label>' +
    '  </div>' +
    '  <label class="field"><span>High latitude rule</span><select name="highLat"><option value="auto">Automatic</option>' +
    '    <option value="middleofthenight">Middle of the night</option><option value="seventhofthenight">Seventh of the night</option>' +
    '    <option value="twilightangle">Twilight angle</option></select>' +
    '    <small>Used for Fajr and Isha far from the equator. Find latitude and longitude by searching the mosque in a map app.</small></label>' +
    '</div>' +

    '<div class="pt-sec wide" data-for="calendar">' +
    '  <div class="field"><span>Calendar</span><p class="hint" id="pt-cal-status"></p>' +
    '    <div class="logo-row"><label class="btn"><input type="file" id="pt-cal-file" accept=".csv,text/csv,text/plain" hidden>Upload calendar (CSV)</label>' +
    '      <button type="button" class="btn ghost" id="pt-cal-template">Download a template</button>' +
    '      <button type="button" class="btn ghost" id="pt-cal-clear">Clear calendar</button></div>' +
    '    <small>Columns: date (YYYY-MM-DD), fajr, sunrise, dhuhr, asr, maghrib, isha. Optional: sunset and fajr_iqamah … isha_iqamah. ' +
    'Times like 05:30 or 5:30 AM. Uploading adds or replaces those dates. Missing dates use the calculated times.</small>' +
    '    <div id="pt-cal-errors" class="hint"></div></div>' +
    '</div>' +

    '<div class="pt-sec wide" data-for="calculated calendar">' +
    '  <div class="field"><span>Iqamah</span><small>Used when the calendar has no iqamah time for that prayer.</small></div>' +
    '  <table class="times"><thead><tr><th>Prayer</th><th>Iqamah</th><th></th></tr></thead><tbody id="pt-rules"></tbody></table>' +
    '</div>' +

    '<div class="pt-sec wide" data-for="manual calculated calendar">' +
    '  <div class="field"><span>Dim the screen during prayer</span>' +
    '    <small>From each iqamah, a dark curtain covers the screen for the minutes below; the template stays underneath. ' +
    'On Fridays the Jumua times are used instead of Dhuhr. 0 minutes turns it off for that prayer.</small></div>' +
    '  <label class="check"><input type="checkbox" id="pc-enabled"> Dim the screen during the congregation</label>' +
    '  <label class="field"><span>Darkness <em id="pc-dark-val"></em></span><input type="range" id="pc-dark" min="20" max="95" step="5"></label>' +
    '  <div class="field-row pc-mins">' +
    ['fajr', 'dhuhr', 'asr', 'maghrib', 'isha', 'jumua'].map(function (p) {
      return '<label class="field"><span>' + (p === 'jumua' ? 'Jumua' : NAMES[p]) + ' (min)</span><input type="number" min="0" max="120" data-pc="' + p + '"></label>';
    }).join('') +
    '  </div>' +
    '  <div><button type="button" class="btn" id="pc-test">Dim the screen now for 15 seconds</button> <span class="hint" id="pc-test-msg"></span></div>' +
    '</div>' +

    '<div class="pt-sec wide" data-for="calculated calendar">' +
    '  <div class="field"><span>Today on the screen <em id="pt-today-src"></em></span></div>' +
    '  <table class="times"><thead><tr><th>Prayer</th><th>Azan</th><th>Iqamah</th></tr></thead><tbody id="pt-today"></tbody></table>' +
    '  <p class="hint" id="pt-today-extra"></p>' +
    '</div>';
  timesForm.parentNode.insertBefore(box, timesForm);

  var style = document.createElement('style');
  style.textContent = '.pt-box{margin-bottom:22px}.pt-sec{display:grid;gap:12px}.pt-box .wide{grid-column:1/-1}' +
    '.pc-mins{grid-template-columns:repeat(6,1fr)}@media(max-width:700px){.pc-mins{grid-template-columns:repeat(3,1fr)}}.pt-loc{padding-top:0}.pt-err{color:#c0392b}.pt-ok{color:#1e8449}#pt-rules input{width:100%}';
  document.head.appendChild(style);

  function cfg() { return STOS.loadConfig(); }
  function save(c) {
    STOS.saveConfig(c);
    window.dispatchEvent(new StorageEvent('storage', { key: STOS.STORAGE_KEY }));   // manage.js reloads its copy
    render();
  }

  function render() {
    var c = cfg(), src = c.prayerSource || 'manual', calc = c.calc || {}, rules = c.iqamahRules || PT.DEFAULT_RULES;
    box.querySelectorAll('[data-src]').forEach(function (b) { b.setAttribute('aria-pressed', String(b.dataset.src === src)); });
    $id('pt-src-help').textContent = {
      manual: 'The same times every day, typed below.',
      calculated: 'Calculated every day from the mosque location. Jumua times are set below.',
      calendar: 'Taken from your uploaded timetable, one row per date. Jumua times are set below.'
    }[src];
    box.querySelectorAll('.pt-sec').forEach(function (s) { s.hidden = s.dataset.for.split(' ').indexOf(src) < 0; });

    // Fixed times are only edited in "Fixed times"; Jumua stays editable in every mode.
    var fixed = src === 'manual';
    timesForm.querySelector('table.times').hidden = !fixed;
    ['sunrise', 'sunset'].forEach(function (k) { timesForm[k].closest('label').hidden = !fixed; });

    ['lat', 'lng', 'method', 'madhab', 'highLat'].forEach(function (k) {
      var el = box.querySelector('[name="' + k + '"]');
      if (document.activeElement !== el) el.value = calc[k] != null ? calc[k] : '';
    });

    $id('pt-rules').innerHTML = PT.PRAYERS.map(function (p) {
      var r = rules[p] || PT.DEFAULT_RULES[p];
      return '<tr><td>' + NAMES[p] + '</td><td><select data-rule="' + p + '" data-k="mode">' +
        '<option value="offset"' + (r.mode !== 'fixed' ? ' selected' : '') + '>Minutes after azan</option>' +
        '<option value="fixed"' + (r.mode === 'fixed' ? ' selected' : '') + '>Fixed time</option></select></td><td>' +
        (r.mode === 'fixed'
          ? '<input type="time" data-rule="' + p + '" data-k="time" value="' + esc(r.time || '') + '" aria-label="' + NAMES[p] + ' iqamah time">'
          : '<input type="number" min="0" max="180" data-rule="' + p + '" data-k="minutes" value="' + esc(r.minutes != null ? r.minutes : '') + '" aria-label="' + NAMES[p] + ' minutes after azan">') +
        '</td></tr>';
    }).join('');

    var pc = c.prayerCurtain || STOS.DEFAULTS.prayerCurtain, pm = pc.minutes || {};
    $id('pc-enabled').checked = pc.enabled !== false;
    if (document.activeElement !== $id('pc-dark')) $id('pc-dark').value = pc.darkness || 70;
    $id('pc-dark-val').textContent = '(' + (pc.darkness || 70) + '%)';
    box.querySelectorAll('[data-pc]').forEach(function (el) {
      if (document.activeElement !== el) el.value = pm[el.dataset.pc] != null ? pm[el.dataset.pc] : 10;
      el.disabled = pc.enabled === false;
    });
    $id('pc-dark').disabled = pc.enabled === false;

    var cal = c.calendar || {}, dates = Object.keys(cal).sort(), today = PT.dateKey(new Date());
    $id('pt-cal-status').innerHTML = dates.length
      ? dates.length + ' day' + (dates.length === 1 ? '' : 's') + ' loaded, ' + dates[0] + ' to ' + dates[dates.length - 1] + '. ' +
        (cal[today] ? '<span class="pt-ok">Today is in the calendar.</span>' : '<span class="pt-err">Today is not in the calendar.</span>')
      : 'No calendar uploaded yet.';

    var r = PT.resolve(c, new Date()), t = r.times;
    $id('pt-today-src').textContent = '(' + { manual: 'fixed times', calculated: 'calculated', calendar: 'from the calendar' }[r.source] + ')';
    $id('pt-today').innerHTML = PT.PRAYERS.map(function (p) {
      var x = t[p] || {};
      return '<tr><td>' + NAMES[p] + '</td><td>' + esc(x.azan || '—') + '</td><td>' + esc(x.iqamah || '—') + '</td></tr>';
    }).join('');
    $id('pt-today-extra').innerHTML = 'Sunrise ' + esc(t.sunrise || '—') + ' · Sunset ' + esc(t.sunset || '—') +
      ' · Jumua ' + esc([t.jumua1, t.jumua2].filter(Boolean).join(', ') || '—') +
      (r.note ? '<br><span class="pt-err">' + esc(r.note) + '</span>' : '');
  }
  function $id(id) { return document.getElementById(id); }

  box.addEventListener('click', function (e) {
    var b = e.target.closest('[data-src]');
    if (!b) return;
    var c = cfg(); c.prayerSource = b.dataset.src; save(c);
  });
  function curtain(c) {
    c.prayerCurtain = JSON.parse(JSON.stringify(c.prayerCurtain || STOS.DEFAULTS.prayerCurtain));
    c.prayerCurtain.minutes = c.prayerCurtain.minutes || {};
    return c.prayerCurtain;
  }
  $id('pc-test').addEventListener('click', function () {
    var c = cfg(); curtain(c).test = Date.now().toString(36) + Math.random().toString(36).slice(2, 6); save(c);
    $id('pc-test-msg').textContent = 'Sent. The screen dims within a few seconds, for 15 seconds.';
  });
  box.addEventListener('input', function (e) {
    if (e.target.id === 'pc-dark') $id('pc-dark-val').textContent = '(' + e.target.value + '%)';
  });
  box.addEventListener('change', function (e) {
    var el = e.target, c = cfg();
    if (el.id === 'pc-enabled') { curtain(c).enabled = el.checked; return save(c); }
    if (el.id === 'pc-dark') { curtain(c).darkness = +el.value; return save(c); }
    if (el.dataset.pc) { curtain(c).minutes[el.dataset.pc] = Math.max(0, Math.min(120, parseInt(el.value, 10) || 0)); return save(c); }
    if (el.dataset.rule) {
      var rules = c.iqamahRules = c.iqamahRules || JSON.parse(JSON.stringify(PT.DEFAULT_RULES));
      var r = rules[el.dataset.rule] = rules[el.dataset.rule] || {};
      r[el.dataset.k] = el.dataset.k === 'minutes' ? Math.max(0, Math.min(180, parseInt(el.value, 10) || 0)) : el.value;
      save(c);
    } else if (['lat', 'lng', 'method', 'madhab', 'highLat'].indexOf(el.name) >= 0) {
      c.calc = c.calc || {}; c.calc[el.name] = el.value; save(c);
    }
  });

  $id('pt-cal-file').addEventListener('change', function (e) {
    var file = e.target.files[0];
    e.target.value = '';
    if (!file) return;
    file.text().then(function (text) {
      var res = PT.parseCSV(text), c = cfg();
      var errBox = $id('pt-cal-errors');
      if (res.count) {
        c.calendar = Object.assign(c.calendar || {}, res.rows);
        c.prayerSource = 'calendar';
        save(c);
      }
      errBox.innerHTML = (res.count ? '<span class="pt-ok">' + res.count + ' day' + (res.count === 1 ? '' : 's') + ' imported.</span>' : '') +
        (res.errors.length ? '<br><span class="pt-err">' + res.errors.slice(0, 8).map(esc).join('<br>') +
          (res.errors.length > 8 ? '<br>… and ' + (res.errors.length - 8) + ' more' : '') + '</span>' : '');
    });
  });
  $id('pt-cal-template').addEventListener('click', function () {
    var now = new Date(), blob = new Blob([PT.templateCSV(cfg(), now.getFullYear(), now.getMonth())], { type: 'text/csv' });
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'prayer-calendar-' + now.getFullYear() + '-' + String(now.getMonth() + 1).padStart(2, '0') + '.csv';
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
  });
  $id('pt-cal-clear').addEventListener('click', function () {
    if (!confirm('Remove every date from the uploaded calendar?')) return;
    var c = cfg(); c.calendar = {}; save(c);
    $id('pt-cal-errors').textContent = '';
  });

  // Keep in step with other tabs, phones and the device.
  window.addEventListener('storage', function (e) { if (e.key === STOS.STORAGE_KEY) render(); });
  render();
})();
