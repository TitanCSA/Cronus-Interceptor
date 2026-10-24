# Build and test

Windows 10/11 x64, Visual Studio 2022 Desktop C++ tools, a Windows SDK and CMake.
The WDK and an installed driver are not needed to compile the app or run offline
tests. No .NET runtime or scripting runtime is required by the release executable.

```powershell
.\Build.ps1
.\Build.ps1 -AddressSanitizer
.\Build.ps1 -Render
.\Build.ps1 -Target Package
```

Output: `App/.build/Release/bin/CronusInterceptor.exe`. The portable package
contains the native executable, scripts, documentation and license files. Source
is available directly from the repository; no kernel implementation is included.

```powershell
.\CronusInterceptor.exe --demo
.\CronusInterceptor.exe --render studio.png
.\CronusInterceptor.exe --self-test ui-result.json
```

Offline modes do not elevate or open a controller/driver handle. The UI test
does not read/write your live script library or persistent preferences. Core
tests use synthetic inputs, hostile-source fixtures and bounded stress loops.

The optional `App/Tests/Fetch-Corpus.ps1` downloads pinned public GPL scripts to
the ignored build directory. The repository does not redistribute those scripts
as its own examples. Run `GpcTests.exe --corpus <download-directory>` to repeat
the compilation and synthetic execution checks. See [validation](VALIDATION.md)
for results and the distinction between smoke tests and behavior assertions.

## Source layout

| Folder | Role |
| --- | --- |
| `App/Core` | GPC compiler, bounded bytecode VM, controller identifiers, persistence |
| `App/Capture` | SDK bridge, per-device execution, timer output, passive telemetry |
| `App/UI` | Native editor, library, diagnostics and monitor |
| `App/Tests` | Semantic, corpus, codec and stress fixtures |
| `Interceptor` | Two-file Gamepad & Controller SDK |
| `Scripts` | Original MIT examples |

The capture bridge and parts of the bytecode engine originate from TitanCSA's
MIT-licensed GamePad Remapper. This fork changes GPC identifier numbering,
language behavior, cycle scheduling, persistence and HID/native controller mapping.

## Driver setup packaging

Normal startup checks driver metadata and registration before capture/monitoring.
Missing, unavailable and access-denied states have separate installation guidance.
The check never creates a capture or starts the allowance.

`Build.ps1 -Target Package` downloads the pinned Gamepad & Controller 1.0.13.0
GUI installer when it is not already cached. Packaging verifies its SHA-256,
product name and version, then includes it in the ZIP and as `InterceptorSetup.exe`.
Offline compilation and tests do not download or install the driver.

`--check-driver report.json` writes a read-only availability report; it does not
start capture or monitoring. It does not request elevation, so a non-elevated
caller may receive an access-denied state even when the driver is installed.
