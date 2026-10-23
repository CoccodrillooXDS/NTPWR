# NTPWR — NT Power Management Driver

NTPWR provides automatic power-off support for legacy Windows NT systems. It uses uACPI to inspect firmware ACPI tables and evaluate AML, with a kernel shutdown hook and a Winlogon service to handle power-off while preserving restart behavior. The shutdown-hook implementation is adapted from [QEMUPWR](https://github.com/CoccodrillooXDS/QEMUPWR).

The current ACPI S5 backend supports a specific QEMU PIIX4 firmware configuration. Generic ACPI and APM power-off are not yet supported.

## Compatibility

- **Windows NT 3.51 SP5 (x86):** shutdown and restart on QEMU PIIX4.
- **Windows NT 4.0 SP6 (x86):** shutdown and restart on QEMU PIIX4.
- **Other NT versions:** unsupported until their kernel interfaces and shutdown paths have been validated.

The current kernel hook recognizes specific NT 3.51 and NT 4.0 kernel profiles. The ACPI power-off backend requires the expected BOCHS/BXPC firmware and PM1 register configuration. Other configurations retain the original HAL behavior rather than attempting an unsupported power-off.

## Components

- `NTPWR.EXE` — native Win32 GUI, command-line utility, driver installer and Winlogon service.
- `NTPWR.SYS` — kernel driver, uACPI interpreter, ACPI diagnostics and shutdown hook.

Only these two executables are required for installation. NTPWR cannot operate alongside QEMUPWR because both drivers hook the same shutdown path.

## Build

The build hosts supported by the scripts are Linux, macOS and Windows (Git Bash). The output is always 32-bit Windows NT PE binaries.

Requirements:

- Clang 17 or later and LLD (`lld-link`)
- Python 3
- Git
- Bash
- Network access to retrieve the pinned uACPI 7.0.0 source, unless a local checkout is provided

No Microsoft DDK, Visual C++ 2.0, precompiled object files or Microsoft SDK headers are required. The project includes its own minimal NT/Win32 x86 ABI declarations in `include/`.

```sh
bash BUILD.sh
```

Build outputs:

```text
DIST/NTPWR.EXE
DIST/NTPWR.SYS
```

To build with a local uACPI 7.0.0 checkout:

```sh
UACPI_SOURCE=/path/to/uACPI bash BUILD.sh
```

To rebuild only the user-mode executable:

```sh
bash BUILD-EXE.sh
```

`BUILD.sh` pins uACPI 7.0.0 to commit `686a9a52d9ad72690406d7cada82801377d5e2fa`. GitHub Actions builds both binaries on Ubuntu, macOS and Windows, and uploads the results as workflow artifacts.

## Usage

Copy `NTPWR.EXE` and `NTPWR.SYS` from the same build into a directory on the target system. Remove QEMUPWR or an earlier NTPWR installation before installing NTPWR.

Run `NTPWR.EXE` to open the GUI, or use the command-line interface:

```cmd
NTPWR.EXE SCAN
NTPWR.EXE INFO
NTPWR.EXE INSTALL
NTPWR.EXE STATUS
NTPWR.EXE REMOVE
```

`RESULT`, `START` and `STOP` are also available for diagnostics. A running driver with an active kernel hook cannot be unloaded normally. If command-line output is unavailable, inspect `NTPWR-CLI.LOG` in the current directory.

Use a VM snapshot when evaluating an untested operating system or firmware configuration.

## Limitations

- S5 power-off is currently limited to the validated QEMU PIIX4 firmware profile; firmware-independent FADT/GAS handling is not implemented.
- AML shutdown preparation methods such as `_PTS` and `_GTS` are not executed.
- The uACPI adapter runs in reduced-hardware mode, without the full ACPI event and hardware-access subsystem.
- The APM backend is not implemented.
- Compatibility outside the listed Windows NT kernel profiles is not established.

## License

NTPWR was developed with assistance from AI (ChatGPT by OpenAI).

NTPWR is licensed under the MIT License. See `LICENSE`. It incorporates uACPI 7.0.0 under the MIT License; see `THIRD_PARTY_NOTICES.md` for the applicable copyright and license text.
