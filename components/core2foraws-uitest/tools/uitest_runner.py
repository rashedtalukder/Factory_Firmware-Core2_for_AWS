# SPDX-FileCopyrightText: 2026 Rashed Talukder
# SPDX-License-Identifier: Apache-2.0
"""
Host-side runner for the Core2 for AWS LVGL UI test harness.

A small Playwright-style driver that injects touch input over UART and reads
back the firmware's structured replies. Pair with the screenshot component for
visual assertions.

Examples:
    # Print device geometry, dump the widget tree, then tap and swipe:
    python uitest_runner.py --port /dev/cu.usbserial-02036BEC info
    python uitest_runner.py --port /dev/cu.usbserial-02036BEC dump
    python uitest_runner.py --port /dev/cu.usbserial-02036BEC tap 160 120
    python uitest_runner.py --port /dev/cu.usbserial-02036BEC swipe 300 120 20 120 250
    python uitest_runner.py --port /dev/cu.usbserial-02036BEC click home.wifi_btn

    # Run a script of commands (one per line), e.g. a smoke test:
    python uitest_runner.py --port ... script smoke.txt

Firmware requirements:
    CONFIG_UITEST_ENABLED=y
    (do NOT also enable CONFIG_SCREENSHOT_SERIAL_TRIGGER — both read stdin)
"""

import argparse
import re
import sys
import time
import zlib

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial is required: pip install -r requirements.txt")

PREFIX = "UITEST"
DEFAULT_BAUD = 115200
DEFAULT_REPLY_TIMEOUT = 10.0
SHOT_TIMEOUT = 120.0
MAX_COMMAND_BYTES = 159


class UITestClient:
    def __init__(self, port, baud=DEFAULT_BAUD, prefix=PREFIX, verbose=False,
                 timeout=DEFAULT_REPLY_TIMEOUT):
        self.prefix = prefix
        self.verbose = verbose
        self.timeout = timeout
        self._rx_buffer = b""
        # Keep DTR/RTS deasserted so opening the port does not reset the board.
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = 0.1
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()

    def close(self):
        if self.ser.is_open:
            self.ser.close()

    def _send(self, line):
        if not line or "\n" in line or "\r" in line:
            raise ValueError("command must be one non-empty line")
        try:
            encoded = line.encode("ascii")
        except UnicodeEncodeError as exc:
            raise ValueError("commands must contain ASCII only") from exc
        if len(encoded) > MAX_COMMAND_BYTES:
            raise ValueError(f"command exceeds {MAX_COMMAND_BYTES} bytes")
        if self.verbose:
            print(f">> {line}")
        self.ser.reset_input_buffer()
        self._rx_buffer = b""
        self.ser.write(encoded + b"\n")
        self.ser.flush()

    def _read_until(self, predicate, timeout=None):
        """Collect lines until predicate(line) is True or timeout. Returns the
        list of matching/relevant lines that carry our prefix."""
        if timeout is None:
            timeout = self.timeout
        deadline = time.monotonic() + timeout
        collected = []
        while time.monotonic() < deadline:
            self._rx_buffer += self.ser.read(256)
            while b"\n" in self._rx_buffer:
                raw, self._rx_buffer = self._rx_buffer.split(b"\n", 1)
                line = raw.decode("ascii", errors="replace").strip()
                if not line:
                    continue
                if self.verbose:
                    print(f"<< {line}")
                if line.startswith(self.prefix) or line.startswith("---"):
                    collected.append(line)
                if predicate(line):
                    return collected
        return collected

    def command(self, line, timeout=None):
        """Send a command and wait for the matching OK/ERR reply."""
        verb = line.split(maxsplit=1)[0].upper()
        ok_prefix = f"{self.prefix}: OK {verb}"
        err_prefix = f"{self.prefix}: ERR {verb}"
        generic_error = f"{self.prefix}: ERR command too long"
        self._send(line)
        lines = self._read_until(
            lambda response: response.startswith(ok_prefix)
            or response.startswith(err_prefix)
            or response == generic_error,
            timeout=timeout,
        )
        ok = any(response.startswith(ok_prefix) for response in lines)
        if not ok:
            err = next(
                (response for response in lines if response.startswith(err_prefix)),
                 next((response for response in lines if response == generic_error),
                     "<timeout>"),
            )
            raise RuntimeError(f"command failed: {line!r} -> {err}")
        return lines

    # ── high-level helpers ────────────────────────────────────────
    def info(self):
        return self.command("INFO")

    def tap(self, x, y):
        return self.command(f"TAP {x} {y}")

    def long_press(self, x, y, ms):
        return self.command(f"LONGPRESS {x} {y} {ms}")

    def swipe(self, x0, y0, x1, y1, ms):
        return self.command(f"SWIPE {x0} {y0} {x1} {y1} {ms}")

    def click(self, obj_id):
        return self.command(f"CLICK {obj_id}")

    def shot(self):
        return self.command("SHOT", timeout=SHOT_TIMEOUT)

    def dump(self):
        self._send("DUMP")
        lines = self._read_until(
            lambda line: line.startswith(f"---{self.prefix}_DUMP_END---")
            or line.startswith(f"{self.prefix}: ERR DUMP")
        )
        error = next(
            (line for line in lines if line.startswith(f"{self.prefix}: ERR DUMP")),
            None,
        )
        if error:
            raise RuntimeError(f"DUMP failed: {error}")
        if not any(line.startswith(f"---{self.prefix}_DUMP_END---") for line in lines):
            raise RuntimeError("DUMP timed out")
        escaped_prefix = re.escape(self.prefix)
        node_pattern = re.compile(
            rf"^{escaped_prefix}: NODE d:(?P<depth>\d+) id:(?P<id>\S+) "
            r"x:(?P<x>-?\d+) y:(?P<y>-?\d+) w:(?P<width>-?\d+) "
            r"h:(?P<height>-?\d+) click:(?P<click>[01]) hidden:(?P<hidden>[01])$"
        )
        trailer_pattern = re.compile(
            rf"^{escaped_prefix}: DUMP CRC32:(?P<crc>[0-9a-fA-F]{{8}}) "
            r"COUNT:(?P<count>\d+) TRUNCATED:(?P<truncated>[01])$"
        )
        nodes = [line for line in lines if line.startswith(f"{self.prefix}: NODE")]
        for node in nodes:
            if node_pattern.fullmatch(node) is None:
                raise RuntimeError(f"malformed DUMP node: {node}")

        trailer_line = next(
            (line for line in lines if line.startswith(f"{self.prefix}: DUMP CRC32:")),
            None,
        )
        trailer = trailer_pattern.fullmatch(trailer_line or "")
        if trailer is None:
            raise RuntimeError("DUMP integrity trailer missing or malformed")
        if int(trailer.group("count")) != len(nodes):
            raise RuntimeError("DUMP node count mismatch")
        payload = "".join(f"{node}\n" for node in nodes).encode("ascii")
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        expected_crc = int(trailer.group("crc"), 16)
        if actual_crc != expected_crc:
            raise RuntimeError(
                f"DUMP CRC32 mismatch: got {actual_crc:08x}, expected {expected_crc:08x}"
            )
        return nodes


