import unittest
import zlib

import uitest_runner


class ScriptedSerial:
    def __init__(self, responses=None):
        self.responses = responses or {}
        self.pending = bytearray()
        self.writes = []
        self.reset_count = 0
        self.is_open = True

    def reset_input_buffer(self):
        self.pending.clear()
        self.reset_count += 1

    def write(self, data):
        self.writes.append(data)
        command = data.decode("ascii").strip()
        response = self.responses.get(command, b"")
        if isinstance(response, str):
            response = response.encode("ascii")
        self.pending.extend(response)
        return len(data)

    def flush(self):
        pass

    def read(self, size):
        if not self.pending:
            return b""
        chunk_size = min(size, 3, len(self.pending))
        chunk = bytes(self.pending[:chunk_size])
        del self.pending[:chunk_size]
        return chunk

    def close(self):
        self.is_open = False


def make_client(serial, timeout=0.02):
    client = object.__new__(uitest_runner.UITestClient)
    client.prefix = "UITEST"
    client.verbose = False
    client.timeout = timeout
    client._rx_buffer = b""
    client.ser = serial
    return client


class ClientTests(unittest.TestCase):
    def test_matches_reply_verb_and_fragmented_lines(self):
        serial = ScriptedSerial({
            "INFO": (
                "UITEST: OK TAP 1 2\n"
                "UITEST: OK INFO W:320 H:240 ROT:0 PERIOD:30\n"
            )
        })
        lines = make_client(serial).info()
        self.assertEqual(lines[-1], "UITEST: OK INFO W:320 H:240 ROT:0 PERIOD:30")
        self.assertEqual(serial.reset_count, 1)
        self.assertEqual(serial.writes, [b"INFO\n"])

    def test_raises_matching_device_error(self):
        serial = ScriptedSerial({"TAP 999 999": "UITEST: ERR TAP ESP_ERR_INVALID_ARG\n"})
        with self.assertRaisesRegex(RuntimeError, "ESP_ERR_INVALID_ARG"):
            make_client(serial).tap(999, 999)

    def test_raises_on_generic_overlong_error(self):
        serial = ScriptedSerial({"INFO": "UITEST: ERR command too long\n"})
        with self.assertRaisesRegex(RuntimeError, "command too long"):
            make_client(serial).info()

    def test_times_out_without_matching_reply(self):
        serial = ScriptedSerial({"INFO": "UITEST: OK TAP 1 2\n"})
        with self.assertRaisesRegex(RuntimeError, "timeout"):
            make_client(serial, timeout=0.001).info()

    def test_dump_returns_nodes(self):
        node = "UITEST: NODE d:0 id:- x:0 y:0 w:320 h:240 click:0 hidden:0"
        crc = zlib.crc32(f"{node}\n".encode("ascii")) & 0xFFFFFFFF
        serial = ScriptedSerial({
            "DUMP": (
                "---UITEST_DUMP_START---\n"
                f"{node}\n"
                f"UITEST: DUMP CRC32:{crc:08x} COUNT:1 TRUNCATED:0\n"
                "---UITEST_DUMP_END---\n"
            )
        })
        nodes = make_client(serial).dump()
        self.assertEqual(len(nodes), 1)

    def test_dump_reports_device_error(self):
        serial = ScriptedSerial({
            "DUMP": (
                "---UITEST_DUMP_START---\n"
                "UITEST: ERR DUMP ESP_ERR_NO_MEM\n"
                "---UITEST_DUMP_END---\n"
            )
        })
        with self.assertRaisesRegex(RuntimeError, "ESP_ERR_NO_MEM"):
            make_client(serial).dump()

    def test_dump_rejects_crc_mismatch(self):
        serial = ScriptedSerial({
            "DUMP": (
                "---UITEST_DUMP_START---\n"
                "UITEST: NODE d:0 id:- x:0 y:0 w:1 h:1 click:0 hidden:0\n"
                "UITEST: DUMP CRC32:00000000 COUNT:1 TRUNCATED:0\n"
                "---UITEST_DUMP_END---\n"
            )
        })
        with self.assertRaisesRegex(RuntimeError, "CRC32"):
            make_client(serial).dump()

    def test_dump_rejects_malformed_node(self):
        serial = ScriptedSerial({
            "DUMP": (
                "---UITEST_DUMP_START---\n"
                "UITEST: NODE broken\n"
                "UITEST: DUMP CRC32:00000000 COUNT:1 TRUNCATED:0\n"
                "---UITEST_DUMP_END---\n"
            )
        })
        with self.assertRaisesRegex(RuntimeError, "malformed"):
            make_client(serial).dump()

    def test_rejects_unsafe_or_long_commands(self):
        client = make_client(ScriptedSerial())
        with self.assertRaises(ValueError):
            client.command("INFO\nTAP 1 2")
        with self.assertRaises(ValueError):
            client.command("X" * (uitest_runner.MAX_COMMAND_BYTES + 1))


if __name__ == "__main__":
    unittest.main()