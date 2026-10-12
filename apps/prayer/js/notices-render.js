/* Notice designs: one per prayer template, sharing its fonts, ornaments and
 * colour theme. A notice screen is a slim header (mosque + clock), the notice
 * itself in the template's panel style, and a prayer-times strip.
 *
 * Notice layouts: text | image | image-left | image-right | image-top | overlay
 * Styles in css/notice.css (.ng, .nh, .nc, .nt, .ns, .nf). */
(function () {
  // head: center | left   panel: box | hero | frame | plain | arch-ogee | arch-pointed | arch-dome
  // strip: chips | bar | slim
  function S(orn, head, panel, strip) { return { orn: orn, head: head, panel: panel, strip: strip }; }
  var STYLES = {
    sanctuary:   S(function (c) { return c.pattern('full'); }, 'center', 'box', 'chips'),
    harmony:     S(function (c) { return c.pattern('sides') + c.lantern('ln-l') + c.lantern('ln-r'); }, 'center', 'arch-ogee', 'bar'),
    heritage:    S(function (c) { return c.pattern('corners') + c.lantern('ln-l') + c.lantern('ln-r'); }, 'center', 'arch-ogee', 'chips'),
    horizon:     S(function (c) { return c.skyline(''); }, 'center', 'box', 'chips'),
    radiance:    S(function (c) { return c.pattern('corners') + c.lantern('ln-l') + c.lantern('ln-r'); }, 'center', 'frame', 'chips'),
    courtyard:   S(function (c) { return c.pattern('sides'); }, 'center', 'box', 'bar'),
    clarity:     S(function (c) { return c.pattern('corners'); }, 'left', 'box', 'chips'),
    minaret:     S(function (c) { return c.pattern('corners') + c.lantern('ln-l') + c.lantern('ln-r'); }, 'center', 'arch-pointed', 'bar'),
    serenity:    S(null, 'left', 'box', 'bar'),
    beacon:      S(null, 'left', 'box', 'chips'),
    essence:     S(function (c) { return c.pattern('sides'); }, 'left', 'box', 'bar'),
    mihrab:      S(function (c) { return c.arch('dome', '', 'orn frame-arch'); }, 'center', 'box', 'chips'),
    balance:     S(null, 'center', 'frame', 'bar'),
    vista:       S(null, 'left', 'box', 'bar'),
    pillar:      S(null, 'left', 'plain rule', 'slim'),
    gathering:   S(function (c) { return c.pattern('sides'); }, 'center', 'box', 'chips'),
    unity:       S(function (c) { return c.pattern('corners'); }, 'center', 'box', 'chips'),
    perspective: S(null, 'center', 'frame', 'bar'),
    flow:        S(null, 'left', 'hero', 'chips'),
    gateway:     S(function (c) { return c.pattern('corners'); }, 'center', 'arch-pointed', 'bar'),
    alcove:      S(function (c) { return c.pattern('corners'); }, 'left', 'arch-pointed', 'slim'),
    arcade:      S(function (c) { return c.pattern('sides'); }, 'center', 'arch-dome', 'chips'),
    focus:       S(function (c) { return c.skyline(''); }, 'left', 'plain', 'slim'),
    legacy:      S(function (c) { return c.pattern('corners') + '<div class="orn frame-line"></div>'; }, 'center', 'box', 'bar')
  };

  var LAYOUTS = [
    { id: 'text', name: 'Text only', image: false, text: true },
    { id: 'image', name: 'Image only', image: true, text: false },
    { id: 'image-left', name: 'Image left', image: true, text: true },
    { id: 'image-right', name: 'Image right', image: true, text: true },
    { id: 'image-top', name: 'Image on top', image: true, text: true },
    { id: 'overlay', name: 'Text over image', image: true, text: true }
  ];
  function layoutInfo(id) { for (var i = 0; i < LAYOUTS.length; i++) if (LAYOUTS[i].id === id) return LAYOUTS[i]; return LAYOUTS[0]; }

  var IMAGE_ICON = '<svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="3.5" stroke-linejoin="round"><rect x="6" y="10" width="52" height="44" rx="6"/><circle cx="22" cy="25" r="5"/><path d="M8 48l15-15 10 10 8-8 15 15"/></svg>';

  function head(c, style) {
    var cfg = c.cfg;
    return '<header class="nh nh-' + style.head + '">' + c.logo() +
      '<div class="nh-id">' + c.name() + c.sub() + '</div>' +
      '<div class="nh-time">' + c.clock('inline') + '<p class="dline">' + c.greg('long') + '<span class="bar">|</span>' + c.hijri() + '</p></div></header>';
  }

  function strip(c, v) {
    var t = c.cfg.times, esc = STOS.esc;
    var items = STOS.PRAYERS.map(function (p) {
      var x = t[p.id] || {};
      if (v === 'slim') return '<span class="ns-p" data-p="' + p.id + '"><small>' + p.name.toUpperCase() + '</small><b>' + c.time(x.azan) + '</b></span>';
      return '<div class="ns-p" data-p="' + p.id + '"><small>' + p.name.toUpperCase() + '</small><b>' + c.time(x.azan) + '</b><em>Iqamah ' + c.time(x.iqamah) + '</em></div>';
    }).join('');
    var next = '<div class="ns-next"><small>NEXT: <span data-k="nextName"></span></small><b data-k="countdown"></b></div>';
    return '<div class="ns ns-' + v + (v === 'bar' ? ' sf-panel' : '') + '">' + items + next + '</div>';
  }

  function foot(c, slide) {
    var dots = '';
    if (slide.total > 1) {
      for (var i = 0; i < slide.total; i++) dots += '<i' + (i === slide.index ? ' class="on"' : '') + '></i>';
    }
    var credit = c.cfg.mosque.showBrand ? '<span class="ft-credit"><img class="ft-mark" src="img/stos-mark.png?v=2" alt="">Powered by <b><span class="b1">S</span><span class="b2">T</span><span class="b3">O</span><span class="b4">S</span></b></span>' : '';
    return '<div class="nf"><span class="nf-dots">' + dots + '</span>' + credit + '</div>';
  }

  // The notice content itself. slide.placeholder draws an image placeholder
  // when no image is set (settings previews); on the real screen an
  // image layout without an image falls back to text only.
  function body(c, n, slide) {
    var esc = STOS.esc;
    if (!n) n = { layout: 'text', title: 'No notices right now', body: 'Add notices from the settings page.', tag: '', meta: '' };
    var url = c.mediaUrl(n.imageId), lay = layoutInfo(n.layout).id;
    if (!url && lay !== 'text' && !slide.placeholder) lay = 'text';
    if (lay === 'image' && !url && !slide.placeholder) lay = 'text';

    var mediaEl = url
      ? '<div class="nt-media fit-' + (n.fit === 'contain' ? 'contain' : 'cover') + '"><img class="nt-blur" src="' + esc(url) + '" alt=""><img class="nt-img" src="' + esc(url) + '" alt=""></div>'
      : '<div class="nt-media nt-ph"><i>' + IMAGE_ICON + '</i><span>Your image</span></div>';
    var hasText = n.tag || n.title || n.body || n.meta;
    var textEl = '<div class="nt-text" data-fitbox>' +
      (n.tag ? '<span class="nt-tag" dir="auto">' + esc(n.tag) + '</span>' : '') +
      (n.title ? '<h2 class="nt-title" dir="auto">' + esc(n.title) + '</h2>' : '') +
      (n.body ? '<p class="nt-body" dir="auto">' + esc(n.body) + '</p>' : '') +
      (n.meta ? '<p class="nt-meta" dir="auto">' + c.icon('calendar') + '<span>' + esc(n.meta) + '</span></p>' : '') +
      '</div>';
    if (lay === 'image') return '<div class="nt nt-image">' + mediaEl + '</div>';
    if (lay === 'text' || !hasText) return lay === 'text' ? '<div class="nt nt-text-only">' + textEl + '</div>' : '<div class="nt nt-image">' + mediaEl + '</div>';
    return '<div class="nt nt-' + lay + '">' + mediaEl + textEl + '</div>';
  }

  function panel(c, style, inner) {
    var p = style.panel;
    if (p.indexOf('arch-') === 0) return c.arch(p.slice(5), inner, 'nc nc-arch sf-panel');
    var sf = p === 'hero' ? ' sf-hero' : (p.indexOf('plain') === 0 ? '' : ' sf-panel');
    return '<section class="nc nc-' + p.replace(' ', ' nc-') + sf + '">' + inner + '</section>';
  }

  /* ---------- combined screen: prayer panel + rotating notice ---------- */

  function combinedHead(c) {
    return '<header class="nh ch">' + c.logo() + '<div class="nh-id">' + c.name() + c.sub() + '</div>' +
      '<div class="ch-dates"><p class="dline">' + c.greg('long') + '</p><p class="dline">Hijri: ' + c.hijri() + '</p></div></header>';
  }
  // The prayer panel borrows the template's surface: hero / frame / plain / box.
  function prayerPanel(c, style) {
    var p = style.panel, cls = p === 'hero' ? 'sf-hero cp-box' : p === 'frame' ? 'sf-panel cp-frame' : p.indexOf('plain') === 0 ? 'cp-plain' : 'sf-panel cp-box';
    return '<section class="cp ' + cls + '">' + c.clock('inline') + c.countdown('box', 'cp-cd') + c.table('cp-tbl') + c.info('tiles', 'cp-info', { jicon: 'people' }) + '</section>';
  }

  window.STOS_renderCombined = function (tpl, c, notice, slide) {
    var style = STYLES[tpl.id] || STYLES.sanctuary;
    return (style.orn ? style.orn(c) : '') +
      '<div class="cg cg-' + (slide.arrangement || 'prayer-left') + '">' + combinedHead(c) + prayerPanel(c, style) +
      panel(c, style, body(c, notice, slide)) +
      '<div class="cf"><span class="nf-dots">' + dots(slide) + '</span>' + c.footer('cf-ft') + '</div></div>';
  };
  function dots(slide) {
    var out = '';
    if (slide.total > 1) for (var i = 0; i < slide.total; i++) out += '<i' + (i === slide.index ? ' class="on"' : '') + '></i>';
    return out;
  }

  window.STOS_NOTICE_LAYOUTS = LAYOUTS;
  window.STOS_renderNotice = function (tpl, c, notice, slide) {
    var style = STYLES[tpl.id] || STYLES.sanctuary;
    return (style.orn ? style.orn(c) : '') +
      '<div class="ng ng-' + style.head + ' ng-' + style.strip + '">' + head(c, style) + panel(c, style, body(c, notice, slide)) +
      strip(c, style.strip) + foot(c, slide) + '</div>';
  };
})();
