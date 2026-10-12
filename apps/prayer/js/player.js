/* Rotates the three display pages in one screen. Default order and times:
 * prayer times 10 s -> notices 6 s -> prayer + notices 6 s -> repeat. */
(function () {
  var params = new URLSearchParams(location.search);
  var PAGES = [
    { key: 'prayer', src: 'index.html', seconds: 10 },
    { key: 'notices', src: 'notices.html', seconds: 6 },
    { key: 'combined', src: 'combined.html', seconds: 6 }
  ];

  // Pass display options (template, theme, orientation...) through to every page.
  var pass = new URLSearchParams();
  params.forEach(function (v, k) { if (!PAGES.some(function (p) { return p.key === k; })) pass.set(k, v); });
  var query = pass.toString() ? '?' + pass.toString() : '';

  var pages = PAGES.map(function (p) {
    var s = params.has(p.key) ? parseFloat(params.get(p.key)) : p.seconds;
    return { key: p.key, src: p.src + query, seconds: isFinite(s) ? s : p.seconds };
  }).filter(function (p) { return p.seconds > 0; });
  if (!pages.length) pages = [{ key: 'prayer', src: 'index.html' + query, seconds: 3600 }];

  pages.forEach(function (p) {
    var f = document.createElement('iframe');
    f.src = p.src;
    f.title = p.key;
    f.setAttribute('tabindex', '-1');
    // The screen has no mouse: hide each page's on-screen buttons (Full screen, Settings...).
    f.addEventListener('load', function () { kiosk(f); });
    document.body.appendChild(f);
    p.frame = f;
  });

  function kiosk(f) {
    try {
      var doc = f.contentDocument;
      if (!doc || !doc.head || doc.getElementById('stos-kiosk')) return;
      var st = doc.createElement('style');
      st.id = 'stos-kiosk';
      st.textContent = '.dock { display: none !important; } html, body, * { cursor: none !important; }';
      doc.head.appendChild(st);
    } catch (e) { /* not same-origin: leave the page as it is */ }
  }
  // Re-check regularly: a page that reloads or loads late gets the same treatment.
  setInterval(function () { pages.forEach(function (p) { kiosk(p.frame); }); }, 1000);

  var i = -1;
  function next() {
    if (i >= 0) pages[i].frame.classList.remove('on');
    i = (i + 1) % pages.length;
    pages[i].frame.classList.add('on');
    document.body.dataset.page = pages[i].key;
    if (pages.length > 1) setTimeout(next, pages[i].seconds * 1000);
  }
  next();
})();
