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

## Testing

1. Start a dialogue and confirm the log reports `DialogueMenu opened`.
2. With Dialogue Camera enabled, press the View button three times and confirm Dialogue Camera → Near Third Person → Far Third Person → Dialogue Camera without closing dialogue.
3. Scroll out from the dialogue camera, continue scrolling through Starfield's third-person distance steps, then scroll in until the dialogue camera returns at the normal boundary. Confirm each boundary notch produces one smooth transition without a second camera motion, and that scrolling farther at either end leaves that endpoint unchanged.
4. Disable Dialogue Camera in Starfield's Accessibility settings, start another dialogue, and confirm the View button cycles First Person → Near Third Person → Far Third Person → First Person.
5. In the same disabled-mode dialogue, scroll out from First Person through Near and Far Third Person, then scroll back in to First Person. Confirm each notch performs one smooth transition and each terminal notch leaves the endpoint unchanged.
6. End dialogue from First Person, both third-person distances, and the dialogue camera. Confirm movement and camera-look work, then immediately activate the same NPC again.
7. Confirm each deliberate View-button press logs one `View-button input accepted` line and one `Dialogue toggle` line. Boundary and terminal mouse events log `Mouse dialogue-view input consumed before PlayerControls`; enabled-mode native third-person steps log `Mouse middle-distance input stopped after one ThirdPersonState dispatch`.

The SFSE log is written to `Documents\My Games\Starfield\SFSE\Logs\Toggle Dialogue Camera SF.log`.

## Building

Clone recursively so the pinned CommonLibSF, commonlib-shared, and spdlog
sources are present:

```powershell
git clone --recursive https://github.com/QTR-Modding/ToggleDialogueCameraSF.git
cd ToggleDialogueCameraSF
xmake f -m release -a x64 -y
xmake -y
```

Binary recipients can instead extract the accompanying
`ToggleDialogueCameraSF-v0.17.0-source.zip` and run the two Xmake commands from
its project root. The archive already contains the expanded dependency source
trees and does not require access to the private Git repository.

The build requires Xmake 3.0.9 or newer, MSVC with C++23 support, and the
Windows SDK. Exact dependency revisions and corresponding-source details are
listed in [SOURCE.md](SOURCE.md).

## License

Toggle Dialogue Camera SF is licensed under GPL-3.0-or-later WITH the Modding
Exception AND GPL-3.0 Linking Exception (with Corresponding Source). See
[COPYING](COPYING) and [EXCEPTIONS](EXCEPTIONS).
