/* The settings page on the ESP32 screen. The screen has one square design, so the template choices are hidden
 * (esp32.css); this keeps the "Look" group from opening the hidden Template tab, and labels the colour tab. */
(function () {
  'use strict';
  var HIDDEN = { template: 1, ndesign: 1, combined: 1 };
  var lastLook = 'theme';
  function $(s) { return document.querySelector(s); }
  document.addEventListener('click', function (e) {
    var tab = e.target.closest && e.target.closest('.tab-group button[data-tab]');
    if (tab && tab.parentNode.dataset.group === 'look' && !HIDDEN[tab.dataset.tab]) lastLook = tab.dataset.tab;
    var group = e.target.closest && e.target.closest('.mode-tabs button[data-group="look"]');
    if (group && typeof window.STOS_showTab === 'function') {
      e.stopImmediatePropagation();
      e.preventDefault();
      window.STOS_showTab(lastLook);
    }
  }, true);
  function relabel() {
    var h = $('[data-panel="theme"] h2');
    if (h && !h.dataset.esp) { h.dataset.esp = '1'; h.textContent = 'Colour theme for this screen'; }
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', relabel); else relabel();
  /* The screen reads PNG and JPEG (not WebP) and has room for pictures up to its own size (480 px): pictures are
   * saved here as JPEG, no bigger than that. */
  var toBlob = HTMLCanvasElement.prototype.toBlob;
  if (toBlob) HTMLCanvasElement.prototype.toBlob = function (cb, type, quality) {
    var src = this, max = 480, k = Math.min(1, max / Math.max(src.width, src.height));
    if (type === 'image/webp') { type = 'image/jpeg'; quality = 0.88; }
    if (k < 1) {
      src = document.createElement('canvas');
      src.width = Math.max(1, Math.round(this.width * k));
      src.height = Math.max(1, Math.round(this.height * k));
      src.getContext('2d').drawImage(this, 0, 0, src.width, src.height);
    }
    return toBlob.call(src, cb, type, quality);
  };
  try {                                        /* a tab remembered from before, now hidden */
    var t = sessionStorage.getItem('stos-tab');
    if (HIDDEN[t]) sessionStorage.setItem('stos-tab', 'theme');
  } catch (e) { /* private mode */ }
})();
