# AIOStreams phone connection

The dashboard uses the app's HTTPS QR and expiring session pattern. It does not use the plain HTTP LAN text-entry helper for credentials. The shared worker serves `/aio/`; its `AioPair` Durable Object supplies a five-minute, memory-only inbox. No database, KV, analytics or credential logs are used.

The QR fragment contains a random 128-bit phone capability. Only its first eight hex characters identify the inbox; every operation also needs the full phone capability or the independent 128-bit TV capability. Both come from `/dev/urandom`, without a weak fallback. Credentials never enter a URL, query string, browser history or persistent browser storage. Refreshing the phone page requires scanning again.

The phone displays the manifest's exact HTTPS origin and calls that origin's `/api/v1/user` with Basic authorization, redirects disabled. After success it posts UUID/password to the pairing inbox over HTTPS. The TV consumes the inbox once, rejects an origin mismatch, and independently checks `/api/v1/user` over verified HTTPS with redirects disabled before accepting authorization. The dashboard then refreshes. Closing the screen or changing account/profile cancels acceptance; pending sessions expire after five minutes. Success removes the pending credentials immediately, retaining only public status until expiry.

The user's instance must permit the browser's cross-origin `/api/v1/user` request. If it rejects CORS, redirects, credentials, or TLS verification, the phone shows an error and sends no credentials to the inbox. Manual UUID/password entry remains available as the secondary action. A package without an HTTPS `NV_REC_URL`, or a worker without the new routes, shows pairing unavailable.

## Deployment required, not performed

1. Deploy `servidor/recomendacoes` as the existing `nuvio-recomendacoes` worker, including `src/aiopair.js` and the `index.js` route/export changes.
2. Apply Wrangler migration `v1-aiopair`, creating SQLite Durable Object class `AioPair`, with binding `AIOPAIR` from `servidor/recomendacoes/wrangler.toml`. No D1 SQL migration, KV namespace, new secret, or TV-site deployment is needed.
3. Ship a native TV build containing `src/aiopair.c`, `src/aiopair.h`, dashboard/i18n changes, and the existing HTTPS `NUVIO_REC_URL` build setting. Keep the worker's existing environment and bindings.

Validation: `bash tests/aiostreams.sh` exercises the real TV polling/validation code, real phone script, a local HTTPS fake instance and worker session handler, origin rejection, wrong credentials, redirects, expiry, single-use delivery and log privacy. The test uses synthetic data only. No real instance was contacted and no worker was deployed.
