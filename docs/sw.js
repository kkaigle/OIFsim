// Network first for the game page so new versions arrive as soon as you are online; cached copy when offline.
const C='itc-v138',F=["./","index.html","fonts.css","three.min.js","manifest.webmanifest","icon-192.png","icon-512.png","apple-touch-icon.png","fonts/barlow-latin-400-normal.woff2","fonts/barlow-latin-500-normal.woff2","fonts/barlow-latin-600-normal.woff2","fonts/saira-condensed-latin-500-normal.woff2","fonts/saira-condensed-latin-600-normal.woff2","fonts/saira-condensed-latin-700-normal.woff2","fonts/saira-stencil-one-latin-400-normal.woff2","fonts/share-tech-mono-latin-400-normal.woff2"];
self.addEventListener('install',e=>{e.waitUntil(caches.open(C).then(c=>c.addAll(F)).then(()=>self.skipWaiting()))});
self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==C).map(k=>caches.delete(k)))).then(()=>self.clients.claim()))});
self.addEventListener('fetch',e=>{const u=new URL(e.request.url);if(e.request.method!=='GET'||u.origin!==location.origin)return;
 const page=e.request.mode==='navigate'||u.pathname.endsWith('/index.html');
 if(page){e.respondWith(fetch(e.request).then(r=>{const k=r.clone();caches.open(C).then(c=>c.put('index.html',k));return r}).catch(()=>caches.match('index.html')));return}
 e.respondWith(caches.match(e.request).then(m=>m||fetch(e.request).then(r=>{const k=r.clone();caches.open(C).then(c=>c.put(e.request,k));return r})))});
