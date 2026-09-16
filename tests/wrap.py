"""Black-box checks for column wrapping. Requires only Python's standard library."""
import json
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
ANSI = re.compile(r"\x1b\[[0-9;:]*m|\x1b\]8;[^\x07]*?(?:\x07|\x1b\\)")


class Wrapping(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "config.json"
        self.config = {
            "logo": {"type": "data", "source": "AAA\nBBB\nCCC", "padding": {"right": 2}},
            "display": {"pipe": True},
            "modules": [{"type": "custom", "key": "Test", "format": "one two three four five six seven eight nine ten"}],
        }

    def run_ff(self, *args, ok=True):
        self.path.write_text(json.dumps(self.config))
        result = subprocess.run([BINARY, "-c", str(self.path), *args], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode == 0, ok, result.stderr)
        return result.stdout

    def test_side_logo_and_continuations(self):
        output = self.run_ff("--wrap=30")
        self.assertEqual(output.splitlines(), [
            "AAA  Test: one two three four",
            "BBB      five six seven eight",
            "CCC      nine ten",
        ])

    def test_logo_exhaustion(self):
        self.config["logo"]["source"] = "AAA"
        output = self.run_ff("--wrap", "30")
        self.assertEqual(output.splitlines()[1:], ["         five six seven eight", "         nine ten"])

    def test_top_fallback_boundary(self):
        at_twenty = self.run_ff("--wrap=25").splitlines()
        self.assertTrue(at_twenty[0].startswith("AAA  Test:"))
        below_twenty = self.run_ff("--wrap=24").splitlines()
        self.assertEqual(below_twenty[:3], ["AAA", "BBB", "CCC"])
        self.assertTrue(any(line.startswith("Test:") for line in below_twenty[3:]))

    def test_right_logo(self):
        self.config["logo"]["position"] = "right"
        lines = self.run_ff("--wrap=30").splitlines()
        self.assertEqual(lines, [
            "Test: one two three four AAA",
            "    five six seven eight BBB",
            "    nine ten             CCC",
        ])
        self.config["logo"]["source"] = "AAA"
        lines = self.run_ff("--wrap=30").splitlines()
        self.assertEqual(sum("AAA" in line for line in lines), 1)

    def test_explicit_newlines(self):
        self.config["logo"] = "none"
        self.config["modules"][0] = {"type": "custom", "format": "first\n  second\n\nlast"}
        self.assertEqual(self.run_ff("--wrap=12"), "first\n  second\n\nlast\n")

    def test_narrow_unicode_and_identifiers(self):
        self.config["logo"] = "none"
        for text in ["abcdefghijklmnopqrstuvwxyz", "界界界", "e\u0301e\u0301e\u0301"]:
            self.config["modules"][0] = {"type": "custom", "format": text}
            for width in [1, 2, 4, 8]:
                output = self.run_ff(f"--wrap={width}")
                self.assertEqual("".join(output.split()), text)

    def test_key_width_and_long_key(self):
        self.config["logo"] = "none"
        self.config["modules"][0] = {"type": "custom", "key": "K", "keyWidth": 10, "format": "value"}
        self.assertEqual(self.run_ff("--wrap=20"), "K:       value\n")
        self.config["modules"][0]["key"] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        output = self.run_ff("--wrap=12")
        self.assertTrue(all(len(line) <= 12 for line in output.splitlines()))
        self.assertIn("ABCDEFGHIJKLMNOPQRSTUVWXYZ:", "".join(output.split()))

    def test_pipes_and_overrides(self):
        baseline = self.run_ff("--wrap=off")
        self.assertEqual(self.run_ff(), baseline)
        self.assertEqual(self.run_ff("--wrap"), baseline)
        self.assertEqual(self.run_ff("--wrap=auto"), baseline)
        self.config["display"]["wrap"] = 30
        self.assertNotEqual(self.run_ff(), baseline)
        self.assertEqual(self.run_ff("--wrap=off"), baseline)
        self.config["display"]["disableLinewrap"] = True
        self.assertNotEqual(self.run_ff(), baseline)

    def test_invalid_values(self):
        for value in ["", "0", "-1", "1.5", "yes", "4294967296", "999999999999999999999"]:
            self.run_ff("--wrap=" + value, ok=False)
        for value in [None, True, 0, -1, 1.5, "30", "bad", 4294967296]:
            self.config["display"]["wrap"] = value
            self.run_ff(ok=False)

    def test_json_results_and_generated_config(self):
        self.config["modules"] = ["kernel"]
        self.assertEqual(self.run_ff("--format", "json", "--wrap=10"), self.run_ff("--format", "json", "--wrap=off"))
        generated = json.loads(self.run_ff("--gen-config", "-", "--wrap=30"))
        self.assertEqual(generated["display"]["wrap"], 30)
        generated = json.loads(self.run_ff("--gen-config", "-"))
        self.assertNotIn("wrap", generated.get("display", {}))

    def test_cursor_escape_passthrough_and_separator(self):
        self.config["logo"] = "none"
        self.config["modules"] = [{"type": "custom", "format": "\x1b[20GXabcdefghijklmnop"}]
        self.assertEqual(self.run_ff("--wrap=8"), "\x1b[20GXabcdefghijklmnop\n")
        self.config["modules"] = [{"type": "separator", "string": "-", "times": 80}]
        self.assertEqual(self.run_ff("--wrap=8"), "--------\n")

    def test_color_and_hyperlink_survive(self):
        self.config["logo"] = "none"
        self.config["modules"] = [{"type": "custom", "format": "\x1b[31m\x1b]8;;https://example.org\x1b\\one two three four\x1b]8;;\x1b\\\x1b[m"}]
        output = self.run_ff("--wrap=12")
        self.assertEqual(ANSI.sub("", output), "one two\n    three\n    four\n")
        for line in output.splitlines():
            self.assertIn("\x1b[31m", line)
            self.assertIn("\x1b]8;;https://example.org\x1b\\", line)
            self.assertIn("\x1b]8;;\x1b\\", line)

    def test_image_layout_with_declared_dimensions(self):
        # These two protocols delegate decoding to the terminal; inspect placement bytes.
        image = Path(self.temp.name) / "image.png"
        import base64
        image.write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aD1sAAAAASUVORK5CYII="))
        self.config["display"]["pipe"] = False
        self.config["modules"] = [{"type": "custom", "key": "Test", "format": "visible value"}]
        for backend in ["iterm", "kitty-direct"]:
            self.config["logo"] = {"type": backend, "source": str(image), "width": 20, "height": 4,
                "position": "right", "padding": {"left": 1, "right": 2}}
            output = self.run_ff("--wrap=80")
            self.assertIn("\x1b[59G", output)
            self.assertIn("Test: visible value", ANSI.sub("", output))
            output = self.run_ff("--wrap=30")
            self.assertNotIn("\x1b[9G", output)
            self.assertIn("Test: visible value", ANSI.sub("", output))

    def test_color_blocks_and_statistics(self):
        self.config["logo"] = "none"
        self.config["modules"] = [{"type": "colors", "symbol": "block", "block": {"width": 3}}]
        output = self.run_ff("--wrap=8")
        for line in ANSI.sub("", output).splitlines():
            self.assertLessEqual(len(line), 8)
            self.assertEqual(len(line) % 3, 0)
        self.config["modules"] = [{"type": "custom", "format": "one two three four five"}]
        lines = self.run_ff("--wrap=12", "--stat", "0").splitlines()
        self.assertEqual(lines[:-1], ["one two", "    three", "    four", "    five"])
        self.assertRegex(lines[-1], r"^\s*\d+\.\d+ms$")

    @unittest.skipIf(os.name == "nt", "POSIX pipe polling")
    def test_live_top_logo_order(self):
        self.config["logo"]["position"] = "right"
        self.path.write_text(json.dumps(self.config))
        process = subprocess.Popen([BINARY, "-c", str(self.path), "--wrap=24", "--dynamic-interval", "20"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL)
        output = b""
        clear = b"\x1b[H\x1b[2J"
        try:
            deadline = time.monotonic() + 5
            while output.count(clear) < 3 and time.monotonic() < deadline:
                ready, _, _ = select.select([process.stdout], [], [], 0.2)
                if ready:
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        break
                    output += chunk
            frames = output.split(clear)
            self.assertGreaterEqual(len(frames), 4)
            for frame in frames[1:-1]:
                self.assertLess(frame.index(b"AAA"), frame.index(b"Test:"))
                self.assertEqual(frame.count(b"AAA"), 1)
        finally:
            process.terminate()
            process.communicate(timeout=5)

    @unittest.skipIf(os.name == "nt", "POSIX terminal test")
    def test_auto_terminal_and_failed_detection(self):
        import fcntl
        import pty
        import struct
        import termios

        def terminal(columns, args=()):
            self.path.write_text(json.dumps(self.config))
            master, slave = pty.openpty()
            try:
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 40, columns, 0, 0))
                process = subprocess.Popen([BINARY, "-c", str(self.path), *args], stdout=slave, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL)
                os.close(slave)
                slave = -1
                chunks = []
                while True:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError:
                        break
                    if not chunk:
                        break
                    chunks.append(chunk)
                _, error = process.communicate(timeout=10)
                self.assertEqual(process.returncode, 0, error)
                return b"".join(chunks).decode().replace("\r\n", "\n")
            finally:
                os.close(master)
                if slave >= 0:
                    os.close(slave)

        baseline = self.run_ff("--wrap=off")
        self.assertEqual(terminal(30), self.run_ff("--wrap=30"))
        self.assertEqual(terminal(0), baseline)
        self.assertEqual(terminal(0, ["--wrap=30"]), self.run_ff("--wrap=30"))
        self.assertEqual(terminal(20, ["--wrap=80"]), self.run_ff("--wrap=80"))
        self.config["display"]["disableLinewrap"] = True
        self.assertEqual(terminal(30), baseline)
        self.assertEqual(terminal(30, ["--wrap"]), self.run_ff("--wrap=30"))


if __name__ == "__main__":
    unittest.main()
