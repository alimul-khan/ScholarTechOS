/* Settings page: pick a template and colour theme, manage notices and their
 * design, edit mosque details and prayer times. Every change is saved
 * straight away; the display and notices pages pick it up from localStorage
 * (notice images live in IndexedDB via STOS.media). */
(function () {
  var cfg = STOS.loadConfig();
  var $ = function (sel, root) { return (root || document).querySelector(sel); };
  var $$ = function (sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); };
  var esc = STOS.esc;
  var NOTICE_TABS = ['notices', 'ndesign', 'combined'];
  var activeTab = 'template', staleTemplateGrid = false, gridTimer, editingId = null;

  function noticeMode() { return NOTICE_TABS.indexOf(activeTab) >= 0; }
  function findNotice(id) { return (cfg.notices || []).filter(function (n) { return n.id === id; })[0]; }
  // The notice shown in previews: the one being edited, else the first one.
  function sampleNotice() {
    return findNotice(editingId) || STOS.activeNotices(cfg)[0] || (cfg.notices || [])[0] || null;
  }
  // Notices tab previews the sample notice in its own design; the design tab previews the default.
  function currentNoticeDesign() {
    if (activeTab === 'combined') return STOS.combinedDesign(cfg);
    return activeTab === 'notices' ? STOS.noticeDesignFor(cfg, sampleNotice()) : STOS.noticeDesign(cfg);
  }
  function noticeIds() { return (cfg.notices || []).map(function (n) { return n.imageId; }); }

  /* ---------- save + live preview ---------- */

  // Preview orientation is a viewing choice; it starts from the screen setting.
  var previewOrient = /^portrait/.test(cfg.options.orientation) ? 'portrait' : 'landscape';

  function previewOpts() {
    var o = { orientation: previewOrient };
    if (noticeMode()) {
      var d = currentNoticeDesign();
      o.templateId = d.templateId; o.themeId = d.themeId;
      o.notice = sampleNotice();
      o.slide = { index: 0, total: 1, placeholder: true, arrangement: d.arrangement };
      if (activeTab === 'combined') o.combined = true;
    }
    return o;
  }
  var preview = STOS.mount($('#preview'), Object.assign({ config: cfg }, previewOpts()));
  function refreshPreview() { preview.update(cfg, previewOpts()); updateNowLabel(); }

  function setPreviewOrient(o) {
    previewOrient = o;
    $$('.preview-bar .seg button').forEach(function (b) { b.setAttribute('aria-pressed', b.dataset.orient === o ? 'true' : 'false'); });
    $('#preview').classList.toggle('is-portrait', o === 'portrait');
    $$('.grid.thumbs').forEach(function (g) { g.classList.toggle('is-portrait', o === 'portrait'); });
    refreshPreview();
    renderActiveGrid();
  }
  $$('.preview-bar .seg button').forEach(function (b) { b.addEventListener('click', function () { setPreviewOrient(b.dataset.orient); }); });
  var statusEl = $('#status'), statusTimer;

  function setStatus(text, err) {
    clearTimeout(statusTimer);
    statusEl.textContent = text;
    statusEl.classList.toggle('err', !!err);
    if (!err) statusTimer = setTimeout(function () { statusEl.textContent = 'All changes saved'; }, 1500);
  }

  function save(opts) {
    var ok = STOS.saveConfig(cfg);
    if (ok) setStatus('Saved');
    else setStatus('Could not save: browser storage is unavailable', true);
    refreshPreview();
    if (!opts || !opts.skipGrids) scheduleGridRefresh();
  }

  function updateNowLabel() {
    var tplId = cfg.templateId, theme;
    if (noticeMode()) {
      var d = currentNoticeDesign();
      tplId = d.templateId; theme = STOS.getTheme(d.themeId);
    }
    var tpl = STOS.getTemplate(tplId);
    theme = theme || STOS.themeFor(cfg, tpl.id);
    var combined = activeTab === 'combined';
    $('#now-label').textContent = (combined ? 'Prayer + notices · ' : noticeMode() ? 'Notices · ' : '') + pad2(tpl.n) + ' ' + tpl.name + ' · ' + theme.name;
    $('#now-eyebrow').textContent = combined ? 'Prayer + notices screen' : noticeMode() ? 'Notices screen' : 'Prayer screen';
    $('#now-swatches').innerHTML = theme.swatch.map(function (c) { return '<i style="background:' + c + '"></i>'; }).join('');
    $('#theme-for').textContent = STOS.getTemplate(cfg.templateId).name;
  }
  function pad2(n) { return (n < 10 ? '0' : '') + n; }

  /* ---------- thumbnail grids ---------- */

  var thumbs = [];   // mounted mini renders, destroyed on re-render
  function clearThumbs() { thumbs.forEach(function (t) { t.destroy(); }); thumbs = []; }

  function thumbCard(opts) {
    var b = document.createElement('button');
    b.type = 'button';
    b.className = 'thumb';
    b.setAttribute('aria-pressed', opts.selected ? 'true' : 'false');
    b.innerHTML = '<div class="shot"></div><div class="cap">' + opts.caption + '</div>';
    b.addEventListener('click', opts.onPick);
    var m = { config: cfg, templateId: opts.templateId, themeId: opts.themeId, orientation: previewOrient, live: false };
    if (opts.notice !== undefined) { m.notice = opts.notice; m.slide = { index: 0, total: 1, placeholder: true, arrangement: opts.arrangement }; }
    if (opts.combined) m.combined = true;
    thumbs.push(STOS.mount(b.firstChild, m));
    return b;
  }
  function markPressed(grid, btn) {
    $$('.thumb', grid).forEach(function (b) { b.setAttribute('aria-pressed', b === btn ? 'true' : 'false'); });
  }

  function renderTemplateGrid() {
    var grid = $('#template-grid');
    grid.innerHTML = '';
    window.STOS_TEMPLATES.forEach(function (tpl) {
      var theme = STOS.themeFor(cfg, tpl.id);
      grid.appendChild(thumbCard({
        templateId: tpl.id, themeId: theme.id, selected: tpl.id === cfg.templateId,
        caption: '<b><span class="num">' + pad2(tpl.n) + '</span>' + esc(tpl.name) + '</b><small>' + esc(theme.name) + '</small>',
        onPick: function () { cfg.templateId = tpl.id; save({ skipGrids: true }); markPressed(grid, this); }
      }));
    });
  }

  function orderedThemes(tpl) {
    var first = [tpl.theme].concat(tpl.suggest || []);
    var picked = first.map(STOS.getTheme).filter(Boolean);
    return picked.concat(window.STOS_THEMES.filter(function (t) { return first.indexOf(t.id) < 0; }));
  }
  function themeBadge(tpl, theme) {
    return theme.id === tpl.theme ? '<span class="badge orig">Original</span>'
      : (tpl.suggest || []).indexOf(theme.id) >= 0 ? '<span class="badge">Suggested</span>' : '';
  }

  function renderThemeGrid() {
    var grid = $('#theme-grid'), tpl = STOS.getTemplate(cfg.templateId), current = STOS.themeFor(cfg, tpl.id).id;
    grid.innerHTML = '';
    orderedThemes(tpl).forEach(function (theme) {
      grid.appendChild(thumbCard({
        templateId: tpl.id, themeId: theme.id, selected: theme.id === current,
        caption: '<b>' + esc(theme.name) + '</b>' + themeBadge(tpl, theme),
        onPick: function () {
          cfg.themes[tpl.id] = theme.id;
          save({ skipGrids: true });
          markPressed(grid, this);
          staleTemplateGrid = true;
        }
      }));
    });
  }

  /* Design galleries for the notices screen and the combined screen:
   * "Match prayer screen" + the 24 designs, then "Match" + every theme. */
  var DESIGN_KINDS = {
    ndesign: { key: 'noticeDesign', resolve: function (c) { return STOS.noticeDesign(c); }, grid: '#ndesign-grid', themeGrid: '#ntheme-grid', themeFor: '#ntheme-for' },
    combined: { key: 'combinedDesign', resolve: function (c) { return STOS.combinedDesign(c); }, grid: '#cdesign-grid', themeGrid: '#ctheme-grid', themeFor: '#ctheme-for', combined: true }
  };

  function renderDesignGrids(kind) {
    var K = DESIGN_KINDS[kind], d = cfg[K.key], n = sampleNotice(), prayer = STOS.getTemplate(cfg.templateId);
    var arr = K.resolve(cfg).arrangement;
    var grid = $(K.grid);
    grid.innerHTML = '';
    var matchTheme = d.themeId !== 'match' ? STOS.getTheme(d.themeId) : STOS.themeFor(cfg, prayer.id);
    grid.appendChild(thumbCard({
      templateId: prayer.id, themeId: matchTheme.id, notice: n, combined: K.combined, arrangement: arr, selected: d.templateId === 'match',
      caption: '<b>Match prayer screen</b><small>' + esc(prayer.name) + '</small>',
      onPick: function () { d.templateId = 'match'; save({ skipGrids: true }); markPressed(grid, this); renderThemeGallery(kind); }
    }));
    window.STOS_TEMPLATES.forEach(function (tpl) {
      var theme = d.themeId !== 'match' ? STOS.getTheme(d.themeId) : STOS.themeFor(cfg, tpl.id);
      grid.appendChild(thumbCard({
        templateId: tpl.id, themeId: theme.id, notice: n, combined: K.combined, arrangement: arr, selected: d.templateId === tpl.id,
        caption: '<b><span class="num">' + pad2(tpl.n) + '</span>' + esc(tpl.name) + '</b><small>' + esc(theme.name) + '</small>',
        onPick: function () { d.templateId = tpl.id; save({ skipGrids: true }); markPressed(grid, this); renderThemeGallery(kind); }
      }));
    });
    renderThemeGallery(kind);
  }

  var themeThumbs = [];
  function renderThemeGallery(kind) {
    themeThumbs.forEach(function (t) { t.destroy(); });
    var before = thumbs.length;
    var K = DESIGN_KINDS[kind], d = cfg[K.key], n = sampleNotice(), r = K.resolve(cfg), tpl = STOS.getTemplate(r.templateId);
    var grid = $(K.themeGrid);
    grid.innerHTML = '';
    $(K.themeFor).textContent = tpl.name;
    grid.appendChild(thumbCard({
      templateId: tpl.id, themeId: STOS.themeFor(cfg, tpl.id).id, notice: n, combined: K.combined, arrangement: r.arrangement, selected: d.themeId === 'match',
      caption: '<b>Match prayer screen</b><small>' + esc(STOS.themeFor(cfg, tpl.id).name) + '</small>',
      onPick: function () { d.themeId = 'match'; save({ skipGrids: true }); renderActiveGrid(); }
    }));
    orderedThemes(tpl).forEach(function (theme) {
      grid.appendChild(thumbCard({
        templateId: tpl.id, themeId: theme.id, notice: n, combined: K.combined, arrangement: r.arrangement, selected: d.themeId === theme.id,
        caption: '<b>' + esc(theme.name) + '</b>' + themeBadge(tpl, theme),
        onPick: function () { d.themeId = theme.id; save({ skipGrids: true }); renderActiveGrid(); }
      }));
    });
    themeThumbs = thumbs.splice(before);
  }

  // Combined screen: prayer panel on the left or right.
  $$('#arr-seg button').forEach(function (b) {
    b.addEventListener('click', function () {
      cfg.combinedDesign.arrangement = b.dataset.arr;
      save({ skipGrids: true });
      renderActiveGrid();
    });
  });
  function syncArrangement() {
    var a = STOS.combinedDesign(cfg).arrangement;
    $$('#arr-seg button').forEach(function (b) { b.setAttribute('aria-pressed', b.dataset.arr === a ? 'true' : 'false'); });
  }

  function renderActiveGrid() {
    clearThumbs();
    if (activeTab !== 'notices') clearNoticeRows();
    themeThumbs.forEach(function (t) { t.destroy(); }); themeThumbs = [];
    if (activeTab === 'template') { renderTemplateGrid(); staleTemplateGrid = false; }
    else if (activeTab === 'theme') renderThemeGrid();
    else if (DESIGN_KINDS[activeTab]) {
      var kind = activeTab;
      if (kind === 'combined') syncArrangement();
      STOS.media.preload(noticeIds()).then(function () { if (activeTab === kind) renderDesignGrids(kind); });
    }
    else if (activeTab === 'notices') renderNoticeList();
  }
  // Content edits (name, times...) change every thumbnail; redraw once typing pauses.
  function scheduleGridRefresh() {
    staleTemplateGrid = true;
    clearTimeout(gridTimer);
    gridTimer = setTimeout(function () { if (['template', 'theme', 'ndesign', 'combined'].indexOf(activeTab) >= 0) renderActiveGrid(); }, 600);
  }

  /* ---------- tabs ---------- */

  function showTab(name) {
    if (!$('.panel[data-panel="' + name + '"]')) name = 'template';
    activeTab = name;
    $$('.tabs button').forEach(function (b) { b.setAttribute('aria-selected', b.dataset.tab === name ? 'true' : 'false'); });
    $$('.panel').forEach(function (p) { p.hidden = p.dataset.panel !== name; });
    refreshPreview();
    renderActiveGrid();
    try { sessionStorage.setItem('stos-tab', name); } catch (e) { /* ignore */ }
  }
  $$('.tabs button').forEach(function (b) { b.addEventListener('click', function () { showTab(b.dataset.tab); }); });

  /* ---------- notices ---------- */

  var rowThumbs = {};
  function clearNoticeRows() { Object.keys(rowThumbs).forEach(function (k) { rowThumbs[k].destroy(); }); rowThumbs = {}; }

  function layoutName(id) {
    var l = window.STOS_NOTICE_LAYOUTS.filter(function (x) { return x.id === id; })[0];
    return l ? l.name : 'Text only';
  }
  function fmtDate(d) {
    if (!d) return '';
    var p = d.split('-'), months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    return (+p[2]) + ' ' + months[(+p[1]) - 1] + ' ' + p[0];
  }
  function noticeSummary(n) {
    var bits = [layoutName(n.layout), (+n.duration || 12) + 's'];
    if (n.from && n.until) bits.push(fmtDate(n.from) + ' – ' + fmtDate(n.until));
    else if (n.from) bits.push('from ' + fmtDate(n.from));
    else if (n.until) bits.push('until ' + fmtDate(n.until));
    if (STOS.activeNotices({ notices: [n] }).length === 0 && n.enabled !== false) bits.push('not showing today');
    var nd = STOS.noticeDesignFor(cfg, n);
    if (nd.custom) bits.push(STOS.getTemplate(nd.templateId).name + ', ' + STOS.getTheme(nd.themeId).name);
    return bits.join(' · ');
  }

  function renderNoticeList() {
    STOS.media.preload(noticeIds()).then(function () {
      if (activeTab !== 'notices') return;
      clearNoticeRows();
      var list = $('#notice-list'), notices = cfg.notices || [];
      list.innerHTML = notices.length ? '' : '<p class="empty">No notices yet. Add one to get started.</p>';
      notices.forEach(function (n, i) {
        var d = STOS.noticeDesignFor(cfg, n);
        var row = document.createElement('div');
        row.className = 'nrow' + (n.enabled === false ? ' off' : '') + (n.id === editingId ? ' editing' : '');
        row.innerHTML =
          '<div class="nshot"></div>' +
          '<div class="ninfo"><b>' + esc(n.title || n.tag || 'Untitled notice') + '</b><small>' + esc(noticeSummary(n)) + '</small></div>' +
          '<div class="nactions">' +
            '<label class="switch" title="Show this notice"><input type="checkbox" data-act="toggle"' + (n.enabled !== false ? ' checked' : '') + ' aria-label="Show this notice"><span></span></label>' +
            '<button type="button" class="icon-btn" data-act="up" aria-label="Move up"' + (i === 0 ? ' disabled' : '') + '>↑</button>' +
            '<button type="button" class="icon-btn" data-act="down" aria-label="Move down"' + (i === notices.length - 1 ? ' disabled' : '') + '>↓</button>' +
            '<button type="button" class="btn" data-act="edit">' + (n.id === editingId ? 'Close' : 'Edit') + '</button>' +
          '</div>';
        row.querySelector('.nshot').addEventListener('click', function () { toggleEdit(n.id); });
        row.addEventListener('click', function (e) {
          var act = e.target.dataset && e.target.dataset.act;
          if (act === 'edit') toggleEdit(n.id);
          else if (act === 'up' || act === 'down') moveNotice(i, act === 'up' ? -1 : 1);
        });
        row.querySelector('[data-act="toggle"]').addEventListener('change', function () {
          n.enabled = this.checked; save({ skipGrids: true }); renderNoticeList();
        });
        list.appendChild(row);
        rowThumbs[n.id] = STOS.mount(row.querySelector('.nshot'), {
          config: cfg, templateId: d.templateId, themeId: d.themeId, orientation: 'landscape', live: false,
          notice: n, slide: { index: 0, total: 1, placeholder: true }
        });
        if (n.id === editingId) list.appendChild(buildEditor(n));
      });
    });
  }

  function toggleEdit(id) {
    editingId = editingId === id ? null : id;
    renderNoticeList();
    refreshPreview();
  }
  function moveNotice(i, step) {
    var a = cfg.notices, j = i + step;
    if (j < 0 || j >= a.length) return;
    var t = a[i]; a[i] = a[j]; a[j] = t;
    save({ skipGrids: true }); renderNoticeList();
  }

  $('#notice-add').addEventListener('click', function () {
    var n = { id: 'n' + Date.now().toString(36), enabled: true, layout: 'text', tag: 'Announcement', title: 'New notice',
      body: '', meta: '', imageId: '', fit: 'cover', duration: 12, from: '', until: '' };
    cfg.notices = (cfg.notices || []).concat([n]);
    editingId = n.id;
    save({ skipGrids: true });
    renderNoticeList();
    setTimeout(function () { var t = $('#ne-title'); if (t) { t.focus(); t.select(); } }, 60);
  });

  var LAYOUT_ICONS = {
    'text': '<i class="l-lines"></i>',
    'image': '<i class="l-img full"></i>',
    'image-left': '<i class="l-img half"></i><i class="l-lines"></i>',
    'image-right': '<i class="l-lines"></i><i class="l-img half"></i>',
    'image-top': '<i class="l-img top"></i><i class="l-lines short"></i>',
    'overlay': '<i class="l-img full"><i class="l-lines on"></i></i>'
  };

  function buildEditor(n) {
    var ed = document.createElement('div');
    ed.className = 'neditor';
    var layouts = window.STOS_NOTICE_LAYOUTS.map(function (l) {
      return '<button type="button" class="lay lay-' + l.id + '" data-layout="' + l.id + '" aria-pressed="' + (n.layout === l.id) + '">' +
        '<span class="lay-art">' + LAYOUT_ICONS[l.id] + '</span><span>' + l.name + '</span></button>';
    }).join('');
    var lay = window.STOS_NOTICE_LAYOUTS.filter(function (x) { return x.id === n.layout; })[0] || window.STOS_NOTICE_LAYOUTS[0];
    var url = STOS.media.cached(n.imageId);
    ed.innerHTML =
      '<div class="field wide"><span>Layout</span><div class="lay-row">' + layouts + '</div></div>' +
      '<div class="ne-grid">' +
        '<label class="field" data-needs="text"><span>Label <em>(small tag above the title)</em></span><input id="ne-tag" name="tag" type="text" maxlength="30" list="tag-suggest" value="' + esc(n.tag) + '"></label>' +
        '<label class="field" data-needs="text"><span>Title</span><input id="ne-title" name="title" type="text" maxlength="90" value="' + esc(n.title) + '"></label>' +
        '<label class="field wide" data-needs="text"><span>Message</span><textarea name="body" rows="4" maxlength="600">' + esc(n.body) + '</textarea></label>' +
        '<label class="field wide" data-needs="text"><span>Details <em>(date, time, place or contact)</em></span><input name="meta" type="text" maxlength="120" value="' + esc(n.meta) + '"></label>' +
        '<div class="field wide" data-needs="image"><span>Image</span><div class="img-row">' +
          '<div class="img-preview">' + (url ? '<img src="' + esc(url) + '" alt="">' : '<span>No image</span>') + '</div>' +
          '<div class="img-actions">' +
            '<label class="btn"><input type="file" accept="image/*" data-act="image" hidden>' + (url ? 'Replace image' : 'Upload image') + '</label>' +
            (url ? '<button type="button" class="btn ghost" data-act="remove-image">Remove</button>' : '') +
            '<div class="seg small" role="group" aria-label="Image fit">' +
              '<button type="button" data-fit="cover" aria-pressed="' + (n.fit !== 'contain') + '">Fill the space</button>' +
              '<button type="button" data-fit="contain" aria-pressed="' + (n.fit === 'contain') + '">Show whole image</button>' +
            '</div>' +
            '<small>Posters and flyers usually look best with “Show whole image”.</small>' +
          '</div></div></div>' +
        designField(n) +
        '<label class="field"><span>Show for</span><select name="duration">' +
          [8, 10, 12, 15, 20, 30, 45, 60].map(function (s) { return '<option value="' + s + '"' + ((+n.duration || 12) === s ? ' selected' : '') + '>' + s + ' seconds</option>'; }).join('') +
        '</select></label>' +
        '<div class="field"><span>Show between <em>(optional)</em></span><div class="date-row">' +
          '<input type="date" name="from" value="' + esc(n.from) + '" aria-label="Show from">' +
          '<span>to</span><input type="date" name="until" value="' + esc(n.until) + '" aria-label="Show until"></div></div>' +
      '</div>' +
      '<div class="ne-foot"><button type="button" class="btn ghost danger" data-act="delete">Delete notice</button>' +
        '<a class="btn" href="notices.html?notice=' + encodeURIComponent(n.id) + '" target="stos-notices">View full screen ↗</a>' +
        '<button type="button" class="btn primary" data-act="done">Done</button></div>';

    function syncNeeds() {
      var l = window.STOS_NOTICE_LAYOUTS.filter(function (x) { return x.id === n.layout; })[0];
      $$('[data-needs="text"]', ed).forEach(function (el) { el.hidden = !l.text; });
      $$('[data-needs="image"]', ed).forEach(function (el) { el.hidden = !l.image; });
    }
    syncNeeds();

    var rowTimer;
    function changed() {
      save({ skipGrids: true });
      clearTimeout(rowTimer);
      rowTimer = setTimeout(function () {       // refresh just this row's thumbnail + summary
        var t = rowThumbs[n.id];
        var nd = STOS.noticeDesignFor(cfg, n);
        if (t) t.update(cfg, { templateId: nd.templateId, themeId: nd.themeId, orientation: 'landscape', notice: n, slide: { index: 0, total: 1, placeholder: true } });
        var row = ed.previousElementSibling;
        if (row) { row.querySelector('.ninfo b').textContent = n.title || n.tag || 'Untitled notice'; row.querySelector('.ninfo small').textContent = noticeSummary(n); }
      }, 300);
    }

    ed.addEventListener('input', function (e) {
      var el = e.target;
      if (!el.name || el.type === 'file') return;
      n[el.name] = el.name === 'duration' ? +el.value : el.value;
      if (el.name === 'templateId') syncThemeDefault();
      changed();
    });
    // The "Default" swatch means the default notice theme, or the chosen template's own colours.
    function syncThemeDefault() {
      var b = ed.querySelector('[data-theme=""]');
      if (b) b.querySelector('span').textContent = n.templateId ? 'Template colours' : 'Default';
    }
    ed.addEventListener('click', function (e) {
      var t = e.target.closest('button');
      if (!t) return;
      if (t.dataset.layout) {
        n.layout = t.dataset.layout;
        $$('.lay', ed).forEach(function (b) { b.setAttribute('aria-pressed', b === t ? 'true' : 'false'); });
        syncNeeds(); changed();
      } else if (t.dataset.theme !== undefined) {
        n.themeId = t.dataset.theme;
        $$('[data-theme]', ed).forEach(function (b) { b.setAttribute('aria-pressed', b === t ? 'true' : 'false'); });
        changed();
      } else if (t.dataset.fit) {
        n.fit = t.dataset.fit;
        $$('[data-fit]', ed).forEach(function (b) { b.setAttribute('aria-pressed', b === t ? 'true' : 'false'); });
        changed();
      } else if (t.dataset.act === 'remove-image') {
        var old = n.imageId; n.imageId = '';
        save({ skipGrids: true });
        if (old) STOS.media.del(old);
        renderNoticeList();
      } else if (t.dataset.act === 'delete') {
        if (!confirm('Delete the notice “' + (n.title || 'Untitled') + '”?')) return;
        if (n.imageId) STOS.media.del(n.imageId);
        cfg.notices = cfg.notices.filter(function (x) { return x !== n; });
        editingId = null;
        save({ skipGrids: true }); renderNoticeList();
      } else if (t.dataset.act === 'done') {
        toggleEdit(n.id);
      }
    });
    ed.querySelector('[data-act="image"]').addEventListener('change', function () {
      var file = this.files && this.files[0];
      this.value = '';
      if (!file) return;
      setStatus('Saving image…');
      downscale(file, 1920).then(function (blob) {
        return STOS.media.put(blob);
      }).then(function (id) {
        var old = n.imageId;
        n.imageId = id;
        if (n.layout === 'text') n.layout = 'image-left';
        return STOS.media.preload([id]).then(function () {
          save({ skipGrids: true });
          if (old) STOS.media.del(old);
          renderNoticeList();
        });
      }).catch(function () { setStatus('That image could not be saved. Try a smaller JPG or PNG.', true); });
    });
    return ed;
  }

  // Per-notice design: a template (or the default notice design) and a colour theme.
  function designField(n) {
    var def = STOS.getTemplate(STOS.noticeDesign(cfg).templateId).name;
    var opts = '<option value="">Default notice design (' + esc(def) + ')</option>' + window.STOS_TEMPLATES.map(function (t) {
      return '<option value="' + t.id + '"' + (n.templateId === t.id ? ' selected' : '') + '>' + pad2(t.n) + ' ' + esc(t.name) + '</option>';
    }).join('');
    var sw = function (id, label, colors) {
      var dots = colors ? colors.map(function (c) { return '<i style="background:' + c + '"></i>'; }).join('') : '<i class="def"></i>';
      return '<button type="button" class="sw" data-theme="' + id + '" aria-pressed="' + ((n.themeId || '') === id) + '" title="' + esc(label) + '">' +
        '<b>' + dots + '</b><span>' + esc(label) + '</span></button>';
    };
    var swatches = sw('', n.templateId ? 'Template colours' : 'Default') +
      window.STOS_THEMES.map(function (t) { return sw(t.id, t.name, t.swatch); }).join('');
    return '<div class="field wide ne-design"><span>Design <em>(this notice only)</em></span>' +
      '<select name="templateId" aria-label="Notice template">' + opts + '</select>' +
      '<div class="sw-row" role="group" aria-label="Notice colour theme">' + swatches + '</div></div>';
  }

  // Resize to fit `max` px on the longest side; keeps transparency (WebP), falls back to JPEG.
  function downscale(file, max) {
    return new Promise(function (resolve, reject) {
      var url = URL.createObjectURL(file), img = new Image();
      img.onload = function () {
        var k = Math.min(1, max / Math.max(img.width, img.height));
        var cv = document.createElement('canvas');
        cv.width = Math.max(1, Math.round(img.width * k)); cv.height = Math.max(1, Math.round(img.height * k));
        cv.getContext('2d').drawImage(img, 0, 0, cv.width, cv.height);
        URL.revokeObjectURL(url);
        cv.toBlob(function (b) {
          if (b && b.type === 'image/webp') resolve(b);
          else cv.toBlob(function (j) { j ? resolve(j) : reject(); }, 'image/jpeg', 0.88);
        }, 'image/webp', 0.9);
      };
      img.onerror = function () { URL.revokeObjectURL(url); reject(); };
      img.src = url;
    });
  }

  /* ---------- mosque form ---------- */

  var mosqueForm = $('#mosque-form');
  function fillMosque() {
    mosqueForm.name.value = cfg.mosque.name;
    mosqueForm.city.value = cfg.mosque.city;
    mosqueForm.address.value = cfg.mosque.address;
    mosqueForm.showBrand.checked = !!cfg.mosque.showBrand;
    $('#logo-preview').innerHTML = cfg.mosque.logo ? '<img src="' + esc(cfg.mosque.logo) + '" alt="Logo">'
      : '<svg viewBox="0 0 64 64" fill="currentColor"><path d="M32 11c-3 4-12 7.5-15.5 14C14 30 14.5 34 15 36h34c.5-2 1-6-1.5-11C44 18.5 35 15 32 11zM13 38h38v22H13zM4 26h6v34H4zM54 26h6v34h-6z"/></svg>';
    $('#logo-remove').hidden = !cfg.mosque.logo;
  }
  mosqueForm.addEventListener('input', function (e) {
    var el = e.target;
    if (el.name === 'showBrand') cfg.mosque.showBrand = el.checked;
    else if (el.name in cfg.mosque) cfg.mosque[el.name] = el.value;
    else return;
    save();
  });

  // Logos are downscaled so they fit comfortably in browser storage.
  $('#logo-file').addEventListener('change', function () {
    var file = this.files && this.files[0];
    this.value = '';
    if (!file) return;
    var reader = new FileReader();
    reader.onload = function () {
      var img = new Image();
      img.onload = function () {
        var max = 400, k = Math.min(1, max / Math.max(img.width, img.height));
        var canvas = document.createElement('canvas');
        canvas.width = Math.max(1, Math.round(img.width * k));
        canvas.height = Math.max(1, Math.round(img.height * k));
        canvas.getContext('2d').drawImage(img, 0, 0, canvas.width, canvas.height);
        cfg.mosque.logo = canvas.toDataURL('image/png');
        fillMosque(); save();
      };
      img.onerror = function () { setStatus('That file could not be read as an image', true); };
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
  $('#logo-remove').addEventListener('click', function () { cfg.mosque.logo = ''; fillMosque(); save(); });

  /* ---------- prayer times ---------- */

  var timesForm = $('#times-form');
  function fillTimes() {
    $('#times-body').innerHTML = STOS.PRAYERS.map(function (p) {
      var t = cfg.times[p.id] || {};
      return '<tr><td>' + p.name + '</td>' +
        '<td><input type="time" data-p="' + p.id + '" data-kind="azan" value="' + esc(t.azan) + '" aria-label="' + p.name + ' azan"></td>' +
        '<td><input type="time" data-p="' + p.id + '" data-kind="iqamah" value="' + esc(t.iqamah) + '" aria-label="' + p.name + ' iqamah"></td></tr>';
    }).join('');
    ['sunrise', 'sunset', 'jumua1', 'jumua2'].forEach(function (k) { timesForm[k].value = cfg.times[k] || ''; });
  }
  timesForm.addEventListener('input', function (e) {
    var el = e.target;
    if (el.dataset.p) (cfg.times[el.dataset.p] = cfg.times[el.dataset.p] || {})[el.dataset.kind] = el.value;
    else if (el.name) cfg.times[el.name] = el.value;
    else return;
    save();
  });

  /* ---------- display options ---------- */

  var displayForm = $('#display-form');
  function fillDisplay() {
    var o = cfg.options;
    displayForm.clock24.checked = !!o.clock24;
    displayForm.seconds.checked = !!o.seconds;
    displayForm.hijriOffset.value = String(o.hijriOffset || 0);
    displayForm.orientation.value = o.orientation || 'auto';
    displayForm.messages.value = o.messages || '';
  }
  displayForm.addEventListener('input', function (e) {
    var el = e.target, o = cfg.options;
    if (el.type === 'checkbox') o[el.name] = el.checked;
    else if (el.name === 'hijriOffset') o.hijriOffset = +el.value;
    else if (el.name === 'orientation') {
      o.orientation = el.value;
      if (el.value !== 'auto') { save(); setPreviewOrient(/^portrait/.test(el.value) ? 'portrait' : 'landscape'); return; }
    }
    else if (el.name) o[el.name] = el.value;
    save();
  });

  /* ---------- backup ---------- */

  function blobToDataUrl(blob) {
    return new Promise(function (res, rej) { var r = new FileReader(); r.onload = function () { res(r.result); }; r.onerror = rej; r.readAsDataURL(blob); });
  }
  // Exports carry notice images too, so a setup can move to another screen intact.
  $('#export').addEventListener('click', function () {
    var ids = noticeIds().filter(Boolean), out = { stosExport: 2, config: cfg, media: {} };
    Promise.all(ids.map(function (id) {
      return STOS.media.get(id).then(function (b) { return b ? blobToDataUrl(b).then(function (u) { out.media[id] = u; }) : null; });
    })).catch(function () { /* export settings even if images can't be read */ }).then(function () {
      var blob = new Blob([JSON.stringify(out)], { type: 'application/json' });
      var a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'stos-display-settings.json';
      document.body.appendChild(a); a.click(); a.remove();
      setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
    });
  });
  $('#import').addEventListener('change', function () {
    var file = this.files && this.files[0];
    this.value = '';
    if (!file) return;
    file.text().then(function (txt) {
      var data = JSON.parse(txt), conf = data.stosExport ? data.config : data, imgs = (data.stosExport && data.media) || {};
      return Promise.all(Object.keys(imgs).map(function (id) {
        return fetch(imgs[id]).then(function (r) { return r.blob(); }).then(function (b) { return STOS.media.put(b, id); });
      })).then(function () {
        cfg = STOS.merge(STOS.clone(STOS.DEFAULTS), conf);
        editingId = null;
        return STOS.media.preload(noticeIds());
      });
    }).then(function () { fillAll(); save(); renderActiveGrid(); })
      .catch(function () { setStatus('That file is not a valid settings export', true); });
  });
  $('#reset').addEventListener('click', function () {
    if (!confirm('Reset every setting, including prayer times, notices and your logo, back to the defaults?')) return;
    cfg = STOS.clone(STOS.DEFAULTS);
    editingId = null;
    STOS.media.clear().catch(function () {});
    fillAll(); save(); renderActiveGrid();
  });

  // Another settings tab saved: take its version.
  window.addEventListener('storage', function (e) {
    if (e.key !== STOS.STORAGE_KEY) return;
    cfg = STOS.loadConfig();
    STOS.media.preload(noticeIds()).then(function () { fillAll(); refreshPreview(); scheduleGridRefresh(); if (activeTab === 'notices') renderNoticeList(); });
  });

  function fillAll() { fillMosque(); fillTimes(); fillDisplay(); }

  fillAll();
  updateNowLabel();
  if (previewOrient === 'portrait') setPreviewOrient('portrait');
  var startTab = 'template';
  try { startTab = sessionStorage.getItem('stos-tab') || 'template'; } catch (e) { /* ignore */ }
  STOS.media.preload(noticeIds()).then(function () { showTab(startTab); refreshPreview(); });
})();
