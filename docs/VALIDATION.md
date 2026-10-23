# Validation and public-script coverage

Version 0.1.0, 23 October 2026. These are conformance and integration observations,
not certification that every `.gpc` behaves identically to a physical Cronus.
No physical Cronus hardware or official compiler oracle was available for a
byte-for-byte, cycle-for-cycle comparison.

## Offline semantics and robustness

The Release and separate AddressSanitizer configurations pass the CTest suites
for GPC syntax/examples, expected semantics and isolated persistent storage.
The semantic suite includes more than **4.48 million assertions**, comprising:

- Every signed 16-bit native axis value for unchanged raw passthrough.
- Native XInput/GIP Y-sign conversion and Sony HID button/axis mapping.
- Both integer widths, wrap boundaries, short-circuit logic, bit mutation,
  data readers, typed matrices and independent dimension bounds.
- Event edges, previous/physical/current values, timers and the 10 ms VM base.
- Held combo steps, empty waits, nested calls, release and timed blocking.
- 200,000 randomized controller cycles and 20,000 malformed-source mutations.
- Instruction-budget, array-index and unsupported-function error paths.

The registry fixture uses a unique test identity, checks shared cache and final
flush, rejects a corrupt value type, and removes its own test key. It does not
alter an existing script's persistence. Offline UI checks compile valid and
invalid source and confirm that offline Run opens no driver.

CPU-only microbenchmarks are not USB-to-game latency measurements. Actual GPC
cycle timing and Windows scheduling are more relevant than a throughput figure.

## Actual Windows input path

The production capture bridge and GPC engine ran in a dedicated Windows Hyper-V
VM, against owned persistent USB GIP and HID VHF fixtures, using an isolated
test-signed copy of the pending one-hour driver.

- The same GPC source swapped A/B and inverted Y for both families; Windows
  XInput and HID consumers received the expected representable output.
- Buttons outside the edits, triggers and opaque report content were retained.
- Timed combos progressed with no further physical-source submissions.
- Stop restored the original held native state.
- An infinite-loop script produced the instruction-budget diagnostic and
  released capture into passthrough.
- A bounded 3,200-report HID stress run finished with **3,202 received / 3,248
  forwarded** (including timer output), and **186 / 186 process handles** before
  and after. No report-loss claim beyond this bounded test is made.

The test package, services and filter associations were removed afterward;
original VM package/security baselines were restored and Windows shut down
normally. No driver was installed on the workstation hosting development.

## Public GPC corpus

The optional downloader pins two repositories:

- [cypher139/cronus](https://github.com/cypher139/cronus/tree/f364883e1df49451d5165c2ad1088244380f90f6)
- [Fadexz/COD-Pro](https://github.com/Fadexz/COD-Pro/tree/7fd78ebb3deedadcdbe2fbc6baca4105e3f9097e)

For **36 source files**, 27 compile. Of these, 25 complete the bounded synthetic
input smoke test; two stop with an explicit unavailable-sensor diagnostic. Nine
are rejected at compilation. Detailed per-file results are in
[CORPUS-RESULTS.md](CORPUS-RESULTS.md).

Separate expected-behavior fixtures make **216 assertions** against four original
downloaded scripts in both integer modes: pressure-button limiting, modulo/edge
traces and virtual LED state, the Moonlight guide-button chord and the complete
inverted-throttle range. These assert expected source behavior, not results read
from a physical Cronus.

The smoke inputs do not exercise every branch or feature in a script. A `RUN`
result is not a universal compatibility badge. In particular, LED/motor commands
remain explicitly virtual; unavailable touch/sensor data is never invented.
Scripts requiring OLED, touchpad synthesis, physical feedback or other missing
facilities remain unsupported. Some old public sources also omit required
arguments or statement separators.

The GPL downloads and their licenses stay in ignored build storage. They are
not redistributed as this project's original MIT example scripts or bundled
with the portable release.
