if (typeof window === 'undefined') {
    const isolationHeaders = {
        'Cross-Origin-Embedder-Policy': 'require-corp',
        'Cross-Origin-Opener-Policy': 'same-origin'
    };

    self.addEventListener('install', () => self.skipWaiting());
    self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));

    self.addEventListener('fetch', event => {
        if (event.request.cache === 'only-if-cached' && event.request.mode !== 'same-origin')
            return;

        event.respondWith((async () => {
            const response = await fetch(event.request);
            if (response.type === 'opaque')
                return response;

            const headers = new Headers(response.headers);
            for (const [name, value] of Object.entries(isolationHeaders))
                headers.set(name, value);

            return new Response(response.body, {
                headers,
                status: response.status,
                statusText: response.statusText
            });
        })());
    });
} else if (!window.crossOriginIsolated && navigator.serviceWorker) {
    let reloaded = false;
    const reload = () => {
        if (!reloaded) {
            reloaded = true;
            window.location.reload();
        }
    };

    navigator.serviceWorker.addEventListener('controllerchange', reload);
    navigator.serviceWorker.register(document.currentScript.src).then(registration => {
        if (registration.active && !navigator.serviceWorker.controller)
            reload();
    }).catch(error => console.error('Could not enable cross-origin isolation:', error));
}
