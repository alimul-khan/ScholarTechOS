/* Settings page: a drag bar between the preview and the settings to change their widths.
 * The position is remembered in this browser only (a viewing preference, not a screen setting). */
(function () {
  var layout = document.querySelector('.layout');
  var preview = document.querySelector('.preview-col');
  if (!layout || !preview) return;
  var KEY = 'stos-manage-preview-width';

  var bar = document.createElement('div');
  bar.className = 'split-bar';
  bar.setAttribute('role', 'separator');
  bar.setAttribute('aria-orientation', 'vertical');
  bar.setAttribute('aria-label', 'Drag to resize the preview and settings');
  bar.tabIndex = 0;
  preview.after(bar);

  var style = document.createElement('style');
  style.textContent =
    '.layout{grid-template-columns:var(--preview-w,46%) 14px 1fr;column-gap:12px}' +
    '.split-bar{align-self:stretch;cursor:col-resize;border-radius:7px;position:relative;touch-action:none;min-height:200px}' +
    '.split-bar::after{content:"";position:absolute;top:0;bottom:0;left:50%;width:4px;margin-left:-2px;border-radius:2px;background:var(--border,#3a3f4b)}' +
    '.split-bar:hover::after,.split-bar.dragging::after,.split-bar:focus-visible::after{background:var(--accent,#4da0ff);width:6px;margin-left:-3px}' +
    '@media (max-width: 980px){.split-bar{display:none}.layout{grid-template-columns:1fr}}';
  document.head.appendChild(style);

  function apply(px) {
    var box = layout.getBoundingClientRect();
    var min = 280, max = box.width - 420;
    px = Math.max(min, Math.min(max, px));
    layout.style.setProperty('--preview-w', Math.round(px) + 'px');
    window.dispatchEvent(new Event('resize'));          // previews rescale to the new width
    return px;
  }
  try { var saved = parseFloat(localStorage.getItem(KEY)); if (saved) apply(saved); } catch (e) { /* ignore */ }

  bar.addEventListener('pointerdown', function (e) {
    e.preventDefault();
    bar.setPointerCapture(e.pointerId);
    bar.classList.add('dragging');
    var left = layout.getBoundingClientRect().left + parseFloat(getComputedStyle(layout).paddingLeft);
    function move(ev) { apply(ev.clientX - left - 6); }
    function up(ev) {
      bar.classList.remove('dragging');
      bar.removeEventListener('pointermove', move);
      bar.removeEventListener('pointerup', up);
      try { localStorage.setItem(KEY, String(preview.getBoundingClientRect().width)); } catch (er) { /* ignore */ }
    }
    bar.addEventListener('pointermove', move);
    bar.addEventListener('pointerup', up);
  });
  // Keyboard: arrows move the bar by 40 px.
  bar.addEventListener('keydown', function (e) {
    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight') return;
    var w = apply(preview.getBoundingClientRect().width + (e.key === 'ArrowLeft' ? -40 : 40));
    try { localStorage.setItem(KEY, String(w)); } catch (er) { /* ignore */ }
  });
})();
