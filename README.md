# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, press `F`, scroll the mouse wheel, or click the right stick to switch views. When Starfield's Dialogue Camera setting is enabled, the mod switches between the dialogue camera and third person. When Dialogue Camera is disabled, it switches between first and third person. The keyboard and gamepad fallbacks can be changed in `Data\\SFSE\\Plugins\\ToggleDialogueCameraSF.ini`. Set `bAutoToggle=1` to enter first person when dialogue opens and return to third person when it closes.

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
4. End the dialogue from both the dialogue-camera and third-person views. Confirm movement and camera-look work, then immediately activate the same NPC again.
5. Confirm each deliberate press or wheel gesture logs one `Toggle input accepted` line and one `Dialogue toggle` line.
6. Optionally set `bAutoToggle=1` and verify automatic first-person entry and third-person exit.

The SFSE log is written to `Documents\\My Games\\Starfield\\SFSE\\Logs\\Toggle Dialogue Camera SF.log`.
