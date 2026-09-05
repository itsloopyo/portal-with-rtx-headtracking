# Portal with RTX Head Tracking

![Portal with RTX running with this mod](https://raw.githubusercontent.com/itsloopyo/portal-with-rtx-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Portal with RTX that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; the portal gun still aims with your mouse or controller
- **6DOF positional tracking** - lean in and peek around corners with head position
- **Works with any OpenTrack-compatible source** - webcam, phone app, or anything else that sends the OpenTrack UDP protocol

## Requirements

- [Portal with RTX](https://store.steampowered.com/app/2012840/Portal_with_RTX/) on Steam.
- A tracking source that sends the OpenTrack UDP protocol: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam or a VR headset, or a phone app that speaks it.
- Windows 10 or 11. Portal with RTX runs the 32-bit Source engine, and the mod ships as a 32-bit `.asi`.

## Installation

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

On first launch the mod writes `HeadTracking.ini` next to `hl2.exe`, in the game's root folder.

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

Both columns do the same thing. Use whichever your keyboard has.

| Action | Nav cluster | Chord |
|--------|-------------|-------|
| Toggle head tracking | `End` | `Ctrl+Shift+Y` |
| Cycle tracking mode (full, rotation only, position only) | `Page Up` | `Ctrl+Shift+G` |
| Toggle yaw mode (world-locked, camera-local) | `Page Down` | `Ctrl+Shift+H` |

## Configuration

`HeadTracking.ini` sits in the game's root folder, next to `hl2.exe`. It is written with these defaults on first launch, and read again each time the game starts.

The game has its own field of view control: **Options > Video > Advanced**, where
the slider runs from 75 to 90. The console cvar behind it, `fov_desired`, takes
75 to 120. The `[View] Fov` key below is for going outside that: it is written
into the view the frame is rendered from rather than into the cvar, so it accepts
30 to 150 and does not travel in your userinfo. `FovViewmodel` has no in-game
control at all, because `viewmodel_fov` is a cheat cvar. Use the game's slider if
90 is wide enough for you.

Whichever you use, head tracking moves the view by the same amount on screen:
the pose is scaled for the field of view the frame is actually being rendered at,
so a wider setting does not make tracking feel stronger.

```ini
; Portal with RTX head tracking - default config

[Network]
Port=4242
EnableOnStartup=1

[Sensitivity]
; Scales the tracker's rotation before it reaches the view. 1 is 1:1,
; and the accepted range is 0.1 to 3. A value outside it is refused and
; noted in the log. Shape the pose in your tracker app instead where you
; can - a profile there behaves the same in every game.
Yaw=1
Pitch=1
Roll=1

[Smoothing]
; Picked per connection from the tracker's source address, and applied
; to both rotation and position. 0 = no smoothing, 1 = heavy.
; LocalSmoothing: tracker runs on this machine (loopback)
LocalSmoothing=0
; RemoteSmoothing: tracker is a remote device on the network
RemoteSmoothing=0.15

[Position]
; 6DOF head position, applied to the render view origin only
Enabled=1
; Scales head travel before the limits below, so the envelope keeps
; meaning metres. 1 is 1:1 with your real head movement, and the
; accepted range is 0 to 5.
SensX=1
SensY=1
SensZ=1
; Movement envelope in metres, each accepted from 0.01 to 0.5. Z is
; asymmetric on purpose: leaning in gets more room than pulling back,
; which would clip the player model.
LimitX=0.3
LimitY=0.2
LimitZ=0.4
LimitZBack=0.1

[Hotkeys]
Toggle=0x23
YawMode=0x22
; Page Up: cycle 6DOF -> rotation-only -> position-only
ModeCycle=0x21

[View]
; true = horizon-locked yaw (default), false = camera-local yaw
WorldSpaceYaw=1
; Field of view, same units as the game's fov_desired cvar (horizontal
; degrees at 4:3; the mod widens it for your real aspect ratio as the
; engine does). Written into the render view rather than the cvar, so it
; is not bound by fov_desired's own range. Accepted from 30 to 150, or
; 0 to leave the game's FOV alone. Applies only while tracking is
; enabled (End).
Fov=0
; The weapon is drawn with its own FOV. Widening Fov leaves the gun
; looking oversized against the wider world: LOWER this to shrink it.
; 0 = leave the game's viewmodel FOV alone.
FovViewmodel=0

[Debug]
; Writes HeadTracking.log next to hl2.exe, fresh every launch (the
; previous session is kept as HeadTracking.prev.log, and nothing else). It
; records the build profile, the tracker connection and the pose being
; applied. That is the file to attach to a bug report - leave it on.
LogToFile=1
```

## Troubleshooting

**Mod not loading**

- Check that both `winmm.dll` and `PortalWithRTXHeadTracking.asi` are in the game's `bin` folder. A copy beside `hl2.exe` is never loaded.
- Check that `HeadTracking.log` appears next to `hl2.exe` after a launch. No log file at all means either `LogToFile=0` in the ini, or the loader never ran the mod - check the ini first.
- If the log says no build profile matches this `client.dll`, the game has been patched to a build this mod does not know yet. The mod stays dormant and the game runs vanilla, so check the Releases page for an update.

**No tracking response**

- Confirm the tracker is sending to `127.0.0.1:4242`, or to your PC's LAN IP on port `4242` from a phone, and that `Port` in the ini matches.
- Press `End` (or `Ctrl+Shift+Y`). Tracking may be toggled off, and `EnableOnStartup=0` in the ini starts it off.
- Tracking is suppressed outside gameplay, so the menu backdrop, the pause menu and loading screens do not move. Load a save and try there.
- If the tracker is on another device, allow the game through the Windows firewall on the private network.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` for a phone or a device over WiFi. It sets how long the view takes to settle: the `0.15` default is a 23.5ms time constant, `0.3` is 28.5ms, and the scale runs to 10s at `1.0`. Small steps do very little, so move it in large ones.
- If your tracker sends a raw feed, route it through OpenTrack and use its filters rather than leaning on the mod's smoothing.
- A webcam feed that jitters usually wants more light and a higher camera frame rate before it wants more smoothing.

**Wrong rotation or lean axis**

- Axis direction is fixed in the tracker, not here. The mod applies the pose it is sent at 1:1, so an axis that moves the wrong way is inverted in OpenTrack's mapping or in your phone app's settings, and fixing it there keeps every game consistent.
- If the view swings oddly around the horizon while you look straight up or down, press `Page Down` (or `Ctrl+Shift+H`) to switch between world-locked and camera-local yaw.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod, and `HeadTracking.ini` and its logs from the game folder, so back the ini up first if you want to keep your tuning. The ASI loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway.

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
