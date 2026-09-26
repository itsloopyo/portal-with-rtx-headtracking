# Portal with RTX Head Tracking

![Portal with RTX running with this mod](https://raw.githubusercontent.com/itsloopyo/portal-with-rtx-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Portal with RTX that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; the portal gun still aims with your mouse or controller
- **6DOF positional tracking** - lean in and peek around corners with head position
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Portal with RTX](https://store.steampowered.com/app/2012840/Portal_with_RTX/) on Steam.
- A tracking source that sends the OpenTrack UDP protocol: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam or a VR headset, or a phone app that speaks it.
- Windows 10 or 11. Portal with RTX runs the 32-bit Source engine, and the mod ships as a 32-bit `.asi`.

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Portal with RTX**, and click
**Play with head tracking**.

### Standalone Installer

1. Download the installer ZIP from the [Releases](https://github.com/itsloopyo/portal-with-rtx-headtracking/releases) page.
2. Extract it anywhere.
3. Double-click `install.cmd`. It finds the game, installs the ASI loader and copies the mod in.
4. Configure OpenTrack (or your phone app) to output UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your game, tell it where the game is. Either set the environment variable:

```powershell
$env:PORTAL_WITH_RTX_PATH = "D:\Games\Steam\steamapps\common\PortalRTX"
```

or pass the folder holding `hl2.exe` as an argument:

```powershell
install.cmd "D:\Games\Steam\steamapps\common\PortalRTX"
```

### Manual Installation

The payload goes into the game's `bin` folder, not the folder holding `hl2.exe`. Source loads `bin\launcher.dll` with an altered search path, so `bin` is where the proxy DLL and everything below it are searched for. A copy next to `hl2.exe` is never loaded.

1. Copy `vendor\ultimate-asi-loader\dinput8.dll` from the installer ZIP to `<game>\bin\winmm.dll`. Skip this if you already have an ASI loader there.
2. Copy `plugins\PortalWithRTXHeadTracking.asi` to `<game>\bin\`.

The Nexus ZIP is already in this shape: extract it over the game folder and it drops `bin\PortalWithRTXHeadTracking.asi` into place. It does not bundle a loader, so take `dinput8.dll` from the installer ZIP on the Releases page, or from an Ultimate ASI Loader release of your own, and put it at `<game>\bin\winmm.dll` as in step 1.

On first launch the mod writes `CameraUnlock.ini` next to `hl2.exe`, in the game's root folder.

## Setting Up OpenTrack

In OpenTrack, set **Output** to `UDP over network`, open its options and set the address to `127.0.0.1` and the port to `4242`. Pick an **Input** below, then press Start.

Centering is done in the tracker. Use OpenTrack's Center bind, your phone app's CENTER button, or SteamVR's reset view, and the mod follows.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR and let it see the headset.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Webcam Setup

1. In OpenTrack, set **Input** to `neuralnet tracker`. It tracks your face from a plain webcam, with no markers, clips or IR hardware.
2. Open its options and pick your camera, resolution and frame rate.
3. Leave **Output** on `UDP over network`, `127.0.0.1:4242`.

### Phone App Setup

The mod accepts one thing: the OpenTrack UDP protocol on port `4242`. A phone tracker is usable here if it sends that protocol itself, or ships a PC-side companion that does. Check your app against that before anything else.

For an app that does send it, what decides the wiring is how much filtering it does on the phone. An app that filters on-device can point straight at your PC's LAN IP on port `4242`. A raw or lightly filtered feed sent direct will jitter, because the mod's smoothing is sized to take the edge off a clean signal rather than to rescue a noisy one, and an app like that should send to OpenTrack instead so its filters and curves can clean the feed up first.

The test is quicker than the theory: send direct, hold your head still, and if the view drifts or shakes, route it through OpenTrack.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody with a phone already in their pocket. It filters on-device, so it can send direct. Any app that filters enough noise works the same way.

A phone on WiFi is a remote connection and gets `RemoteSmoothing`. So does a tracker running on this same PC if it sends to your LAN address instead of `127.0.0.1`, because the mod classifies the transport, not the machine. Send to `127.0.0.1` to get `LocalSmoothing`.

## Controls

Each action has a list of keys, and any key in it fires the action. By default
each list holds a nav-cluster key and a chord, so use whichever your keyboard has.

| Action | Default keys | Setting |
|--------|--------------|---------|
| Toggle head tracking | `End`, `Ctrl+Shift+Y` | `ToggleKey` |
| Cycle tracking mode (full, rotation only, position only) | `PageUp`, `Ctrl+Shift+G` | `CycleTrackingModeKey` |
| Toggle yaw mode (world-locked, camera-local) | `PageDown`, `Ctrl+Shift+H` | `YawModeKey` |

The tracking mode and the yaw mode are saved to `CameraUnlock.ini` the moment
they change, so the next launch starts in the mode you left it in. `End` turns
tracking on and off for the session only and saves nothing: whether tracking is
on at launch is `EnableOnStartup`.

Every key in the three lists, the chords included, can be changed or removed
under `[Hotkeys]` in `CameraUnlock.ini`, for example `ToggleKey=F8, Ctrl+Shift+Y`.

## Configuration

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Portal with RTX head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default

[View]
; Field of view in degrees, as the game's fov_desired: horizontal, at 4:3, and the mod
; widens it for your screen as the game does. 0 leaves the game's own. Otherwise 30 to
; 150, which fov_desired's own 75 to 120 does not bound. Applies only while head
; tracking is on.
Fov=0.0
; Field of view the gun in your hands is drawn with, in the same degrees. A wider Fov
; leaves the gun looking oversized: lower this to shrink it. 0 leaves the game's own.
FovViewmodel=0.0

[Debug]
; true: write HeadTracking.log beside hl2.exe, new at every launch, with the launch before
; kept as HeadTracking.prev.log. It records the build profile, the tracker connection and
; the view the mod draws. Attach it to a bug report.
LogToFile=true
```
<!-- /cameraunlock:config -->

The game has its own field of view control: **Options > Video > Advanced**, where
the slider runs from 75 to 90. The console cvar behind it, `fov_desired`, takes
75 to 120. The `[View] Fov` setting is for going outside that: it is written
into the view the frame is rendered from rather than into the cvar, so it accepts
30 to 150 and does not travel in your userinfo. `FovViewmodel` has no in-game
control at all, because `viewmodel_fov` is a cheat cvar. Use the game's slider if
90 is wide enough for you.

Whichever you use, head tracking moves the view by the same amount on screen:
the pose is scaled for the field of view the frame is actually being rendered at,
so a wider setting does not make tracking feel stronger.

The settings are read once at startup, so restart the game after editing the
file.

## Troubleshooting

**Mod not loading**

- Check that both `winmm.dll` and `PortalWithRTXHeadTracking.asi` are in the game's `bin` folder. A copy beside `hl2.exe` is never loaded.
- Check that `HeadTracking.log` appears next to `hl2.exe` after a launch. No log file at all means either `LogToFile=false` under `[Debug]` in `CameraUnlock.ini`, or the loader never ran the mod - check the file first.
- If the log says no build profile matches this `client.dll`, the game has been patched to a build this mod does not know yet. The mod stays dormant and the game runs vanilla, so check the Releases page for an update.

**No tracking response**

- Confirm the tracker is sending to `127.0.0.1:4242`, or to your PC's LAN IP on port `4242` from a phone, and that `UdpPort` in `CameraUnlock.ini` matches.
- Press `End` (or `Ctrl+Shift+Y`). Tracking may be toggled off, and `EnableOnStartup=false` in `CameraUnlock.ini` starts it off.
- Tracking is suppressed outside gameplay, so the menu backdrop, the pause menu and loading screens do not move. Load a save and try there.
- If the tracker is on another device, allow the game through the Windows firewall on the private network.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` in `CameraUnlock.ini` for a phone or a device over WiFi. It sets how long the view takes to settle: the `0.15` default is a 23.5ms time constant, `0.3` is 28.5ms, and the scale runs to 10s at `1.0`. Small steps do very little, so move it in large ones.
- If your tracker sends a raw feed, route it through OpenTrack and use its filters rather than leaning on the mod's smoothing.
- A webcam feed that jitters usually wants more light and a higher camera frame rate before it wants more smoothing.

**Wrong rotation or lean axis**

- Axis direction is fixed in the tracker, not here. The mod applies the pose it is sent at 1:1, so an axis that moves the wrong way is inverted in OpenTrack's mapping or in your phone app's settings, and fixing it there keeps every game consistent.
- If the view swings oddly around the horizon while you look straight up or down, press `Page Down` (or `Ctrl+Shift+H`) to switch between world-locked and camera-local yaw.

## Updating

Download the new release and run `install.cmd` again. Your settings in `CameraUnlock.ini` are kept, and the installer does not touch `HeadTracking.ini`.

## Uninstalling

Run `uninstall.cmd`. This removes the mod and its logs from the game folder, and leaves `CameraUnlock.ini` and `HeadTracking.ini` in place, so a reinstall keeps your settings. The ASI loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway.

## Building from Source

Requires Visual Studio 2022 with the C++ desktop workload (32-bit toolset), CMake, and [pixi](https://pixi.sh).

```powershell
git clone --recurse-submodules https://github.com/itsloopyo/portal-with-rtx-headtracking.git
cd portal-with-rtx-headtracking
pixi run build-release
pixi run test
pixi run package
```

`pixi run package` writes the installer and Nexus ZIPs to `release/`.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- Valve, for Portal and the Source engine.
- NVIDIA Lightspeed Studios, for Portal with RTX and RTX Remix.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG, which loads the mod.
- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu, statically linked for function hooking.
- [OpenTrack](https://github.com/opentrack/opentrack), whose UDP protocol the mod speaks.
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core), the shared head tracking library.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Valve or NVIDIA. Use at your own risk.
