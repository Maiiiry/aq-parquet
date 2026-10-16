# Mobile migration notes

Firmware changes that opensensor-space-mobile may see on the wire or in
LOG_TAIL. No backward compatibility is kept, so each entry says what changed
and whether the app must act. Newest first.

## Storage worker watchdog (feat/multi-chip-portability-and-watchdog)

- LOG_TAIL `PARQUET ERROR operation=worker-stall` gains a trailing
  `watchdog=armed|warn|off` field. Additive.
- New LOG_TAIL lines `PARQUET WATCHDOG mode=... timeout_s=... consecutive=...`
  at boot and `PARQUET ERROR operation=worker-watchdog-reset consecutive=N`
  after a stall reset. Additive.
- A stalled storage worker now resets the device after about 60 s instead of
  staying wedged. The app sees a normal BLE/LAN disconnect and reconnect.
- No STATUS, LIVE, GET_DIAG, GET_CONFIG or advertising field changes. The app
  does not parse these log lines today, so no app change is required.
