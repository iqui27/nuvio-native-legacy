# Diagnostic log links — source only

Adapted from Matis94's log export. This branch does not deploy or provision the service.
The existing diagnostic receipt and automatic report behavior remain unchanged.

## Separate creation authority

The client sends POST `/v1/loglink` only after the person selects RIGHT or OK in
the existing diagnostics card. Opening the panel, normal logging and automatic
reporting do not call this upload. Redaction runs before compression and HTTP.
The local diagnostic switch is off by default; turning it off closes the card.

Build configuration **NUVIO_LOGLINK_TOKEN** becomes **NV_LOGLINK_TOKEN**. The worker
checks the independent **LOGLINK_TOKEN** secret with `x-nuvio-auth: loglink`.
This credential creates links only. The server refuses the broad **DIAG_TOKEN**
on this route; the existing receipt route refuses the upload credential. Account
routes accept only their existing account authentication. No read/list/delete/admin
route accepts the upload credential. Downloads require the separate random
128-bit capability issued for that specific snapshot.

Never copy or derive the upload credential from **DIAG_TOKEN**. Creation fails
with 503 when both worker secrets match, or when its own secret, **LOGS** KV
binding or **DB** counter binding is absent. This is an extractable client
credential, so server quotas remain necessary. Do not expose it through build
output. No credential values belong in the repository, reports or captures.

Before any future deployment, provision these configuration names through the
usual secret/binding mechanism:

- Worker secret: **LOGLINK_TOKEN** (independent upload-only authority).
- Worker KV binding: **LOGS** (currently commented out in `wrangler.toml`).
- Existing worker D1 binding: **DB**, including the `limite` table from
  `migracao-005-amigos.sql`.
- Build property: **NUVIO_LOGLINK_TOKEN**. Leaving it unset disables cloud upload.

No values or namespace IDs are supplied here. This feature cannot be called live
until the configuration and deployment have occurred and been checked separately.

## Bounds and expiry

The worker accepts at most 2 MiB per request, checks both declared size and
streamed bytes, and uses atomic D1 counters for six attempts per IP per hour and
100 attempts globally per UTC day. Attempts consume quota even if their payload
is later rejected. At most four fixed day windows can overlap a rolling 72-hour
interval: the conservative live-data bound is 800 MiB plus metadata, with the
KV provider responsible for physical reclamation after TTL expiry. IP counters
use the worker's trusted `CF-Connecting-IP` header, never a client-selected key.

KV receives a 259200-second TTL. GET and HEAD also check the stored absolute
expiry and return 404 at or after it, even if KV still returns bytes. Missing,
malformed and guessed tokens return 404. Responses use `Cache-Control: no-store`.
A capability grants read access until expiry; app logs never print it. There is
no listing or administrative API. Redaction is conservative and best effort;
user review of the local snapshot remains useful before sharing.

## Offline evidence

Run with TMPDIR on the external SSD:

- `bash tests/diagnostics_port.sh`: synthetic worker requests with real in-memory
  SQLite, local HTTP snapshots, expiry, auth separation, receipt compatibility,
  redaction through the real upload thread with a stub HTTP boundary, QR texture
  reuse/replacement/deletion, explicit-action gating, and all 29 language tables.
- `bash tests/diagnostics_baseline.sh`: archived pre-port `76aa42819` fails actual
  snapshot HTTP content and link/expiry assertions; prior implementation
  `13d121214` accepts the broad token where the new test expects 401. The wrapper
  succeeds only when these specific baseline failures occur. No worktree or
  stash is modified. QR lifecycle is exercised on the current implementation;
  there is no pre-port diagnostics QR card to run that lifecycle against.
- `bash tools/build-local.sh`: compile only, no app launch or account calls.

The existing hidden `tests/registro_shot.sh` fixture can capture the approved
log panel over Settings. It does not show the new QR card, so that capture is
context proof only. No new QR composition approval or TV runtime validation is
claimed. base-dono: existing TV Glass UI log panel; no new board or visual style.
