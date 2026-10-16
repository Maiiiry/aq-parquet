# Mobile migration notes

Firmware changes that opensensor-space-mobile may see on the wire or in
LOG_TAIL. No backward compatibility is kept, so each entry says what changed
and whether the app must act. Newest first.

## Smaller resident writer workspace (feat/multi-chip-portability-and-watchdog)

- `PARQUET BEGIN psram_workspace_bytes=` drops because the codec-test
  benchmark file is no longer resident, 213112 to 162136 bytes on both
  boards. Value change only.
- LOG_TAIL `PARQUET ERROR operation=codec-test reason=no-psram` is replaced by
  `PARQUET ERROR operation=allocate mem=psram bytes=N purpose=codec-test`.
- A failed workspace allocation at boot now logs
  `PARQUET ERROR operation=allocate mem=psram bytes=N purpose=writer-state
  logging=false`. Other boot allocation failures keep the old line.
- The app does not parse these lines, so no app change is required.

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
