# What is IREC?
IRCAP allow you to intercept all IRP requests to the target driver without any side effects. It can be used to collect interesting corpus for fuzzing and can manipulate I/O communication between an application and kernel. 

## Motivation
A variety of interesting corpus are important to improve efficiency of coverage-guided fuzzing. If we able to capture I/O communication between an application and kernel, it will be of great help to fuzzer. So we had to find a way to use IRP requests as initial seeds.

## Getting started
You need to install Visual Studio and Windows Driver Kit. Once you have completed building an driver development environment, open `ircap/hook.h` for editing.

```c
// ircap/hook.h
#pragma once

UNICODE_STRING TARGET_DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Driver\\@@DEVICE_NAME@@");
UNICODE_STRING PROGRAM_FILE_PATH  = RTL_CONSTANT_STRING(L"\\DosDevices\\C:\\program.irp");
```
Specfiy `TARGET_DEVICE_NAME` to the device name you want to hook. And `PROGRAM_FILE_PATH` is the file path where captured IRP requests are stored.

To capture IRP requests to target driver, follow these steps (Run cmd as administrator):
1. Register a `ircap.sys` as a boot service.
```bash
sc.exe create ircap binpath=ircap.sys type=kernel start=boot
```
2. Reboot your computer
```bash
reboot
```
3. Run the application that loads the target driver.
4. After running many operatons by clicking the application, unload `ircap.sys` manually.
5. Then, captured IRP requests is stored in `C:\program.irp`  <br><br>


> **Caution!** <br> When the capturing finished, you should remove the service.
<br> sc.exe delete ircap
