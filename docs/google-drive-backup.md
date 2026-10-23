# Google Drive backup: release registration

The project owner registers Nuvio once. Each user then authorizes their own
Google account and stores backups in their own Drive. Users do not create
Google Cloud projects, and the app does not share backups automatically.

## Register the production application

1. Create or select an owner-controlled project at
   https://console.cloud.google.com/ and enable **Google Drive API**.
2. In **Google Auth Platform → Branding**, configure the application name,
   support email, developer contact and the project's published privacy policy.
   Configure **Audience** for the intended users. During external testing, add
   the Google accounts that will test sign-in under **Test users**.
3. In **Data Access**, add only
   `https://www.googleapis.com/auth/drive.file`. This feature uses a visible
   **Nuvio Backups** folder and app-created files. Do not substitute
   `drive.appdata`: the TV device flow rejected that scope in registration
   preflight. Full Drive access is unnecessary.
4. In **Clients**, create an **Android** OAuth client. Use the production APK's
   actual application ID and release signing certificate SHA-1. Register each
   supported package/signing certificate combination separately. The test
   package and test certificate must not be used as production defaults.
5. Create a **TVs and Limited Input devices** client in the same project. This
   enables the shared device-code sign-in on Android TV/Fire TV, LG webOS,
   and Samsung Tizen (TPK and WGT) without requiring Google Play Services. Keep the client configuration in release
   configuration, outside source control.
6. Set the build environment values consumed by Gradle and `tools/env.sh`:

   ```text
   NUVIO_GOOGLE_ANDROID_CLIENT_ID=<owner Android client ID>
   NUVIO_GOOGLE_TV_CLIENT_ID=<owner TV client ID>
   NUVIO_GOOGLE_TV_CLIENT_SECRET=<owner TV client credential>
   ```

   Use protected CI configuration for the TV credential and do not echo it in
   build logs. This is an installed application: values embedded in its APK
   cannot be treated as confidential server credentials. User access and
   refresh tokens must never be committed or included in settings backups.
7. Build and sign using the existing production package and signing identity.
   Validate Google authorization, backup creation, listing, password-protected
   restore, cancellation and account/profile switching with that exact build.
   Validate both phone and TV authorization paths. A test-registration APK's
   successful sign-in does not validate the production registration.
8. Before general release, move the OAuth application out of Testing and
   complete any verification Google requires for the configured branding,
   audience and scopes. Check Google's current testing token-expiry rules.

## User behavior and limits

- Backups are private unless their Google Drive owner deliberately shares
  them. A visible folder does not grant other users access.
- Encryption/decryption happens on the device. The user creates and confirms
  a backup password, then enters it to restore. The password is not saved or
  uploaded; losing it prevents recovery.
- Backups are bound to the signed-in Nuvio account/profile. Existing
  destination connections are preserved; supported missing local connections
  are merged. Hardware settings require explicit restore opt-in.
- Nuvio-synchronized preferences and service login sessions remain separate.
  Caches and generated programme/artwork data are not settings backups.
- Google authorization is tied to the release registration. Do not promise
  that development-registration backups will be visible to a different
  production OAuth project under `drive.file`; validate migration separately.
- Android phones retain Google's native authorization UI and Android's
  encrypted credential store. LG webOS and Samsung Tizen use the same
  limited-input OAuth client and device-code flow as Android TV. Keep all
  platform clients in the same Google Cloud project for `drive.file` visibility.
- LG/Samsung keep a separate refresh credential per Nuvio account/profile and
  OAuth client in the app's private installation storage; it is not a backup
  section. Native files use mode 0600; WGT persists through the app's IndexedDB.
  Backup passwords and access tokens are never written to that file.
- WGT restore waits for an acknowledged IDBFS flush after each transaction
  phase. A rejected/timed-out flush retains recovery state instead of reporting
  success. Ordinary background persistence keeps its existing scheduling.
- Desktop builds are development targets; a TV device-code OAuth registration
  is not used as a desktop or browser sign-in fallback.

## References

- https://developers.google.com/workspace/drive/api/guides/api-specific-auth
- https://developers.google.com/identity/authorization/android
- https://developers.google.com/identity/protocols/oauth2/limited-input-device

## Cross-platform verification

The backup envelope is unchanged: AES-256-GCM with PBKDF2-HMAC-SHA256 (600,000
iterations), the same account/profile binding and password on every target.
The TV build scripts bundle only the required mbedTLS 3.6.7 primitives from a
checksum-verified source archive. Android continues using its existing mbedTLS.

Verify phone -> TV and TV -> phone backup/restore with the same Nuvio profile
and Google project, including imported fonts, service connections, password
errors, cancellation, a restart and optional hardware settings. On WGT, inject
a storage error and reload during restore: a memory-only write is not a pass.
Real Google approval and physical-TV operation remain release validation steps.
