# Personal servers implementation plan

**Goal:** Implement the owner-approved personal-server flows inside the redesigned Library.
**Spec:** `/Volumes/ExternalSSD/tmp/servidores-polish/`, design commit `5a0726edc`.
**Architecture:** Reuse `servidores.c`, provider workers, shared keyboard, QR generator, source ranking and row editor. Keep network operations off the render thread and bind results to the current profile.
**Constraints:** pt-BR UI through every language table; official provider marks/Lucide only; bounded allocations; no push/deploy/stash. The owner's explicit approval supersedes the older release assignment in DECISOES.md.

- [x] Add failing Library membership/filter regression; implement canonical and native memberships without changing pages; verify and commit.
- [x] Add setup protocol/state regressions; implement discovery, users, Emby Connect and provider setup using existing cancellation/storage contracts; verify and commit. Reconnect persistence remains a gap.
- [ ] Replace setup/manage presentation with approved geometry, connect Library/list/row/source/detail paths, localize all strings; verify and commit. Functional integration exists; exact geometry, row controls and recovery states remain incomplete.
- [ ] Run focused suite, i18n checks and final local build. Capture and open mock/app PNGs per screen, record remaining differences honestly. Setup/Library evidence exists; complete Detail/Sources/Home-row evidence remains incomplete.

Review focus: duplicate canonical titles; native unmatched titles; expired authorization; Back/profile changes during network work; no token/password/secret in logs or QR; unavailable/offline servers; zero selected libraries; weak-TV render cost.
