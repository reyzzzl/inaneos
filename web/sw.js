// Minimal COOP/COEP service worker so SharedArrayBuffer (shared wasm
// memory) works on static hosts like GitHub Pages that cannot send
// isolation headers. Local serve.py already sends them, so this is a
// no-op there (index.html only registers when not isolated).
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));
self.addEventListener('fetch', (e) => {
  if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin')
    return;
  e.respondWith(
    fetch(e.request).then((res) => {
      const headers = new Headers(res.headers);
      headers.set('Cross-Origin-Opener-Policy', 'same-origin');
      headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
      return new Response(res.body, {
        status: res.status,
        statusText: res.statusText,
        headers,
      });
    })
  );
});
