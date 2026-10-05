# Factory Validation

Run from the repository root with Python 3, pyserial, Pillow and a native C
compiler installed:

```sh
sh components/Core2-for-AWS-IoT-Kit/tests/host/run.sh
python3 -m unittest discover -s components/core2foraws-screenshot/tools -v
python3 -m unittest discover -s components/core2foraws-uitest/tools -v
python3 -m unittest discover -s tests/host -v
pio run -e core2foraws
```

Native tests compile actual C functions with minimal SDK stubs and sanitizers.
They cover screenshot encoding/queue/lifecycle, unique widget IDs, strict integer
parsing, canceled pointer resets, LED repaint/retry, BSP scan ownership, and
microphone/speaker cleanup retained after failed startup or tab exit.
They do not prove real task scheduling, UART byte integrity, or sensor accuracy.

## Hardware Smoke

Enable `CONFIG_UITEST_ENABLED=y`, keep `CONFIG_SCREENSHOT_SERIAL_TRIGGER` off,
and enable screenshot async support. Build and flash the application using the
device's verified partition table and boot-selected application slot. Do not
assume an offset for another device or overwrite its data partitions.

```sh
python3 components/core2foraws-uitest/tools/uitest_runner.py \
  --port /dev/cu.usbserial-02036BEC --timeout 15 \
  script tests/hardware/factory_smoke.txt --artifacts /tmp/core2-factory-smoke
```

The script starts at Home and checks named queries, diagnostics, complete dumps,
microphone entry/exit, LED values, power LED toggles, completed Wi-Fi scans,
screenshots and cancellation. It does not write secure-element slots or change
RTC time. Artifact images can contain nearby network names; do not publish them
without review. Restore the normal configuration with both serial listeners
disabled when testing ends.

For a long binary-transfer test, instead enable the screenshot serial trigger,
select COBS, disable RLE, and keep UITEST off. The factory initializes the UART
driver; the default polling UART backend was not byte-exact in this test.

```sh
python3 components/core2foraws-screenshot/tools/screenshot_capture.py \
  --port /dev/cu.usbserial-02036BEC --no-reset --trigger --delta \
  --count 2 --timeout 90 --output /tmp/core2-cobs.png --sidecar
```

## Verified Scope (2026-09-07)

- ESP-IDF 6.1.0 / Espressif32 7.1.2 on the connected Core2 AWS board.
- Factory smoke: 47 commands passed, including five 320x240 CRC-verified images.
- Separate microphone test: five repeated enter/exit cycles with Listening/Idle
  assertions, named motor on/off checks, and three consecutive Power captures.
- COBS: full 231,301-byte encoded keyframe decoded to 230,400 bytes with matching
  CRC, followed by a 12,096-byte decoded delta with matching CRC at 115200 baud.
- Native BSP sanitizer suite and 57 component/factory host tests passed
  (32 screenshot, 21 UI-test, 4 factory tests).
- Clock, Mic, Power, diagnostics and populated Wi-Fi layouts inspected from
  device captures. Microphone signal accuracy and acoustic playback quality
  were not measured. Wi-Fi scanning passed; association/provisioning was not
  exercised. Hardware allocation-failure retry screens were not fault-injected.

Two hardware regressions discovered during this work were fixed: running SHOT
on the 8 KiB UI listener stack overflowed on Power, so it now waits on the
dedicated capture worker; long COBS output was corrupted after encoding by the
polling-console path, while the interrupt-driven UART backend passed unchanged
encoder output. An initial malformed dump was rejected, not accepted as a pass.

No secure-element provisioning, lock, slot, key or counter writes were performed.
Application-only flashes left the partition table intact. The stack-overflow
test caused the existing crash handler to write its coredump partition.