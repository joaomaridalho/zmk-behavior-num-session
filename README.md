# zmk-behavior-num-session

> Disclaimer: This repository was initially generated with AI assistance and should be reviewed and tested before production use.

ZMK behavior module for a "smart num" session:

- tap once to activate a number layer
- configured keepers (numbers by default) keep the layer active
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
- remains active while keeper keys are pressed
- remains active when modifiers are pressed, and marks the session as modified
- if the session is modified, the next non-modifier key deactivates the layer
- if the session is not modified, the first non-keeper key deactivates the layer

Keepers are HID usages, not OS characters. Matching does not depend on keyboard layout.

## Configuration

Properties on the behavior instance:

- **`continue-list`** (optional): Extra HID keycodes that keep the session active before the first modifier.
- **`ignore-numbers`** (optional): Keep the session active for numeric keys (number row `1..0`, keypad `0..9`, keypad `00` / `000`).
- **`ignore-alphas`** (optional): Keep the session active for alphabetic keys (`A..Z`).

The default `&num_session` instance sets `ignore-numbers` only.

To keep extra keys as well (for example `.` `,` `+` `-`), overlay the continue list in the keymap:

```c
&num_session {
    continue-list = <DOT COMMA PLUS MINUS>;
};
```

List the HID keys your keymap actually sends. Do not assume a US punctuation set.

Custom instances:

```c
/ {
    behaviors {
        num_session_math: num_session_math {
            compatible = "zmk,behavior-num-session";
            #binding-cells = <1>;
            ignore-numbers;
            continue-list = <DOT COMMA PLUS MINUS STAR FSLH EQUAL>;
        };
    };
};
```

Kconfig:

- **`CONFIG_ZMK_BEHAVIOR_NUM_SESSION_MAX_ACTIVE`**: Maximum simultaneous num-session layers (default `10`).