def main():
    p = argparse.ArgumentParser(description="LVGL UI test harness runner")
    p.add_argument("--port", required=True, help="Serial port")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    p.add_argument(
        "--timeout", type=float, default=DEFAULT_REPLY_TIMEOUT,
        help="Command reply timeout in seconds",
    )
    p.add_argument("-v", "--verbose", action="store_true")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info")
    sub.add_parser("dump")

    t = sub.add_parser("tap")
    t.add_argument("x", type=int)
    t.add_argument("y", type=int)

    lp = sub.add_parser("longpress")
    lp.add_argument("x", type=int)
    lp.add_argument("y", type=int)
    lp.add_argument("ms", type=int)

    sw = sub.add_parser("swipe")
    sw.add_argument("x0", type=int)
    sw.add_argument("y0", type=int)
    sw.add_argument("x1", type=int)
    sw.add_argument("y1", type=int)
    sw.add_argument("ms", type=int)

    cl = sub.add_parser("click")
    cl.add_argument("id")

    sub.add_parser("shot")

    sc = sub.add_parser("script")
    sc.add_argument("file")
    sc.add_argument(
        "--continue-on-error", action="store_true",
        help="Continue executing later script lines after a command failure",
    )

    args = p.parse_args()
    if args.timeout <= 0:
        p.error("--timeout must be greater than zero")
    client = UITestClient(
        args.port, args.baud, verbose=args.verbose, timeout=args.timeout
    )
    try:
        if args.cmd == "info":
            for l in client.info():
                print(l)
        elif args.cmd == "dump":
            for l in client.dump():
                print(l)
        elif args.cmd == "tap":
            client.tap(args.x, args.y)
            print("ok")
        elif args.cmd == "longpress":
            client.long_press(args.x, args.y, args.ms)
            print("ok")
        elif args.cmd == "swipe":
            client.swipe(args.x0, args.y0, args.x1, args.y1, args.ms)
            print("ok")
        elif args.cmd == "click":
            client.click(args.id)
            print("ok")
        elif args.cmd == "shot":
            client.shot()
            print("ok")
        elif args.cmd == "script":
            failures = 0
            with open(args.file, encoding="utf-8") as fh:
                for line_number, raw in enumerate(fh, 1):
                    line = raw.strip()
                    if not line or line.startswith("#"):
                        continue
                    print(f"-> {line}")
                    try:
                        client.command(line)
                    except (RuntimeError, ValueError) as exc:
                        failures += 1
                        print(f"{args.file}:{line_number}: {exc}", file=sys.stderr)
                        if not args.continue_on_error:
                            raise
            if failures:
                raise RuntimeError(f"script completed with {failures} failure(s)")
            print("script complete")
    finally:
        client.close()


if __name__ == "__main__":
    main()
