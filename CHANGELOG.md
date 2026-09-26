# Changelog

## [Unreleased]

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed

- Settings move to `CameraUnlock.ini`. Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
  - Reticle settings, and a key that toggled the reticle.
  - The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The chords were fixed in code before; now they can be changed or removed like any other key.
- The tracking mode (`PageUp`) and the yaw mode (`PageDown`) are saved to `CameraUnlock.ini` when they change, so the next launch starts in the mode you left. `End` still changes the session only.
- The old keys take their canonical names: `[Network] Port` becomes `UdpPort`; `EnableOnStartup` moves to `[General]`, and so does `WorldSpaceYaw` from `[View]`; `[Position] Enabled`, which chose the tracking mode at startup, becomes `RotationEnabled` and `PositionEnabled`; `Toggle`, `ModeCycle` and `YawMode` become `ToggleKey`, `CycleTrackingModeKey` and `YawModeKey`. The old single `LimitY` becomes both `PositionLimitY` and `PositionLimitYDown`, which it already set, and each can now be set on its own. `LimitX`, `LimitZ` and `LimitZBack` become `PositionLimitX`, `PositionLimitZ` and `PositionLimitZBack`. `[View] Fov`, `[View] FovViewmodel` and `[Debug] LogToFile` keep their names.
- The lean limits take 0 to 10 metres, where earlier versions refused a limit outside 0.01 to 0.5 and used the default. A limit an earlier version refused imports as the default it ran on.
- The HeadTracking.ini reader is unchanged since v0.1.0, and so is how the mod starts from what it read, so apart from the changes listed here every setting you had carries over as it was.

### Removed

- The rotation sensitivity (`[Sensitivity] Yaw`, `Pitch`, `Roll`) and position sensitivity (`[Position] SensX`, `SensY`, `SensZ`) settings. Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did before.

## [0.1.0] - 2026-09-05

First release.

## [0.0.0] - 2026-09-05

### Added
- Initial release.
- Decoupled 6DOF head tracking for Portal with RTX. The head moves the view while the mouse keeps control of aim, driven by a webcam, phone, or any OpenTrack compatible tracker over UDP.
- Crosshair compensation so the reticle is drawn on the point the portal gun is actually aimed at while the head is turned or leaned.
- `HeadTracking.ini` next to `hl2.exe`, written with defaults and comments on first launch. Covers port, startup state, per-axis sensitivity, local and remote smoothing, 6DOF position scale and limits, world-locked or camera-local yaw, and an optional FOV override. Every sensitivity and lean limit is range-checked, and the accepted band is written next to the key it applies to.
- Hotkeys: `End` / `Ctrl+Shift+Y` toggles tracking, `Page Up` / `Ctrl+Shift+G` cycles full, rotation only and position only, `Page Down` / `Ctrl+Shift+H` switches between world-locked and camera-local yaw.
- The view holds the last pose when the tracker goes quiet, so a webcam that loses your face for a moment does not snap the view to centre and back.
- The view pitch saturates just short of vertical, so head movement on top of a steep aim cannot tip the frame past the top.
- Tracking suppressed outside gameplay, so menus and loading screens are unaffected.
- Per-build PE fingerprinting. On a game build the mod does not recognise it stays fully dormant, installs no hooks, and says so in `HeadTracking.log`.
- `install.cmd` and `uninstall.cmd`, deploying the bundled Ultimate ASI Loader and the mod into the game's `bin` directory.
- `launcher-manifest.json`, so the package installs, updates, verifies and uninstalls through the launcher rather than only by hand.

### Notes
- Head tracking moves the view by the same amount on screen whatever field of view the frame is rendered at, so the game's own FOV slider and the `[View] Fov` key change how much you can see rather than how far your head moves the view.
- Axis direction, deadzones and response curves belong to the tracker. The mod consumes the pose at 1:1 and converts to the engine's axes once, internally, so one tracker profile behaves the same in every game.
- The vendored Ultimate ASI Loader is stripped of the third-party DLLs the upstream 32-bit build embeds as resources (`binkw32.dll`, `wndmode.dll`, `vorbisfile.dll`), none of which are ours to redistribute. Only the `.rsrc` section differs from upstream; the loader's code, imports, relocations and appended PDB are byte-identical, and `pixi run package` refuses to build a ZIP from a loader that still carries them.
