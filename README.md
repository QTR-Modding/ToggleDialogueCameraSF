# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, press the controller's View button (the small button with two overlapping rectangles) to switch views. When Starfield's Dialogue Camera setting is enabled, the gamepad cycles Dialogue Camera → Near Third Person → Far Third Person → Dialogue Camera. When Dialogue Camera is disabled, it cycles First Person → Near Third Person → Far Third Person → First Person.

The mouse wheel retains Starfield's directional camera steps. Scroll out through the two third-person distances and back in one step at a time. The inward endpoint is the dialogue camera when that setting is enabled and first person when it is disabled.

Scrolling farther inward at the dialogue-camera or first-person end, or farther outward at the
far-third-person end leaves the current view unchanged.

## Requirements

- Starfield 1.16.244
- SFSE
- Address Library for SFSE Plugins

## Install

Copy `ToggleDialogueCameraSF.dll` into `Data\SFSE\Plugins`.

## Building

Clone recursively so the pinned CommonLibSF, commonlib-shared, and spdlog
sources are present:

```powershell
git clone --recursive https://github.com/QTR-Modding/ToggleDialogueCameraSF.git
cd ToggleDialogueCameraSF
xmake f -m release -a x64 -y
xmake -y
```

The build requires Xmake 3.0.9 or newer, MSVC with C++23 support, and the
Windows SDK. Exact dependency revisions and corresponding-source details are
listed in [SOURCE.md](SOURCE.md).

## License

Toggle Dialogue Camera SF is licensed under GPL-3.0-or-later WITH the Modding
Exception AND GPL-3.0 Linking Exception (with Corresponding Source). See
[COPYING](COPYING) and [EXCEPTIONS](EXCEPTIONS).
