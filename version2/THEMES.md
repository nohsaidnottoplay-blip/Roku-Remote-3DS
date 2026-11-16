# Make a Version 2 theme

1. Create a folder such as **MyTheme** on your computer. Place a plain UTF-8 file named **theme.json** in it. Start with the manifest below.
2. Add the art, sounds and screenshot next to it, or in relative subfolders such as `art/` and `sounds/`. File names in the manifest must match. Give every button that needs its own sound an entry in `sounds`.
3. ZIP the **contents** of MyTheme. A single enclosing folder is also supported. Use normal ZIP (stored or deflated), not RAR/7z and not an encrypted ZIP. Use just one manifest. Paths must be relative. Do not put scripts in a theme.
4. Copy the ZIP, still compressed, to **/3ds/RokuRemote3DS/themes/** on the 3DS SD card. Open **Setup → Theme → Import custom theme ZIP** and tap its filename.
5. Preview assets in **Images and animations**, **Button sounds** and **Screensaver**. Edit a button to assign different artwork. Use **Theme marketplace → Share a theme ZIP** to share the complete original ZIP with a paired server.

```json
{
  "format": 2,
  "name": "My Tide",
  "creator": "Your name",
  "category": "Nature",
  "description": "A blue remote with a moving tide and a soft click.",
  "base": "Dark",
  "colors": {"top":"102A37","button":"234957","button_text":"F5FAEF"},
  "images": {
    "top": "art/top.png", "bottom": "art/bottom.jpg",
    "Select": "buttons/ok.gif", "Home": "buttons/home.webp",
    "screensaver": "art/tide.webp", "screenshot": "screenshot.png"
  },
  "sounds": {"Home":"sounds/home.wav","Back":"sounds/back.wav","Select":"sounds/ok.wav","Directional":"sounds/tap.wav","Volume":"sounds/tap.wav","Power":"sounds/back.wav"}
}
```

| Asset | Accepted format and behavior |
| --- | --- |
| Color | Six RGB hex digits, with or without #. Bases: Original, Dark, Light. Roles: top, bottom, accent, text, muted, highlight, panel, border, selected, button, button_text. |
| Button, top or bottom artwork | PNG, JPG/JPEG, GIF, WebP. GIF and animated WebP keep their frame timing. PNG/JPEG render as stills. Button and screensaver art keep their proportions; screen backgrounds fill the screen. |
| Screensaver | Set `images.screensaver` to a still or animation. You can preview it, turn it off, choose a different image and set the idle delay. Without a usable image, the built-in screensaver remains available. |
| Marketplace screenshot | Set `images.screenshot` to an image, or include `screenshot.png`. If no readable screenshot exists, the server shows a labelled palette preview. Include an actual screenshot for the most accurate preview. |
| Button sound | WAV with uncompressed signed 16-bit PCM, mono or stereo, 8–48 kHz, up to five seconds. MP3, AAC, ADPCM and float WAV are not button sounds. You can preview, override or disable sounds after import. |

IDs include **Home, Back, Select** (OK), **Up, Down, Left, Right, Play, Rev, Fwd, Info, InstantReplay, VolumeUp, VolumeDown, VolumeMute, Power, PowerOn, PowerOff, Backspace, Enter, ChannelUp, ChannelDown, InputTuner, InputHDMI1–4, InputAV1**. `Directional`, `Volume` and `Power` are group fallbacks. A named per-key asset wins over a group default. Media can also be detected by basename, for example `sounds/Home.wav` or `art/screensaver.gif`. Missing optional art uses the label/palette; missing sounds are silent.

**Console budgets:** up to 8 MiB per ZIP and 128 archive entries. A manifest is limited to 16 KiB. Keep each optional file under 4 MiB. Media images: up to 512×256 pixels, up to 60 frames; total decoded media up to 8 MiB (RGBA uses width × height × 4 bytes per frame). Typical sizes are 60×30 for icons and 160×96 for an animated screensaver. A legacy `theme.ini` can use `[theme]`, `[colors]`, `[images]`, and `[sounds]` sections in place of JSON.

To generate the complete **Tidepool** example, install Pillow and run `python examples/build_tidepool.py companion/themes/Tidepool-Version2.zip` from this directory. It includes all four image formats, six frames of screensaver animation, and four WAV files.
