# Toggle Dialogue Camera SF

An SFSE port of Toggle Dialogue Camera for Starfield.

During dialogue, press `F` or Right Stick Click to switch between first- and third-person views. Both bindings can be changed in `Data\\SFSE\\Plugins\\ToggleDialogueCameraSF.ini`. Set `bAutoToggle=1` to enter first person when dialogue opens and return to third person when it closes.

## Requirements

- Starfield 1.16.244
- SFSE
- Address Library for SFSE Plugins

## Install

Copy `ToggleDialogueCameraSF.dll` and `ToggleDialogueCameraSF.ini` into `Data\\SFSE\\Plugins`.

## Current scope

The initial Starfield port preserves the dialogue-only POV toggle and automatic entry/exit behavior. The Skyrim version's direct third-person zoom manipulation and gradual zoom transition are intentionally excluded until their Starfield camera-state layout is separately verified.
