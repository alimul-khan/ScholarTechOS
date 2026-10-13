# Handover: where the work stands (13 October 2026)

Continuing on another computer? Copy this conversation's folder `~/.claude/projects/-Users-alim-Desktop-ScholarTechOS/`
to the same place, keep the repos at the same paths, and run `claude --resume 4679a824-7e9c-46b8-9cdc-54cb7439baf6`
in `~/Desktop/ScholarTechOS`. Otherwise start a new session and ask Claude to read this file.

## Repos (all private, on GitHub under alimul-khan)

| Repo | Local path | State |
|---|---|---|
| ScholarTechOS | `~/Desktop/ScholarTechOS` | `main` pushed. Buildroot OS for Pi 4 (dev) and Pi Zero 2 W (target). |
| display_template | `~/Desktop/display_template` | `main` pushed. The prayer display web app, shared by every platform. Release tag `v1.0`; four newer commits not yet released (start-up splash page, lighter player, stepped footer-logo zoom). |
| Android_TV_app | `~/Desktop/PhoneApplications/Android_TV_app` | `main` pushed, reorganised: `prayerdisplay/` (Android TV + Fire TV builds), `controller/`, `roku/`, `vega/`, `server/`, `tools/`, `docs/`. Old branches are all merged into `main` and can be deleted. |
| EZ_ads | `~/Desktop/EZ_ads` | **One local commit not pushed on purpose** (`d4b0ba5`, ScholarTechOS screen pairing etc.): pushing may deploy the live site. Dev server: `cd web && APP_URL=http://<this Mac>:3000 EZADS_DEV_NO_LOGIN=1 npx next dev -H 0.0.0.0 -p 3000`. |

## Devices on the home network (10.0.0.x)

| Device | Address | Access | Running |
|---|---|---|---|
| Raspberry Pi 4 (dev board) | 10.0.0.59 | `ssh root@10.0.0.59` (key `~/.ssh/id_ed25519`) | Older ScholarTechOS image, upgraded by hand to the prayer version: app in `/opt/scholartech/prayer`, settings in `/var/lib/scholartech/prayer`, `APP=prayer`, Regina time zone, 1080p forced (EDID file + `hdmi_mode=16`, for the 4K monitor), DNS fix, update key installed. No `git`, so it cannot download app updates until it runs a new image. |
| Fire TV Stick (Fire OS 7, AFTSSS) | 10.0.0.5 | `adb connect 10.0.0.5:5555` | Prayer Display `2.1-firetv`. Its own time zone is set to Winnipeg (one hour ahead in summer): change on the TV. |
| Fire TV Stick 4K Select (AFTCA002) | 10.0.0.230 | none (Vega OS: no ADB, no APKs) | Nothing of ours; see `Android_TV_app/vega/README.md`. |
| Roku TV 43" | 10.0.0.102 | developer installer at `http://10.0.0.102` (user `rokudev`, the owner's password) | Test channel: STOS splash, then the older display. |

## Prayer display: how it fits together

- **display_template** is the app: `player.html` rotates prayer (10 s), notices (6 s), prayer + notices (6 s);
  `manage.html` is the settings page; `serve.py` keeps settings and images on the device (`/api/config`, `/api/media`).
- **Pi (ScholarTechOS prayer image):** `package/scholartech-prayer` installs `apps/prayer` (a copy of display_template,
  refreshed by `scripts/update-prayer-app.sh`). Boot: mosque splash (mpv) → cog in cage showing `player.html`.
  Updates: `prayer-update` (hourly timer) installs the newest `vX.Y` tag of display_template using a read-only deploy
  key, checks it, rolls back if the server does not start. Set up each screen with `scripts/provision.sh <address>`.
- **Android TV / Fire TV:** the app serves display_template from its assets on port 8080 (`/tv`, `/settings`,
  `/manage.html`). Copy updates in with `tools/update_stos_app.sh`. Opens about 35 s after power-on on a Fire TV.
- **Roku:** cannot run display_template (no browser). Plan and options in `Android_TV_app/roku/PLAN.md`; the user is
  leaning towards a phone app that sends settings to the Roku. On hold.
- **Vega Fire TV sticks:** Appstore-only; plan in `Android_TV_app/vega/README.md`. On hold.

## Open items

1. **GitHub Actions stopped**: the account ran out of free minutes for private repos ("payments failed or spending
   limit"). Check https://github.com/settings/billing. Options: make ScholarTechOS public (free builds), or build
   locally with Docker (`./build.sh rpi4`; needs about 80–100 GB for Docker).
2. **Pi prayer image**: GitHub run 38328302909 (rpi4, prayer) was still running at handover; the first attempt
   timed out at 97% of WebKit. When an image exists: flash it, `scripts/provision.sh <address> America/Regina`,
   then tag a display_template release (`v1.1`) and check the Pi installs it by itself.
3. **Release display_template `v1.1`** (splash page, lighter player, stepped logo zoom) once the Pi image can update.
4. **Pi 4 drawing is slow** (about 1 frame a second in WebKit); the v1.1 changes should help. Fonts fall back to
   defaults on the Pi (WPE may not read woff2: add .ttf copies).
5. **Time-zone choice in display settings**, so a screen does not depend on the device's own time-zone setting.
6. **Roku and Vega**: decide where their settings come from (phone app or online).
7. **Clean-up in Android_TV_app**: the old display and settings pages are still in the code but no longer linked;
   one Diagnostics check still reads the old location setting.
8. ScholarTechOS `TODO.md`: Wi-Fi setup hotspot test, A/B updates and more.

## Files that live only on the original Mac (copy them, privately)

- `~/.ssh/id_ed25519` (+ `.pub`): logs in to the Pi.
- `~/.ssh/scholartech_prayer_deploy`: read-only key the Pi screens use to download display_template releases.
- `~/.android/scholarium-release.jks` and `~/.android/scholarium-release.properties`: signs the TV apps. Without
  it, installed apps cannot be updated; keep a backup.

## Ways of working the owner asked for

- Test on the real devices before any long build; never start a ScholarTechOS build without asking (a push to
  `main` starts one: use `[skip ci]` when it should not).
- Do not push EZ_ads. Keep Roku, Fire TV and Pi topics separate.
- Owner handles passwords, factory resets and device settings (time zone and the like) themselves.
