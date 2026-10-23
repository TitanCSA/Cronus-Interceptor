# Public corpus results

Pinned source revisions and interpretation are described in [VALIDATION.md](VALIDATION.md).
`RUN` means bounded synthetic execution only. `FAULT` means a runtime limitation
was detected and passthrough restored. `REJECT` means the compiler refused the
script. LED/motor effects are virtual and require the compatibility notice.

| Original filename | Result | Observation |
| --- | --- | --- |
| absw-grind tatooine1-1.gpc | RUN | Synthetic smoke completed; not hardware parity |
| me train.gpc | RUN | Synthetic smoke completed; not hardware parity |
| moonlight ps.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Sixaxis Aim [2015].gpc | FAULT | Requested sensor/touch/paddle input is unavailable from this controller mapping. |
| spin left stick.gpc | RUN | Synthetic smoke completed; not hardware parity |
| OverBlast.gpc | RUN | Synthetic smoke completed; not hardware parity |
| ps4leds-ultra.gpc | RUN | Synthetic smoke completed; not hardware parity |
| MGS pressure sensitive button limiter.gpc | RUN | Synthetic smoke completed; not hardware parity |
| [v31lbp] Auto-No-Kick.gpc | REJECT | Line 512, column 13: Unsupported or unknown GPC function 'ps4_touchpad'. Physical feedback, console authentication, USB ports and proprietary GamePacks require Cronus hardware; see Compatibility. |
| hold r2 and x.gpc | RUN | Synthetic smoke completed; not hardware parity |
| jump delay.gpc | RUN | Synthetic smoke completed; not hardware parity |
| modulo 2 test.gpc | RUN | Synthetic smoke completed; not hardware parity |
| quick hold stick and r2.gpc | RUN | Synthetic smoke completed; not hardware parity |
| RandC - R2 to fire, strafe toggle.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Volume disguise uses grind.gpc | RUN | Synthetic smoke completed; not hardware parity |
| yorbie uninvert.gpc | REJECT | Line 38, column 5: Unsupported or unknown GPC function 'ps4_touchpad'. Physical feedback, console authentication, USB ports and proprietary GamePacks require Cronus hardware; see Compatibility. |
| [v30] Stick direction indicator using LEDs.gpc | REJECT | Line 70, column 5: Invalid argument count for getPlayerNumber. |
| LittleBigPlanet - Automated Crowd Pleaser trophy.gpc | RUN | Synthetic smoke completed; not hardware parity |
| ModNation Racers Creation Downloader.gpc | REJECT | Line 173, column 7: get_battery needs battery telemetry not provided by this driver SDK. |
| Auto-No-Kick.gpc | REJECT | Line 485, column 13: Unsupported or unknown GPC function 'ps4_touchpad'. Physical feedback, console authentication, USB ports and proprietary GamePacks require Cronus hardware; see Compatibility. |
| Automated Turbo Button Press.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Inversion.gpc | REJECT | Line 644, column 2: Unsupported or unknown GPC function 'ps4_set_touchpad'. Physical feedback, console authentication, USB ports and proprietary GamePacks require Cronus hardware; see Compatibility. |
| LBP Master.gpc | REJECT | Line 411, column 35: Expected ';', found 'set_led'. |
| LBP One-Handed.gpc | RUN | Synthetic smoke completed; not hardware parity |
| combo GrInDeR vol1.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Controller Rumble Test.gpc | RUN | Synthetic smoke completed; not hardware parity |
| DirectionIndicator.gpc | RUN | Synthetic smoke completed; not hardware parity |
| RubberBandMatrix.gpc | RUN | Synthetic smoke completed; not hardware parity |
| One Handed Controller Extra Buttons.gpc | REJECT | Line 190, column 19: Unknown variable 'XB1_PL1'. |
| PrecisionAnimator.gpc | RUN | Synthetic smoke completed; not hardware parity |
| ARC Alternative Racing Controls.gpc | RUN | Synthetic smoke completed; not hardware parity |
| DonutFactory.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Inverted Throttle.gpc | RUN | Synthetic smoke completed; not hardware parity |
| Sixaxis Remapper.gpc | FAULT | Requested sensor/touch/paddle input is unavailable from this controller mapping. |
| StickyButton.gpc | RUN | Synthetic smoke completed; not hardware parity |
| COD Pro.gpc | REJECT | Line 524, column 21: A constant integer expression is required here. |
