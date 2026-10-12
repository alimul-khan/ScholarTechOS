/* Prayer-time curtain for the screen (player.html): from each iqamah, for the minutes set per prayer,
 * a semi-transparent dark layer covers every page. The template and colours stay underneath.
 * On Fridays the Jumua times are used instead of Dhuhr. Settings: cfg.prayerCurtain. */
(function () {
  var PT = STOS.prayerTimes;
  var el = document.createElement('div');
  el.id = 'prayer-curtain';
  el.style.cssText = 'position:fixed;inset:0;z-index:100;pointer-events:none;background:#000;opacity:0;transition:opacity 1.5s ease';
  document.body.appendChild(el);

  function at(day, hhmm) {
    var m = /^(\d{1,2}):(\d{2})$/.exec(hhmm || '');
    return m ? new Date(day.getFullYear(), day.getMonth(), day.getDate(), +m[1], +m[2]).getTime() : null;
  }

  // Is a congregation in progress now? (Checks yesterday too, for an Isha that runs past midnight.)
  function inPrayer(cfg, now) {
    var c = cfg.prayerCurtain || {}, mins = c.minutes || {};
    if (c.enabled === false) return false;
    for (var back = 1; back >= 0; back--) {
      var day = new Date(now.getFullYear(), now.getMonth(), now.getDate() - back);
      var t = PT.resolve(cfg, day).times, friday = day.getDay() === 5;
      var slots = PT.PRAYERS.filter(function (p) { return !(friday && p === 'dhuhr' && (t.jumua1 || t.jumua2)); })
        .map(function (p) { return [(t[p] || {}).iqamah, +mins[p] || 0]; });
      if (friday) [t.jumua1, t.jumua2].forEach(function (j) { if (j) slots.push([j, +mins.jumua || 0]); });
      for (var i = 0; i < slots.length; i++) {
        var start = at(day, slots[i][0]);
        if (start != null && slots[i][1] > 0 && now >= start && now < start + slots[i][1] * 60000) return true;
      }
    }
    return false;
  }

  // "Test" from the settings page: a new test id dims the screen for 15 seconds.
  var seenTest = (STOS.loadConfig().prayerCurtain || {}).test, testUntil = 0;
  setInterval(function () {
    var cfg = STOS.loadConfig(), c = cfg.prayerCurtain || {}, now = new Date();
    if (c.test && c.test !== seenTest) { seenTest = c.test; testUntil = now.getTime() + 15000; }
    var on = now.getTime() < testUntil || inPrayer(cfg, now);
    var dark = Math.max(20, Math.min(95, +c.darkness || 70)) / 100;
    el.style.opacity = on ? String(dark) : '0';
    document.body.dataset.curtain = on ? 'on' : 'off';
  }, 1000);

  STOS.curtain = { inPrayer: inPrayer };
})();
