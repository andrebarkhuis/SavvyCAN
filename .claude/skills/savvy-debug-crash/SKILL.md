---
name: savvy-debug-crash
description: Find out why SavvyCAN (GUI or --cli) died, hung or disappeared on Windows - read the Windows Application event log for the APPCRASH record, decode the faulting module, exception code, process id and lifetime, locate the minidump, and map the result to known causes (XHorse MVCI32.dll crash after ~60-90 s, 32/64-bit DLL mismatch, locked bin folder). Use when a SavvyCAN process is gone unexpectedly, the user asks "did it crash", "why did it close", or a capture stops without a message.
---

# Debugging a SavvyCAN crash or hang

Everything here runs inside the repo and the Windows tools already on the machine.
Do not install anything or write helper scripts; run the commands inline.

## 1. Is it still running? Which build?

```powershell
Get-Process SavvyCAN -ErrorAction SilentlyContinue | Select-Object Id, StartTime, Path
```

`Path` tells you the build: `bin\x86\` is 32-bit, `bin\x64\` is 64-bit. Nothing listed
and nobody closed it means it crashed or exited: go to step 2.

## 2. Read the crash record from the event log

Windows logs every application crash. Pull the last hour of SavvyCAN entries:

```powershell
Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=(Get-Date).AddHours(-1)} |
  Where-Object { $_.Message -like '*SavvyCAN*' } |
  Select-Object TimeCreated, Id, ProviderName, Message | Format-List
```

Two events appear per crash:

- **Id 1000, "Application Error"**: faulting application, **faulting module name**,
  **exception code**, fault offset, **faulting process id (hex)**, **application start time
  (FILETIME, hex)**.
- **Id 1001, "Windows Error Reporting"**: the APPCRASH signature P1..P8 (P4 = module,
  P7 = exception code, P8 = offset) and the report folder under
  `C:\ProgramData\Microsoft\Windows\WER\ReportArchive\AppCrash_SavvyCAN.exe_*`.

Decode the numbers:

```powershell
[Convert]::ToInt32('423C', 16)                                  # process id -> decimal
[DateTime]::FromFileTime([Convert]::ToInt64('1DD63CC26F7BC88', 16))   # start time
```

Lifetime = event TimeCreated minus start time. Match the decimal pid against the one you
saw in step 1 (or launched) so you are explaining the right instance.

## 3. Read the faulting module

| Faulting module | Meaning | What to do |
|---|---|---|
| `MVCI32.dll` (XHorse / clone MVCI J2534 driver, v1.0.0.1, 2010) | Known vendor bug: access violation (0xC0000005, offset 0x1397) after roughly 60-90 s of an open channel. `workspace\REVERSE-ENGINEERING.md` records the same "~88 s DLL crash". | Not fixable in SavvyCAN. Keep sessions short (`--cli --count N`, MCP `capture`), or implement the periodic close/reopen cycle in `src\plugins\passthrucan`. |
| other vendor DLL (`PCANBasic.dll`, `vxlapi.dll`, ...) | Driver bug or wrong bitness. | Check bitness (step 5), then try the vendor's own tool. |
| `qtpassthrucanbus.dll` | The patched plugin itself. | Reproduce with the CLI (step 6) and read `src\plugins\passthrucan\passthrucanio.cpp`; it is only ~330 lines. |
| `Qt5*.dll` / `SavvyCAN.exe` | Application bug. | Reproduce with the CLI, then a debug build (`qmake CONFIG+=debug` in `build-win32\`) and the dump (step 4). |
| `ucrtbase.dll`, `KERNELBASE.dll` with 0xC0000409 or 0x40000015 | Fast-fail / abort: an assertion, `std::terminate`, or uncaught exception. | The real cause is one frame up; needs the dump. |

Exception codes: `0xC0000005` access violation, `0xC0000409` stack buffer overrun /
fast-fail, `0xC000001D` illegal instruction (wrong CPU build), `0xC0000135` /
`0xC000007B` DLL not found or wrong bitness at load, `0xE06D7363` C++ exception.

## 4. Minidumps

This machine has Windows LocalDumps enabled: every crash writes
`%LOCALAPPDATA%\CrashDumps\SavvyCAN.exe.<pid>.dmp`.

```powershell
Get-ChildItem "$env:LOCALAPPDATA\CrashDumps\SavvyCAN*" | Sort-Object LastWriteTime -Descending
```

Match `<pid>` to the decimal process id from step 2. No debugger is installed here
(no `cdb`/`windbg`); if a stack is really needed, ask the user before installing
WinDbg (`winget install Microsoft.WinDbg`) and then open the dump with `!analyze -v`.
For a vendor-DLL crash the module name alone is usually the whole answer.

## 5. Bitness mismatch

A vendor DLL only loads into a process of the same bitness. Symptoms: the 64-bit build
hangs or shows no device for a 32-bit J2534 DLL; the plugin lists the device but
connecting fails or times out. Check:

```powershell
Get-Process SavvyCAN | Select-Object Path            # which build is running
& "C:\Qt\5.15.2\msvc2019\bin\canbusutil.exe" -d passthrucan      # devices the 32-bit kit sees
& "C:\Qt\5.15.2\msvc2019_64\bin\canbusutil.exe" -d passthrucan   # devices the 64-bit kit sees
reg query "HKLM\SOFTWARE\WOW6432Node\PassThruSupport.04.04" /s   # registered 32-bit J2534 DLLs
```

Rule: XHorse / clone MVCI -> `bin\x86`. Serial adapters (CANable, GVRET) work with either.

## 6. Reproduce headless

The CLI exercises the same connection code without the GUI and exits on its own:

```
bin\x86\SavvyCAN.exe --cli --type serialbus --driver passthrucan --port "XHorse - MVCI" --speed 500000 --listen-only --count 20
```

Wrap it in a time limit (Git Bash `timeout 40 ...`) so a hang cannot block the session,
and afterwards check for a leftover process (step 1) and kill it, or the next
`build-win.cmd` will fail to replace `bin\<arch>` ("Cannot replace ... close any
SavvyCAN.exe"). Set `QT_PASSTHRUCAN_CONNECT_FLAGS` (`0x900` = upstream behaviour) to
test whether the plugin's connect-flag patch is involved.

## 7. Hangs (process alive, no output)

- `--cli` connect that never prints frames: another process holds the adapter (only one
  can), the bitness is wrong (step 5), or the bus speed is wrong. Kill and retry once.
- GUI frozen: the vendor DLL is blocking inside a J2534 call. If a dump is wanted, ask the
  user to use Task Manager -> Details -> right-click SavvyCAN.exe -> "Create dump file"
  (it lands in `%LOCALAPPDATA%\Temp`) before the process is killed. Then
  `Stop-Process -Name SavvyCAN -Force`.

## 8. Report

State plainly: what crashed (module, exception), when and after how long, which build,
whether it matches a known cause, and the options. Do not claim SavvyCAN closed on its own
when the record says a driver faulted, and do not claim a fix exists for a vendor DLL bug.
