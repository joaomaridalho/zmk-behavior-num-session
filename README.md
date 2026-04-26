# zmk-behavior-num-session

> Disclaimer: This repository was initially generated with AI assistance and should be reviewed and tested before production use.

ZMK behavior module for a "smart num" session:

- tap once to activate a number layer
- plain numbers keep the layer active
- the first modifier marks the session as modified
- after that, the next non-modifier key exits the layer
- additional modifiers keep stacking before that final key

This is intended for the flow:

`Num`, `1`, `2`, `3`

and also:

`Num`, `Shift`, `Gui`, `1`

without needing a second helper layer in the keymap.

## Module name

The Zephyr/ZMK module name is `zmk-behavior-num-session`.

## Installation

Add the module to your `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: joaomaridalho
      url-base: https://github.com/joaomaridalho
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-behavior-num-session
      remote: joaomaridalho
      revision: main
  self:
    path: config
```

## Usage

Include the provided behavior definition:

```c
#include <behaviors/num_session.dtsi>
```

Then bind it with the index of your number layer:

```c
&num_session NUM
```

## Behavior semantics

`&num_session <layer>`

- activates `<layer>`
- remains active while numeric keycodes are pressed
- remains active when modifiers are pressed, and marks the session as modified
- if the session is modified, the next non-modifier key deactivates the layer
- if the session is not modified, the first non-numeric key deactivates the layer

Numeric detection currently covers:

- keyboard number row `1..0`
- keypad `0..9`
- keypad `00`
- keypad `000`

## Notes

This module is modeled on the same out-of-tree behavior/module pattern used by `zmk-auto-layer`, but the behavior is purpose-built for smart number entry rather than exposing a generic continue-list API.
