# Roku Remote 3DS — Version 2 features, app v1.3

Version 2 names the feature update. The application stays **v1.3 / 1.3.0**, title ID **000400000F8D3A00**. The Original remote, SD paths, 3D banner and square icon are retained. This directory contains the companion, a buildable example theme, and integration checks. It does not turn the older binary at the top of the main branch into Version 2.

## Using the computer companion

1. Connect the 3DS, computer, and Roku to the same local network. Use Setup → Add IP on the remote to select the **TV**. The companion address below is the **computer**.
2. Install desktop Python 3.11 or 3.12. On Windows, open `companion/start-windows.cmd`. On macOS/Linux use the commands below. The first setup downloads PocketSphinx, its English model, and Pillow. There is no cloud speech account or transcription fee.
3. On the 3DS open **Setup → Version 2 → Companion connection**. Enter the **http:// address with port** and the pairing code printed on the PC. Tap **Test connection**. If the PC prompts for firewall permission, allow it on your private home network. Leave the console window open.
4. To dictate, focus a text box on the TV. Choose **Keyboard and dictation → Dictate with 3DS mic → Start recording**. Speak, then tap **Stop and transcribe**. Correct the text on the 3DS; **OK** sends characters to the focused Roku keyboard. Cancel discards them. Recording ends at 20 seconds. Text input itself works without a computer.
5. **Theme marketplace → Browse / refresh catalog** shows that server's themes. Select a theme for its screenshot, creator, category, description, and download. Choose **Share a theme ZIP** to upload your ZIP after confirming. The catalog is hosted by the computer you select; there is no centrally operated public marketplace.

```sh
cd companion
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python companion.py --lan
```

Without `--lan`, the server listens only on the computer. Recordings are processed in memory. On a trusted home LAN the default connection is plain HTTP; do not expose it to the internet. An HTTPS server may be configured with a trusted CA at `/3ds/RokuRemote3DS/cacert.pem`. The pairing code is stored on both devices. Themes and the code persist under `~/.roku-remote-3ds/market/` on the computer.

**Microphone limit:** the 3DS microphone can dictate text into a Roku field that accepts ECP literal keypresses. It cannot serve as a live microphone for a Roku app, website, or YouTube's voice-search button. Roku documents a separate microphone interface for its supported voice remotes and mobile app. See [ECP](https://developer.roku.com/dev/docs/external-control-api) and [roMicrophone](https://developer.roku.com/dev/docs/romicrophone).

## What's retained or added

| Version | Changes |
| --- | --- |
| v1.2 (corrected public label) | Wi-Fi ECP controls, touchscreen buttons and physical keys, volume, mute, replay, options and playback; Auto/Manual Add IP and saved address; device information, connection test, discovery fallback, help and network diagnostics; installed-app picker and named per-TV shortcuts. Earlier internal labels used v1.3.0. |
| v1.2.1 | Socket readiness checked in short slices against one overall deadline, including Auto's device checks. More useful phase and byte-count logging. Native tilted 3D remote-only banner, supplied icon and model attribution in Help. |
| v1.2.2 | Experimental direct Roku audio to wired 3DS headphones, SD DSP dump support, and a square, larger icon. |
| v1.3 | Apps and Roku TV inputs including HDMI share the picker and shortcut strip; Recent keeps the last 12 successful launches per TV; emulator HLE headphone setup and explicit sound errors. |
| Version 2 — app number unchanged | Keyboard and reviewed dictation; swipe/tap touchpad; live drag/resize/layout editor with editable colors and independent transparency; named ordered macros with pauses and confirmation of each command; ZIP button sounds, assignments and previews; calibrated tilt; configurable and previewable screensavers; independently saved remote profiles; local theme marketplace with previews, downloads and sharing; original/dark/light palettes, custom PNG/JPEG/GIF/WebP images and animations with speed controls. |

## Using the 3DS tools

- **Touchpad:** tap the pad for OK, or swipe to send a direction. Long swipes can repeat. Press B to return.
- **Layout editor:** select a button, drag to move it, or switch to **Resize** and drag. **Style** controls its action, name, color, icon and opacity. Each background, border and icon/text has a live slider: 0% transparency is solid; 100% is invisible. **Other element transparency** covers screen art, title, panel, status, accent and shortcuts. Buttons occupy the upper 190 pixels so the existing shortcut strip remains accessible. **Reset layout** restores the Original arrangement for the active profile.
- **Macro buttons:** create a named macro, add keys, app/input launches and timed pauses, then reorder or remove steps. **Add this button to layout** places it on the remote. The queue waits for the TV to acknowledge each command and stops on an error; it does not retry a possibly executed command.
- **Motion and screensaver:** hold the console comfortably, enable tilt, and calibrate. Tilt to navigate on the remote, tilt more deeply forward to select, then return to neutral. Choose an idle interval, screensaver artwork, and animation settings. A wake-up press is consumed.
- **Remote profiles:** make a blank profile or duplicate the current one. Each stores its own buttons, macros, theme and settings. The selected TV, its shortcuts and Recent continue to use the original v1.3 files. New profile data lives in `/3ds/RokuRemote3DS/version2.json`, with a backup.
- **Theme:** select Original, Dark or Light; or import a ZIP from your SD card. See [how to make a theme](THEMES.md). The unaided default remains the original purple remote.
- **Home Menu credit:** “Roku remote” by [doorbin (woodland.mid)](https://sketchfab.com/woodland.mid), [original model](https://sketchfab.com/3d-models/roku-remote-c8d8e557d6c94d48826eef4bb403d8f9), CC BY 4.0. The adapted native model, tilt and icon come from the existing v1.3 source.

