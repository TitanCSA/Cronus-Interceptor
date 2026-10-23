# GPC compatibility

This independent interpreter targets documented Cronus GPC source syntax, with
selectable 16-bit or 32-bit signed arithmetic. It is not a universal replacement
for every firmware/compiler version. GPC2 and compiled/encrypted GamePacks are
different formats and are not decoded.

| Area | Implemented behavior / boundary |
| --- | --- |
| Source | UTF-8, line/block comments, decimal/hex integers, case-sensitive names |
| Structure | `define`, global variables, `init`, `main`, functions, combos |
| Data | Read-only byte data and signed/unsigned byte/word readers |
| Arrays | One/two-dimensional constant arrays, typed integer elements, checked indices; bounded mutable arrays |
| Expressions | Arithmetic, bitwise/logical operations, `^^`, comparisons, ternary, increments, compound assignment, short circuit |
| Flow | `if/else`, `while`, `do/while`, `for`, `switch`/fallthrough, enum, return, break/continue |
| Controller | Common Xbox/PlayStation/Switch/Wii identifier numbers, including numeric identifiers; standard controls are mapped to native/HID input |
| Reads | `get_val`, `get_ival`, `get_lval`, press/release events, state timers |
| Writes | `set_val`, swap, static remap/unmap, block, sensitivity, deadzone, stickize, polar operations |
| Timing | Default 10 ms, `vm_tctrl` offset, combos, nested `call`, run/stop/restart, suspension/resume and step queries |
| Math | abs, inverse, pow, integer sqrt, random, clamp, min/max, bit mutation/extraction |
| Persistence | Software SPVAR namespace per file; shared PVAR namespace; registry writes outside report processing |
| Debug | Six script traces, virtual LED/motor state, exact source diagnostic |
| Platform identity | Compatibility profile: XUSB = Xbox 360, GIP = Xbox One, HID = PS4 default; reported before Run when used |
| Physical feedback | Not available through this SDK. `get_rumble`, battery/ADT/touchpad synthesis and related unsupported functions are rejected |
| Cronus peripherals | LED/motor setters are explicitly virtual; OLED, USB ports, console authentication, slots and hardware control are not emulated |

The current `set_polar` path converts through integer control percentages; it is
not a promise of Cronus raw polar resolution. Random sequences are deterministic
for repeatable testing, not bit-identical to a device's random generator. Windows
timer deadlines are subject to scheduler jitter. HID button positions outside
standard/Sony usage conventions are not universally identifiable.

Compiler errors leave the currently running program intact. Runtime errors,
including unavailable sensor/touch controls or an exhausted instruction budget,
restore physical passthrough. No unsupported function is replaced with an empty
native stub. Calls needing hardware are diagnosed rather than reported as a
successful physical effect.

## Reference material

Implementation and fixtures were checked against these first-party references:

- [Language introduction and integer modes](https://guide.cronus.support/gpc)
- [Basic syntax and statement termination](https://guide.cronus.support/gpc/basic-syntax)
- [Structure](https://guide.cronus.support/gpc/basic-gpc-structure-with-gpc-scripting)
- [Variables](https://guide.cronus.support/gpc/gpc-scripting-variables)
- [User functions](https://guide.cronus.support/gpc/gpc-scripting-user-created-functions)
- [Controller operations](https://guide.cronus.support/gpc/gpc-scripting-core-controller-functions)
- [Combo semantics](https://guide.cronus.support/gpc/gpc-scripting-combo-functions)
- [VM timing and polar operations](https://guide.cronus.support/gpc/device-functions)
- [Math](https://guide.cronus.support/gpc/gpc-scripting-math-functions)
- [Bit operations](https://guide.cronus.support/gpc/gpc-scripting-bit-functions)
- [Remapping](https://guide.cronus.support/gpc/gpc-scripting-remapping)
- [Xbox identifiers](https://guide.cronus.support/gpc/xbox-one)
- [PlayStation identifiers](https://guide.cronus.support/gpc/playstation-5)
- [Persistence](https://guide.cronus.support/gpc/persistent-memory)

Some documentation pages reflect different firmware generations and disagree on
limits. Test results describe this interpreter, not an exhaustive firmware oracle.
