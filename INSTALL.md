# Installing on another TV

Download the `.ipk` (**~47 MB**) from the
[releases page](https://github.com/iqui27/nuvio-native-legacy/releases/latest),
or build it yourself:

```bash
bash tools/arm.sh --ipk
```

The package contains **no credentials**. `tools/testa-ipk.sh` proves it, and
`arm.sh` aborts and deletes the package if one appears.

---

---

## Hisense VIDAA (Experimental)

VIDAA apps are web-based, hosted at a URL. Nuvio is available at:

**`https://nuvio-recomendacoes.henriquef29.workers.dev/tv/`**

Two builds are available; the TV picks automatically:
- **Multithreaded (mt)** — uses SharedArrayBuffer, faster if supported
- **Single-threaded (st)** — fallback for older TVs

### Option A: Bookmark (simplest)

Open the TV browser and bookmark the URL above. No installation needed.

### Option B: Sideload with Developer Mode

For systems that require app to be in the official app list:

```bash
# Enable Developer Mode on TV: Settings > System > About > 1234
# Then run the installer:
sudo python3 tools/vidaa-instalar/instalar.py

# Or with custom IP:
sudo python3 tools/vidaa-instalar/instalar.py --ip 192.168.1.100
```

This creates a local DNS/HTTPS server that mimics vidaahub.com, allowing the TV
to call `Hisense_installApp()`. After installation, change DNS back to automatic.

See `tools/vidaa-instalar/README.md` for detailed instructions.


## Do you need root? No. But read the LS2 caveat.

| | Developer Mode | Homebrew Channel | Root |
|---|---|---|---|
| Root required | no | no, to install the channel | yes |
| Lifetime | **50 h session**, renewable | permanent | permanent |
| LG developer account | required | only to install the channel | no |
| Install with | `ares-install` | from the TV itself | `bash tools/arm.sh` |

### Which webOS versions

webOS **4.x** is measured, on a C9. webOS **5+** is reported working by users.
webOS **3.x** has an experimental build on the `webos3` branch — see the webOS 3
section of the [README](README.md) for what was changed, how it was verified
against retail firmware symbol dumps, and the three things that verification
cannot answer. Nobody here owns a webOS 3 set, so treat it as an experiment: for
those TVs the [web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) is
the safer choice.

### Developer Mode

```bash
ares-setup-device
ares-install space.nuvio.native.legacy_<version>_arm.ipk -d <name>
ares-launch space.nuvio.native.legacy -d <name>
```

`tools/arm.sh` is **not** the tool for this: it uses root over port 22 with a
password, while `ares-install` expects `prisoner@<ip>:9922`, the Developer Mode
ssh.

### Homebrew Channel

Install the `.ipk` through the channel. It puts apps in
`/media/developer/apps/usr/palm/applications/`, which is **exactly** the
directory this app runs from on the development TV — same directory, same
environment.

---

## The caveat that matters: LS2

The risk is not the installation. It is the **bus permission**.

This app talks directly to `luna://com.webos.media` and uses `libAcbAPI` to
drive the video plane. `src/video.c` records that the direct call to
`com.webos.service.tv.display` is **refused by the hub** even on the rooted TV
("Not permitted to send to com.webos.service.tv.display"), because of the role
the app registers under — which is why it goes through libAcbAPI instead, the
path the TV's own browser uses.

And the package **ships no LS2 role file**: only `appinfo.json`, with
`requiredPermissions: ["all"]`. So it depends on what the install directory
grants. On a rooted TV that is permissive. **On a TV without root, this has not
been measured.**

**Likely symptom if it is not granted:** the UI opens normally and video is a
black screen, with or without audio — exactly the failure mode `video.c`
describes when ACB cannot bring up the plane.

What argues in favour, as evidence rather than a promise: this TV runs
`com.limelight.webos` (Moonlight, a **native** app with a media pipeline) and
`org.webosbrew.hbchannel` from the same directory. A native video app
distributed that way works on the device.

**How to settle it:** run it on a TV without root. That cannot be answered from
here — this TV has no Developer Mode app installed and port 9922 is closed
(checked).

---

## What you get on first launch

1. **QR login.** The account code is 32 hex digits, which is why it is a QR and
   not something you type. The session is stored and the refresh token renews
   itself: you sign in once.
2. **Profile selection**, if the account has more than one.
3. Addons, layout settings, TMDB key and watch progress come **from the
   account**.
4. **Trakt is linked on the TV itself**: Settings → Account → Trakt. The account
   does not store a Trakt credential (measured:
   `sync_pull_provider_credentials` returns tmdb, mdblist, animeskip, introdb and
   the debrid providers, never `trakt`). The Trakt code is 8 characters and easy
   to type on a phone.
5. **Simkl** can be linked too, but no screen in this app consumes Simkl yet —
   it exists so the credential reaches the account and from there the web app.

## What still bothers me

- **~47 MB**, nearly all prebaked artwork, so the home screen has something to
  show before you sign in. It used to be 175 MB and to carry the packager's own
  catalogue and addon keys; `arm.sh` now excludes every personal file and
  verifies the exclusion against the finished package, deleting it if one
  appears.
- **50 h** sessions in Developer Mode. LG's limit, not the app's.
- The package uses the id `space.nuvio.native.legacy` so it can coexist with the
  web app (`space.nuvio.webos`) on the same TV. See PORT-LEGACY.md.
