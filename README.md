# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, press the controller's View button (the small button with two overlapping rectangles) to switch views. When Starfield's Dialogue Camera setting is enabled, the gamepad cycles Dialogue Camera → Near Third Person → Far Third Person → Dialogue Camera. When Dialogue Camera is disabled, it switches between first and third person.

The mouse wheel retains Starfield's directional third-person distance steps. Scroll out from the dialogue camera to enter third person, continue scrolling to change the third-person distance, then scroll back in to return to the dialogue camera at the normal first-person boundary. Each dialogue-camera boundary notch performs one camera transition. With Dialogue Camera disabled, Starfield's normal first/third-person wheel behavior is left unchanged.

Scrolling farther inward at the dialogue-camera end or farther outward at the
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
4. Disable Dialogue Camera in Starfield's Accessibility settings, start another dialogue, and confirm the View button toggles First Person ↔ Third Person while the wheel retains normal behavior.
5. End the dialogue from both the dialogue-camera and third-person views. Confirm movement and camera-look work, then immediately activate the same NPC again.
6. Confirm each deliberate View-button press logs one `View-button input accepted` line and one `Dialogue toggle` line, while custom mouse events log `Mouse dialogue-view input consumed before PlayerControls`.

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
`ToggleDialogueCameraSF-v0.13.0-source.zip` and run the two Xmake commands from
its project root. The archive already contains the expanded dependency source
trees and does not require access to the private Git repository.

The build requires Xmake 3.0.9 or newer, MSVC with C++23 support, and the
Windows SDK. Exact dependency revisions and corresponding-source details are
listed in [SOURCE.md](SOURCE.md).

## License

Toggle Dialogue Camera SF is licensed under GPL-3.0-or-later WITH the Modding
Exception AND GPL-3.0 Linking Exception (with Corresponding Source). See
[COPYING](COPYING) and [EXCEPTIONS](EXCEPTIONS).
