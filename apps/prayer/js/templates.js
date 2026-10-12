/* Template layouts. Each template is structure only; colours come from the
 * selected theme. `theme` is the template's original colourway and
 * `suggest` lists other themes that suit it well. Layout styles live in
 * css/display.css under .tpl-<id>. */
(function () {
  function T(n, id, name, theme, font, suggest, render) {
    return { n: n, id: id, name: name, theme: theme, font: font, suggest: suggest, thumb: 'templates/' + (n < 10 ? '0' : '') + n + '-' + id + '.png', render: render };
  }

  window.STOS_TEMPLATES = [
    T(1, 'sanctuary', 'Sanctuary', 'sapphire', 'serif', ['forest-gold', 'maroon-gold', 'night-cyan'], function (c) {
      return c.pattern('full') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) + c.datebar('a-db') +
        '<section class="a-l box sf-panel">' + c.clock('stack') + c.countdown('box', 'sf-hero') + '</section>' +
        c.table('a-r') + c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(2, 'harmony', 'Harmony', 'ivory-teal', 'serif', ['terracotta', 'parchment', 'sapphire'], function (c) {
      return c.pattern('sides') + c.lantern('ln-l') + c.lantern('ln-r') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) +
        '<section class="a-hero">' + c.chip('greg', 'sf-panel') +
        c.arch('ogee', c.clock('stack') + c.countdown('flat'), 'hero-arch sf-hero') + c.chip('hijri', 'sf-panel') + '</section>' +
        c.cards('split', 'a-cards headed') + c.info('bar', 'a-in sf-panel', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(3, 'heritage', 'Heritage', 'maroon-gold', 'serif', ['forest-gold', 'bronze', 'sapphire'], function (c) {
      return c.pattern('corners') + c.lantern('ln-l') + c.lantern('ln-r') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) + c.datebar('a-db') + c.table('a-l') +
        c.arch('ogee', c.clock('stack') + c.countdown('box', 'box'), 'a-r sf-panel') +
        c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(4, 'horizon', 'Horizon', 'emerald-neon', 'serif', ['night-cyan', 'onyx-gold', 'indigo-rose'], function (c) {
      return c.skyline('sky-mid') + c.lantern('ln-l sm') + c.lantern('ln-r sm') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) + c.clock('inline', 'a-clk') +
        c.countdown('pill', 'a-cd sf-panel') + c.cards('split', 'a-cards headed') +
        c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(5, 'radiance', 'Radiance', 'amethyst', 'serif', ['sapphire', 'maroon-gold', 'teal-coral'], function (c) {
      return c.pattern('corners') + c.lantern('ln-l') + c.lantern('ln-r') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) + c.datebar('a-db') +
        '<section class="a-clk frame">' + c.clock('inline') + '</section>' + c.countdown('pill', 'a-cd') +
        c.cards('split', 'a-cards headed') + c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(6, 'courtyard', 'Courtyard', 'terracotta', 'serif', ['ivory-teal', 'parchment', 'azure'], function (c) {
      return c.pattern('sides') + c.skyline('sky-tr') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) + c.datebar('a-db sf-panel') +
        '<section class="a-l box sf-panel">' + c.clock('stack') + c.countdown('box', 'sf-hero') + '</section>' +
        c.table('a-r') + c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(7, 'clarity', 'Clarity', 'azure', 'sans', ['arctic', 'mint', 'sapphire'], function (c) {
      return c.pattern('corners') +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) +
        '<section class="a-l box sf-hero">' + c.clock('inline') + '<div class="mini-dates">' + c.chip('greg') + c.chip('hijri') + '</div></section>' +
        '<section class="a-r box sf-hero">' + c.skyline('sky-box') + c.countdown('box') + '</section>' +
        c.cards('stack', 'a-cards headed joined') + c.info('tiles', 'a-in', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    }),

    T(8, 'minaret', 'Minaret', 'onyx-gold', 'serif', ['forest-gold', 'bronze', 'maroon-gold'], function (c) {
      return c.pattern('corners') + c.lantern('ln-a') + c.lantern('ln-b') +
        '<div class="g">' +
        c.arch('pointed', c.logo() + c.name() + c.sub() +
          '<div class="side-dates">' + c.chip('greg') + c.chip('hijri') + '</div>' + c.info('list', 'side-info', { jicon: 'mosque' }), 'a-sb sf-panel') +
        c.arch('ogee', c.clock('inline') + c.countdown('pill'), 'a-top sf-panel') +
        c.table('a-tbl') + c.footer('a-ft') + c.brand('a-br') + '</div>';
    }),

    T(9, 'serenity', 'Serenity', 'sage', 'sans', ['mint', 'arctic', 'mono'], function (c) {
      return '<div class="g">' + '<header class="hd hd-left a-hd">' + c.logo() + c.name() + c.brand() + '</header>' +
        '<section class="a-clk box sf-panel">' + c.clock('inline') + c.datesLine('', 'short') + '</section>' +
        c.countdown('box', 'a-cd box sf-panel') + c.table('a-tbl') + c.info('list', 'a-in') + c.footer('a-ft') + '</div>';
    }),

    T(10, 'beacon', 'Beacon', 'teal-coral', 'sans', ['plum-salmon', 'sapphire', 'indigo-rose'], function (c) {
      return '<div class="g">' + '<header class="hd hd-left a-hd">' + c.logo() + c.name() + c.brand() + '</header>' +
        '<section class="a-clk box sf-panel">' + c.clock('inline') + c.datesLine('', 'short') + '</section>' +
        c.countdown('bell', 'a-cd box sf-hero') + c.table('a-tbl') + c.info('bar', 'a-in sf-panel') + c.footer('a-ft') + '</div>';
    }),

    T(11, 'essence', 'Essence', 'mono', 'sans', ['sage', 'arctic', 'graphite-gold'], function (c) {
      return c.pattern('sides') +
        '<div class="g">' + '<header class="hd hd-left a-hd">' + c.logo() + c.name() + c.brand() + '</header>' +
        '<section class="a-l box sf-panel">' + c.clock('inline') + '<p class="dline stack">' + c.greg('short') + '<br>Hijri: ' + c.hijri() + '</p><hr>' +
        c.countdown('box', 'box sf-hero') + '</section>' +
        c.table('a-r softhead') + c.info('bar', 'a-in sf-panel') + c.footer('a-ft') + '</div>';
    }),

    T(12, 'mihrab', 'Mihrab', 'indigo-rose', 'sans', ['sapphire', 'emerald-neon', 'plum-salmon'], function (c) {
      return c.arch('dome', '', 'orn frame-arch') +
        '<div class="g">' + '<header class="hd hd-center-row a-hd">' + c.logo() + c.name() + c.brand() + '</header>' +
        '<section class="a-clk box sf-panel">' + c.clock('inline') + c.datesLine('', 'short') + '</section>' +
        c.countdown('bell', 'a-cd box sf-panel') + c.cards('stack', 'a-cards') + c.info('bar', 'a-in sf-panel') + c.footer('a-ft') + '</div>';
    }),

    T(13, 'balance', 'Balance', 'bronze', 'serif', ['onyx-gold', 'maroon-gold', 'forest-gold'], function (c) {
      return '<div class="g">' + c.header({ cls: 'a-hd' }) +
        '<section class="a-clk">' + c.clock('split') + c.datesLine('', 'short') + '</section><i class="a-vr"></i>' +
        c.countdown('box', 'a-cd box') + c.hrow('a-tbl') + c.info('tiles', 'a-in') + c.footer('a-ft lined') + '</div>';
    }),

    T(14, 'vista', 'Vista', 'arctic', 'sans', ['azure', 'mint', 'sage'], function (c) {
      return '<div class="g">' + c.header({ dates: 'short', cls: 'a-hd' }) +
        '<section class="a-clk box sf-panel">' + c.clock('split') + '<p class="dline stack"><b>' + c.greg('long') + '</b><br>Hijri: ' + c.hijri() + '</p></section>' +
        c.countdown('pill', 'a-cd sf-hero') + c.table('a-tbl') + c.info('bar', 'a-in gaps') + c.footer('a-ft lined') + '</div>';
    }),

    T(15, 'pillar', 'Pillar', 'plum-salmon', 'sans', ['teal-coral', 'indigo-rose', 'night-cyan'], function (c) {
      return '<div class="g">' +
        '<aside class="a-sb">' + c.logo() + c.name() + c.datesLine('stack', 'short') + c.info('list', 'side-info') + '</aside>' +
        '<section class="a-clk">' + c.clock('split') + c.datesLine('', 'long') + '</section>' + c.brand('a-br') +
        c.countdown('pill', 'a-cd') + c.table('a-tbl') + c.footer('a-ft lined') + '</div>';
    }),

    T(16, 'gathering', 'Gathering', 'parchment', 'serif', ['ivory-teal', 'terracotta', 'sage'], function (c) {
      return c.pattern('sides') +
        '<div class="g">' + c.header({ cls: 'a-hd' }) +
        '<section class="a-clk">' + c.clock('split') + c.datesLine('', 'long') + '</section>' +
        c.countdown('pill', 'a-cd accent') + c.cards('stack', 'a-cards headed') + c.info('tiles', 'a-in') + c.footer('a-ft lined') + '</div>';
    }),

    T(17, 'unity', 'Unity', 'cobalt', 'round', ['azure', 'teal-coral', 'burgundy-ice'], function (c) {
      return c.pattern('corners') +
        '<div class="g">' + c.header({ dates: 'short', cls: 'a-hd' }) +
        '<section class="a-clk box sf-panel">' + c.clock('split') + '</section>' + c.countdown('box', 'a-cd box sf-panel') +
        '<section class="a-cards box sf-panel">' + c.cards('stack', 'divided') + '</section>' +
        c.info('tiles', 'a-in') + c.footer('a-ft') + '</div>';
    }),

    T(18, 'perspective', 'Perspective', 'graphite-gold', 'round', ['onyx-gold', 'bronze', 'forest-gold'], function (c) {
      return '<div class="g">' + c.header({ dates: 'short', cls: 'a-hd' }) + c.table('a-tbl') +
        '<section class="a-side box sf-panel">' + c.clock('split') + '<p class="dline stack">' + c.greg('short') + '<br><em>Hijri: ' + c.hijri() + '</em></p><hr>' +
        c.countdown('box') + '</section>' + c.info('tiles', 'a-in') + c.footer('a-ft') + '</div>';
    }),

    T(19, 'flow', 'Flow', 'mint', 'round', ['sage', 'arctic', 'ivory-teal'], function (c) {
      return '<div class="g">' + c.header({ dates: 'short', cls: 'a-hd' }) +
        '<section class="a-hero box sf-hero">' + '<div class="hero-clk">' + c.clock('split') + '<p class="dline stack">' + c.greg('short') + '<br>Hijri: ' + c.hijri() + '</p></div><i class="vr"></i>' +
        c.countdown('box') + '</section>' + c.cards('stack', 'a-cards') + c.info('bar', 'a-in sf-panel') + c.footer('a-ft') + '</div>';
    }),

    T(20, 'gateway', 'Gateway', 'burgundy-ice', 'round', ['indigo-rose', 'cobalt', 'maroon-gold'], function (c) {
      return c.pattern('corners') +
        '<div class="g">' + c.header({ dates: 'short', cls: 'a-hd' }) +
        c.arch('pointed', c.clock('split') + '<p class="dline stack">' + c.greg('short') + '<br>Hijri: ' + c.hijri() + '</p><hr>' + c.countdown('box'), 'a-l sf-panel') +
        c.table('a-r') + c.info('bar', 'a-in sf-hero') + c.footer('a-ft') + '</div>';
    }),

    T(21, 'alcove', 'Alcove', 'forest-gold', 'condensed', ['onyx-gold', 'maroon-gold', 'sapphire'], function (c) {
      return c.pattern('corners') +
        '<div class="g">' + '<div class="a-id">' + c.logo() + c.name() + c.brand() + '</div>' +
        '<section class="a-clk">' + c.clock('inline') + '<p class="dline"><b>' + c.greg('long') + '</b></p></section>' +
        c.arch('pointed', c.countdown('big'), 'a-cd sf-panel') + c.table('a-tbl') + c.sunbar('a-sun sf-panel') + c.footer('a-ft') + '</div>';
    }),

    T(22, 'arcade', 'Arcade', 'ivory-teal', 'condensed', ['parchment', 'mint', 'terracotta'], function (c) {
      return c.pattern('sides') +
        '<div class="g">' + '<div class="a-top">' + c.logo() + c.name() + c.brand() + '<p class="dline"><b>' + c.greg('long') + '</b></p></div>' +
        c.clock('inline', 'a-clk') + c.countdown('banner', 'a-cd sf-hero') +
        '<p class="a-at"><span data-k="nextAt"></span></p>' + c.cards('split', 'a-cards arched', { sunrise: true }) + c.footer('a-ft sf-panel') + '</div>';
    }),

    T(23, 'focus', 'Focus', 'night-cyan', 'condensed', ['sapphire', 'emerald-neon', 'graphite-gold'], function (c) {
      return c.skyline('sky-l') + c.skyline('sky-r') +
        '<div class="g">' + '<section class="a-clk">' + c.logo() + '<div>' + c.clock('inline') + '<p class="dline"><b>' + c.greg('long') + '</b></p></div></section><i class="a-vr"></i>' +
        c.countdown('box', 'a-cd') + c.table('a-tbl') + c.sunbar('a-sun sf-panel') + c.footer('a-ft lined') + '</div>';
    }),

    T(24, 'legacy', 'Legacy', 'forest-gold', 'serif', ['maroon-gold', 'sapphire', 'amethyst'], function (c) {
      return c.pattern('corners') + '<div class="orn frame-line"></div>' +
        '<div class="g">' + c.header({ sub: true, cls: 'a-hd' }) +
        '<section class="a-db">' + c.chip('greg', 'sf-panel') + '<i class="sep"></i>' + c.chip('hijri', 'sf-panel') + '</section>' +
        '<section class="a-l box sf-panel">' + c.clock('stack') + c.countdown('box', 'box') + '</section>' +
        c.table('a-r') + c.info('bar', 'a-in sf-panel', { jicon: 'mosque' }) + c.footer('a-ft') + '</div>';
    })
  ];
})();
