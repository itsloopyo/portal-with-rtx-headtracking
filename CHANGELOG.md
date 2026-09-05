# Changelog

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
