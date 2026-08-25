# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, click the right stick to switch views. When Starfield's Dialogue Camera setting is enabled, the gamepad cycles Dialogue Camera → Near Third Person → Far Third Person → Dialogue Camera. When Dialogue Camera is disabled, it switches between first and third person.

The mouse wheel retains Starfield's directional third-person distance steps. Scroll out from the dialogue camera to enter third person, continue scrolling to change the third-person distance, then scroll back in to return to the dialogue camera at the normal first-person boundary. Each dialogue-camera boundary notch performs one camera transition. With Dialogue Camera disabled, Starfield's normal first/third-person wheel behavior is left unchanged.

## Requirements

- Starfield 1.16.244
- SFSE
- Address Library for SFSE Plugins

## Install

Copy `ToggleDialogueCameraSF.dll` into `Data\SFSE\Plugins`.

## Testing

1. Start a dialogue and confirm the log reports `DialogueMenu opened`.
2. With Dialogue Camera enabled, click the right stick three times and confirm Dialogue Camera → Near Third Person → Far Third Person → Dialogue Camera without closing dialogue.
3. Scroll out from the dialogue camera, continue scrolling through Starfield's third-person distance steps, then scroll in until the dialogue camera returns at the normal boundary. Confirm each boundary notch produces one smooth transition without a second camera motion.
4. Disable Dialogue Camera in Starfield's Accessibility settings, start another dialogue, and confirm Right Stick Click toggles First Person ↔ Third Person while the wheel retains normal behavior.
5. End the dialogue from both the dialogue-camera and third-person views. Confirm movement and camera-look work, then immediately activate the same NPC again.
6. Confirm each deliberate Right Stick Click logs one `Right-stick input accepted` line and one `Dialogue toggle` line, while the log records mouse input only at view boundaries.

The SFSE log is written to `Documents\My Games\Starfield\SFSE\Logs\Toggle Dialogue Camera SF.log`.
