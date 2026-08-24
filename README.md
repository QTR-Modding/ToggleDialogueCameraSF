# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, use Starfield's Toggle POV input (`F`, mouse wheel, or Right Stick Click by default) to switch views. When Starfield's Dialogue Camera setting is enabled, the mod switches between the dialogue camera and third person. When Dialogue Camera is disabled, it switches between first and third person. The keyboard and gamepad fallbacks can be changed in `Data\\SFSE\\Plugins\\ToggleDialogueCameraSF.ini`. Set `bAutoToggle=1` to enter first person when dialogue opens and return to third person when it closes.

## Requirements

- Starfield 1.16.244
- SFSE
- Address Library for SFSE Plugins

## Install

Copy `ToggleDialogueCameraSF.dll` and `ToggleDialogueCameraSF.ini` into `Data\\SFSE\\Plugins`.

## Testing

1. Start a dialogue and confirm the log reports `DialogueMenu opened`.
2. With Dialogue Camera enabled, test `F`, mouse wheel, and Right Stick Click and confirm Dialogue Camera ↔ Third Person without closing dialogue.
3. Disable Dialogue Camera in Starfield's Accessibility settings, start another dialogue, and confirm First Person ↔ Third Person.
4. End the dialogue and confirm movement and camera-look controls work immediately without reloading.
5. Confirm the log contains exactly one `TogglePOV input received` and one `Dialogue toggle received` line per input.
6. Optionally set `bAutoToggle=1` and verify automatic first-person entry and third-person exit.

The SFSE log is written to `Documents\\My Games\\Starfield\\SFSE\\ToggleDialogueCameraSF.log`.

## Current scope

This test candidate adapts the dialogue-only toggle to Starfield's optional dialogue-camera state and supports optional automatic entry/exit behavior. The Skyrim version's direct third-person zoom manipulation and gradual zoom transition remain excluded until their Starfield camera-state layout is separately verified.
