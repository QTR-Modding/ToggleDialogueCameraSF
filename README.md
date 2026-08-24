# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, press `F` or Right Stick Click to switch between first- and third-person views. Both bindings can be changed in `Data\\SFSE\\Plugins\\ToggleDialogueCameraSF.ini`. Set `bAutoToggle=1` to enter first person when dialogue opens and return to third person when it closes.

## Requirements

- Starfield 1.16.244
- SFSE
- Address Library for SFSE Plugins

## Install

Copy `ToggleDialogueCameraSF.dll` and `ToggleDialogueCameraSF.ini` into `Data\\SFSE\\Plugins`.

## Testing

1. Start a dialogue and confirm the log reports `DialogueMenu opened`.
2. Press `F` or Right Stick Click once to request first person and again to request third person.
3. End the dialogue and confirm the log reports `DialogueMenu closed`.
4. Optionally set `bAutoToggle=1` and verify automatic first-person entry and third-person exit.

The SFSE log is written to `Documents\\My Games\\Starfield\\SFSE\\ToggleDialogueCameraSF.log`.

## Current scope

This test candidate implements the dialogue-only POV toggle and optional automatic entry/exit behavior. The Skyrim version's direct third-person zoom manipulation and gradual zoom transition remain excluded until their Starfield camera-state layout is separately verified.
