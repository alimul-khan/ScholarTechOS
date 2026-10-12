/* Keeps this page's settings in step with the device (serve.py), so a phone or computer and the
 * screen share one setup. Load after core.js and before the page script.
 *
 * Settings: the app keeps using localStorage; this script fills it from the device before the page
 * starts, sends every save to the device, and pulls changes made elsewhere every few seconds (the
 * pages already re-read localStorage when it changes).
 * Notice images: stored on the device instead of this browser's IndexedDB.
 * Opened as a plain file (no device server), nothing changes. */
(function () {
  if (location.protocol === 'file:' || !window.STOS) return;
  var KEY = STOS.STORAGE_KEY;
  var setItem = Storage.prototype.setItem;
  var rev = 0;

  function store(cfg) {
    try { setItem.call(localStorage, KEY, JSON.stringify(cfg)); } catch (e) { /* storage blocked */ }
  }

  // 1. Before the page reads its settings: take the device's copy (a short blocking request).
  var online = false;
  try {
    var x = new XMLHttpRequest();
    x.open('GET', '/api/config', false);
    x.send();
    if (x.status === 200) {
      var state = JSON.parse(x.responseText);
      online = true;
      rev = state.rev;
      if (state.config) store(state.config);
      else {
        // First run on this device: start from whatever this browser already had.
        var mine = null;
        try { mine = localStorage.getItem(KEY); } catch (e) { /* ignore */ }
        if (mine) push(JSON.parse(mine));
      }
    }
  } catch (e) { /* no device server: keep working from this browser */ }
  if (!online) return;

  // 2. Every save in this page also goes to the device.
  function push(cfg) {
    fetch('/api/config', { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ config: cfg }) })
      .then(function (r) { return r.json(); })
      .then(function (r) { rev = r.rev; })
      .catch(function () { /* retried on the next save */ });
  }
  Storage.prototype.setItem = function (k, v) {
    setItem.call(this, k, v);
    if (this === localStorage && k === KEY) {
      try { push(JSON.parse(v)); } catch (e) { /* not JSON: leave it local */ }
    }
  };

  // 3. Changes made on another phone, computer or screen arrive within a few seconds.
  setInterval(function () {
    fetch('/api/config', { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (s) {
      if (s.rev !== rev && s.config) {
        rev = s.rev;
        store(s.config);
        // Pages that listen for storage events (other tabs) won't get one for their own write; poke them.
        window.dispatchEvent(new StorageEvent('storage', { key: KEY }));
      }
    }).catch(function () { /* offline: keep showing the last settings */ });
  }, 3000);

  // 4. Notice images live on the device, so every screen and phone sees the same ones.
  var url = function (id) { return '/media/' + encodeURIComponent(id); };
  Object.assign(STOS.media, {
    put: function (blob, id) {
      id = id || 'img-' + Date.now().toString(36) + Math.random().toString(36).slice(2, 7);
      return fetch('/api/media?id=' + encodeURIComponent(id), {
        method: 'POST', headers: { 'Content-Type': blob.type || 'application/octet-stream' }, body: blob
      }).then(function (r) { if (!r.ok) throw new Error('upload failed'); return id; });
    },
    get: function (id) { return fetch(url(id)).then(function (r) { return r.ok ? r.blob() : undefined; }); },
    del: function (id) { return fetch('/api/media/' + encodeURIComponent(id), { method: 'DELETE' }); },
    keys: function () { return fetch('/api/media').then(function (r) { return r.json(); }); },
    clear: function () {
      return STOS.media.keys().then(function (ids) { return Promise.all(ids.map(STOS.media.del)); });
    },
    cached: function (id) { return !id ? '' : id.indexOf('samples/') === 0 ? id : url(id); },   // bundled sample images are plain files
    preload: function () { return Promise.resolve(); }
  });
})();
