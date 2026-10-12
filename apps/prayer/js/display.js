/* Display page: shows the saved template + theme full screen and follows
 * changes made on the settings page (in another tab or window) live.
 * Optional URL overrides for multi-screen setups:
 *   ?template=vista&theme=mint&orientation=portrait  (auto | landscape | portrait | portrait-cw | portrait-ccw) */
(function () {
  var params = new URLSearchParams(location.search);
  var overrides = {
    templateId: params.get('template') || undefined,
    themeId: params.get('theme') || undefined,
    orientation: params.get('orientation') || undefined
  };
  var screen = document.getElementById('screen');
  var view = STOS.mount(screen, Object.assign({ config: STOS.loadConfig() }, overrides));

  function refresh() { view.update(STOS.loadConfig(), overrides); }
  window.addEventListener('storage', function (e) { if (e.key === STOS.STORAGE_KEY) refresh(); });
  // Covers browsers/kiosks that don't deliver storage events (e.g. file:// pages).
  setInterval(function () {
    var raw = null;
    try { raw = localStorage.getItem(STOS.STORAGE_KEY); } catch (e) { return; }
    if (raw !== refresh.last) { if (refresh.last !== undefined) refresh(); refresh.last = raw; }
  }, 3000);

  // Controls appear on mouse movement; the cursor hides when idle.
  var timer;
  function wake() {
    document.body.classList.add('awake'); document.body.classList.remove('idle');
    clearTimeout(timer);
    timer = setTimeout(function () { document.body.classList.remove('awake'); document.body.classList.add('idle'); }, 2500);
  }
  ['mousemove', 'touchstart', 'keydown'].forEach(function (ev) { window.addEventListener(ev, wake, { passive: true }); });
  wake();

  function toggleFullscreen() {
    if (document.fullscreenElement) document.exitFullscreen();
    else if (document.documentElement.requestFullscreen) document.documentElement.requestFullscreen();
  }
  document.getElementById('fs').addEventListener('click', toggleFullscreen);
  screen.addEventListener('dblclick', toggleFullscreen);
  window.addEventListener('keydown', function (e) { if (e.key === 'f' || e.key === 'F') toggleFullscreen(); });
})();
