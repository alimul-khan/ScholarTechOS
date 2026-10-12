/* Notices screen (notices.html) and combined prayer + notices screen
 * (combined.html, <body data-mode="combined">): shows today's enabled notices
 * one after another, each for its own duration. Follows settings changes live.
 * URL overrides: ?template=vista&theme=mint&orientation=portrait&notice=<id> (pins one notice)
 * and, on the combined screen, &arrangement=prayer-left|prayer-right */
(function () {
  var params = new URLSearchParams(location.search);
  var combined = document.body.dataset.mode === 'combined';
  var pinned = params.get('notice');
  var screen = document.getElementById('screen');
  var cfg = STOS.loadConfig(), list = [], index = 0, timer = null, view = null;

  // Each notice can carry its own design; URL overrides win over everything.
  function design(n) {
    if (combined) {                        // one design for the whole screen
      var cd = STOS.combinedDesign(cfg), ct = params.get('template');
      var ctheme = params.get('theme') ||
        (ct && (cfg.combinedDesign || {}).themeId === 'match' ? STOS.themeFor(cfg, ct).id : cd.themeId);
      return { templateId: ct || cd.templateId, themeId: ctheme, orientation: params.get('orientation') || undefined,
        combined: true, arrangement: params.get('arrangement') || cd.arrangement };
    }
    var d = STOS.noticeDesignFor(cfg, n), tpl = params.get('template');
    // A template override takes that template's theme unless notices have their own.
    var theme = params.get('theme') ||
      (tpl && !(n && n.themeId) && (cfg.noticeDesign || {}).themeId === 'match' ? STOS.themeFor(cfg, tpl).id : d.themeId);
    return {
      templateId: tpl || d.templateId,
      themeId: theme,
      orientation: params.get('orientation') || undefined
    };
  }

  function buildList() {
    list = STOS.activeNotices(cfg);
    if (pinned) list = (cfg.notices || []).filter(function (n) { return n.id === pinned; });
  }

  function opts() {
    var n = list.length ? list[index] : null, o = design(n);
    o.config = cfg;
    o.notice = n;
    o.slide = { index: index, total: list.length, arrangement: o.arrangement };
    return o;
  }

  function show(fade) {
    clearTimeout(timer);
    var paint = function () {
      if (!view) view = STOS.mount(screen, opts());
      else view.update(cfg, opts());
      screen.classList.remove('fading');
      if (list.length > 1) {
        var secs = Math.max(3, +list[index].duration || 12);
        timer = setTimeout(function () { go(1); }, secs * 1000);
      }
    };
    if (fade && view) { screen.classList.add('fading'); setTimeout(paint, 450); }
    else paint();
  }

  function go(step) {
    if (!list.length) return;
    index = (index + step + list.length) % list.length;
    show(true);
  }

  function ids() { return (cfg.notices || []).map(function (n) { return n.imageId; }); }

  function reload() {
    var currentId = list[index] && list[index].id;
    cfg = STOS.loadConfig();
    STOS.media.preload(ids()).then(function () {
      buildList();
      var keep = list.map(function (n) { return n.id; }).indexOf(currentId);
      index = keep >= 0 ? keep : 0;
      show(false);
    });
  }

  buildList();
  STOS.media.preload(ids()).then(function () { show(false); });

  window.addEventListener('storage', function (e) { if (e.key === STOS.STORAGE_KEY) reload(); });
  var lastRaw;
  setInterval(function () {                 // for browsers/kiosks without storage events
    var raw = null;
    try { raw = localStorage.getItem(STOS.STORAGE_KEY); } catch (e) { return; }
    if (lastRaw !== undefined && raw !== lastRaw) reload();
    lastRaw = raw;
  }, 3000);
  // A notice's date range may start or end at midnight.
  var day = new Date().getDate();
  setInterval(function () { if (new Date().getDate() !== day) { day = new Date().getDate(); reload(); } }, 60000);

  // Controls appear on mouse movement; the cursor hides when idle.
  var idle;
  function wake() {
    document.body.classList.add('awake'); document.body.classList.remove('idle');
    clearTimeout(idle);
    idle = setTimeout(function () { document.body.classList.remove('awake'); document.body.classList.add('idle'); }, 2500);
  }
  ['mousemove', 'touchstart', 'keydown'].forEach(function (ev) { window.addEventListener(ev, wake, { passive: true }); });
  wake();

  function toggleFullscreen() {
    if (document.fullscreenElement) document.exitFullscreen();
    else if (document.documentElement.requestFullscreen) document.documentElement.requestFullscreen();
  }
  document.getElementById('fs').addEventListener('click', toggleFullscreen);
  document.getElementById('prev').addEventListener('click', function () { go(-1); });
  document.getElementById('next').addEventListener('click', function () { go(1); });
  screen.addEventListener('dblclick', toggleFullscreen);
  window.addEventListener('keydown', function (e) {
    if (e.key === 'f' || e.key === 'F') toggleFullscreen();
    else if (e.key === 'ArrowRight') go(1);
    else if (e.key === 'ArrowLeft') go(-1);
  });
})();
