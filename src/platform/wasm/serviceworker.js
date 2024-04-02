// TODO why wont the new service worker ever install/activate??

const cacheName = 'mgba-GITCOMMIT';

self.addEventListener('fetch', event => {
  event.respondWith((async () => {
    const cachedResponse = await caches.match(event.request);
    if (cachedResponse)
      return cachedResponse;
    const responseFromNetwork = await fetch(event.request);
    const cache = await caches.open(cacheName);
    await cache.put(event.request, responseFromNetwork.clone());
    return responseFromNetwork;
  })());
});

self.addEventListener('install', event => {
  console.log(`sw ${cacheName} install`);
  self.skipWaiting();
  event.waitUntil((async () => {
    const cache = await caches.open(cacheName);
    await cache.addAll([
      // TODO find a good way to maintain this list.
      // maybe i could have cmake generate this as part of the build?
      // and make a new cache/version based on the commit hash?
      '/index.html',
      '/manifest.json',
      '/build/mgba.js',
      '/build/mgba.wasm',
      '/controls.js',
      '/game.js',
      '/game.css',
      '/gamemenu.js',
      '/menu.js',
      '/settings.js',
      '/style.css',
      '/fileloader.js',
      '/storage.js',
      '/init.js',
      '/icons/mgba.ico',
      '/icons/mgba-1024-square.png',
    ]);
  })());
});

self.addEventListener('activate', function (event) {
  console.log(`sw ${cacheName} activate`);
  event.waitUntil(
    caches.keys().then(function (cacheNames) {
      return Promise.all(
        cacheNames
          .filter(function (name) {
            // Return true if you want to remove this cache,
            // but remember that caches are shared across
            // the whole origin
            return name != cacheName;
            if (name != cacheName) {
              return true;
            }
          })
          .map(function (name) {
            console.log(`sw ${cacheName} activate deleting cache: ${name}`);
            return caches.delete(name);
          }),
      );
    }),
  );
});
