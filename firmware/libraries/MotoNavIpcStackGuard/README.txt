MotoNavIpcStackGuard
====================

Reserve an adequate stack for the two default ESP-IDF IPC tasks during
Bluetooth initialization. Only task creations named ipc0/ipc1 whose
priority is maxPriority-1, core argument matches the task index and
requested stack is below 4096 bytes are grown to 4096 bytes. Everything
else passes through unchanged, about 6 KB of extra dynamic memory total.

The hook is wired by the linker: the build must pass
`-Wl,--wrap=xTaskCreatePinnedToCore` (firmware/build.py does). If the
flag is missing the link fails on `__real_xTaskCreatePinnedToCore`
instead of silently shipping 1 KiB IPC stacks.

`motoNavIpcStackStatus()` is a read-only snapshot for boot diagnostics,
call it after BLE initialization in setup().
