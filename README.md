# Roku Remote 3DS

A personal Nintendo 3DS homebrew remote for a Roku device on your home network.
The video stays on your TV. The 3DS acts as the remote.
There was help from ChatGPT 6 Astra. Help can also be found in the application itself.

## Install the ready-built app

1. Your 3DS must already have homebrew access. This does not modify the console.
2. Turn off the 3DS. Copy the **RokuRemote3DS** folder to the root of your SD card 3ds folder.
3. Put the SD card back, turn on the 3DS, and open **Homebrew Launcher**.
4. Open **Roku Remote 3DS**.

You could also downlaod the **.cia** file to your 3DS home screen through **FBI**.

The installed files should be:

```
SD card/
  3ds/
    RokuRemote3DS/
      RokuRemote3DS.3dsx
      RokuRemote3DS.smdh
```

Physical testing on a 3DS and Roku is completed and the connection is successful.

## Connect your Roku

1. Turn on your Roku. Connect it and your 3DS to the same home network.
   They can use different Wi-Fi bands if the router allows them to communicate.
   Avoid isolated guest networks.
2. On the Roku, find **Settings > System > Advanced system settings >
   Control by mobile apps** and set it to **Enabled**, when that option exists.
   Older versions may show **Network access > Default** instead.
3. Find the Roku's IP under **Settings > Network > About**.
4. In the 3DS app, choose **Enter Roku IP** and enter that address.
5. Choose **Test connection**, then **Back to remote**. Try **Home**.

The example IP in the screen preview is fictional; enter your own Roku's IP.
The app saves only that IP at `sdmc:/3ds/RokuRemote3DS/roku_ip.txt`.
No account, password, subscription, cloud service, or PC server is needed.

## Controls

| 3DS button | Roku action |
| --- | --- |
| D-pad | Move selection |
| A | OK / Select |
| B | Back |
| X | Roku Home |
| Y | Play / Pause |
| L / R | Volume down / up, when supported |
| SELECT | Open or close setup |
| START | Exit to Homebrew Launcher |

The touchscreen also has Options, Rewind, Fast-forward, Replay, and Mute.
Hold a D-pad direction or shoulder button to repeat it. Touch buttons activate
once per tap. The Nintendo HOME button remains a system button.

## Compatibility and troubleshooting

This is an independent homebrew experiment, not an official Roku app.
Roku's current ECP documentation says third-party platforms may not send ECP
commands. This project uses those documented commands, but compatibility or
permission to use them on your device should not be assumed. It does not
work around Roku restrictions.

- **Roku found** means the device-info endpoint responded. It does not prove
  that remote commands are enabled. Try a button next.
- **Roku denied control**: check the mobile-app control setting. If it is already
  enabled, your Roku software may restrict third-party remotes.
- **No reply / Cannot reach Roku**: check the IP, turn on the Roku, and verify
  both devices are on the same local network. An IP can change after a restart.
- **Unexpected response**: check that you entered the Roku's IP, not your router's.
- **Volume does nothing**: volume requires support from the Roku device and TV
  setup. It is not guaranteed for streaming sticks or boxes.
- **Could not save to SD**: the entered IP still works for the current session.
  Check that the SD card is writable before restarting.

The worker keeps the interface responsive while a request waits up to 2.2
seconds. While a request is pending, extra taps are ignored so commands do not
pile up. Requests are never automatically retried. If a timeout occurs after
a command was sent, the app cannot know whether the Roku performed that action.

## Included source and rebuilding

The **source-project** folder contains the complete C source, Makefile, and tests.
The prebuilt app does not require any of these tools on your computer.

To rebuild, install devkitPro's **3DS development** tools (`3ds-dev`), including
devkitARM, libctru, and the 3DS tools. In a devkitPro shell:

```
cd source-project
make
```

On Windows, run those commands inside the devkitPro MSYS2 shell.

The result is `RokuRemote3DS.3dsx` and `RokuRemote3DS.smdh`.
Linux host tests require GCC and Python 3: `python3 tests/test_ecp.py`.

## Validation

- Compiled and linked with the official devkitPro devkitARM toolchain.
- Tested the actual C socket client against a local simulated Roku: every remote
  key, fragmented HTTP replies, device identification, denied commands, malformed
  responses, invalid IPs, timeouts, and absence of automatic retries.
- Rendered and visually inspected the actual drawing code for Remote, Setup,
  and Help screens. The included preview is a host render, not a device screenshot.
- Not tested on physical Nintendo 3DS or Roku hardware.

## References

- [Roku ECP documentation](https://developer.roku.com/docs/developer-program/dev-tools/external-control-api.md)
- [devkitPro libctru](https://github.com/devkitPro/libctru)
- [devkitPro setup](https://devkitpro.org/wiki/Getting_Started)

Roku and Nintendo trademarks belong to their respective owners.
