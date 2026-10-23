# #418 — Tizen 6.5 account transport

The report contains no TV log. The failure cannot be attributed to TLS, DNS,
IPv6, or a cold-start timeout from that report alone. These changes provide
bounded recovery and evidence; they do not claim to fix the TV's unknown fault.

Code investigation:

- `NuvioTpk65` uses the same native `nuvem_post -> rede_postar_st ->
  postarNativo` path as webOS. `src/tpk.c` and the .NET host do not rewrite
  account requests. The 6.5 manifest includes the internet privilege.
- The account POST has the same JSON body, apikey, bearer and Content-Type on
  native platforms. Platform labels still say webOS; they do not select a
  different transport. Android also has its Java HTTP fallback; the .wgt uses
  browser XHR. Success there does not establish that the TV's libcurl works.
- Tizen loads system `libcurl.so.5`/`.4`; its TLS backend and resolver can differ
  from other platforms. No TLS version or address family is forced. No evidence
  warrants a blanket TLS-version or IPv4 workaround.
- Each account POST allows 25 seconds total and 10 seconds for DNS/TCP/TLS.
  Retry permits three attempts, separated by 1 and 2 seconds, so a completely
  stalled operation can take up to 78 seconds. It runs on the existing worker.
- Generic native POST currently disables certificate/hostname verification.
  Therefore an old CA store or a certificate not-yet-valid caused by the clock
  is not an established cause in this path. The bundled `discord-ca.pem` is set
  if present; main already falls back to system trust for older .tpk resources.
  Handshake negotiation can still fail. Authenticated TLS for account requests
  is a separate existing security limitation; this diagnostic change does not
  alter the trust policy.
- The documented server contract in `sessao.h` requires an anonymous JWT for
  start/poll RPCs (otherwise 401). A working QR cannot be fabricated offline or
  safely requested without that prerequisite. Existing valid anonymous tokens
  are reused. Transport/5xx failures now stop after bounded retries instead of
  starting a second anonymous grant against the same broken transport; a signup
  HTTP rejection still falls back to the legacy grant. QR codes survive transient
  exchange failures. Queued email login suppresses further QR retries.

UI distinguishes timeout, TLS, DNS, connection and HTTP status using existing
translations. Login logs contain only fixed stage/class names and numeric
status, curl code, attempt, UTC epoch and backoff. Response bodies, URLs, email,
password, code, nonce and tokens are not emitted by the login diagnostic.
Native curl error buffers are no longer published: the backend can echo a
request URL containing secrets. Early POST setup failures now replace stale
transport errors. Startup additionally reports the loaded TLS backend.

## Evidence needed from the affected TV

After reproducing QR startup and manual login, send only these lines from
`nuvio.log` (no addon configuration or credentials):

1. Every `[login] stage=... class=... http=... curl=... attempt=...
   utc=... backoff_ms=...` for the failed attempt, including its final
   `backoff_ms=0`, and any `stage=email-response`/`stage=code-response` line.
2. `[rede] libcurl=... tls_backend=... async_dns=...` from that launch.
3. The adjacent `[rede] tempo #... ... curl=... http=... dns=... tcp=...
   tls=... total=... ms conexoes_novas=...` for the account host.

`curl=6` indicates DNS; `28` timeout; `35` TLS handshake; `60` certificate
validation; `77` CA-file loading; `7` connection failure. HTTP takes precedence
when a complete HTTP response exists. `utc` checks the TV clock against the
actual date; timings locate the stalled phase. A class alone cannot prove an
IPv6 route failure or a particular TLS-version incompatibility.

Validation is local: `bash tests/login418.sh` exercises real session/retry and
native POST code with synthetic transport failures, including secret redaction,
HTTP precedence, retry limits/backoff, recovery, certificate failures without
retry, unchanged nonce/body, anonymous fallback and manual-login priority.
Existing session email/outage/partial-token/concurrency tests cover regressions.
No TV build, installation, deployment, push or GitHub requests were performed.
