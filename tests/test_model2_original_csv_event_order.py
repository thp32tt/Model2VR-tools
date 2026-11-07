"""Original Win32 x86 source CSV event-order integrity regression.

Use entirely synthetic rows; never package private Daytona source captures.
"""
import csv
import hashlib
import io
import json
import os
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

from tools import model2_draw_source_report as source_report
from tools.model2_draw_source_report import audit_csv


class OriginalCSVEventOrderTests(unittest.TestCase):
    HEADER = ["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"]
    ROWS = [
        ["1", "2500", "41", "synthetic_probe_unknown", "0", "0", "0", "0"],
        ["2", "2500", "42", "synthetic_probe_unknown", "0", "0", "0", "0"],
        ["3", "2501", "41", "synthetic_probe_unknown", "0", "0", "0", "0"],
    ]

    def test_direct_source_report_cli_default_never_prints_private_metadata(self):
        # This lower-level original-x86 CLI is separate from the already
        # hardened seven-session comparator. All caller/nonce/ordinal data
        # must stay out of terminal transcripts unless --output is explicit.
        sentinel = "0xdeadc0de"
        full = {
            "caller_count": 1, "witness_count": 4,
            "world_pass_proof": "NOT_ESTABLISHED",
            "hmd_runtime_validation": "UNTESTED",
            "original_capture_run": {"nonce_hex": "0xabcdef0123456789"},
            "sources": [{
                "caller_rva_hex": sentinel, "original_draw_ordinal_min": 889,
                "cpu_projection_caller_rva_hex": "0xaabbccdd",
            }],
        }
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / "original.csv"
            raw.write_text("synthetic no real original data", encoding="ascii")
            output = io.StringIO()
            with mock.patch.object(source_report, "audit_csv",
                                   return_value=full):
                with redirect_stdout(output):
                    source_report.main([str(raw)])
            text = output.getvalue()
            summary = json.loads(text)
            self.assertEqual(summary["caller_count"], 1)
            self.assertEqual(summary["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertEqual(summary["hmd_runtime_validation"], "UNTESTED")
            self.assertNotIn("sources", summary)
            for forbidden in (sentinel, "0xabcdef0123456789",
                              "0xaabbccdd", "889", "nonce_hex"):
                self.assertNotIn(forbidden, text)

            destination = Path(folder) / "PRIVATE_SOURCE_REPORT.json"
            with mock.patch.object(source_report, "audit_csv",
                                   return_value=full):
                with redirect_stdout(io.StringIO()) as no_stdout:
                    source_report.main([str(raw), "--output", str(destination)])
            self.assertEqual(no_stdout.getvalue(), "")
            self.assertEqual(json.loads(destination.read_text("utf-8")), full)
            self.assertTrue(destination.read_bytes().endswith(b"\n"))

    def test_direct_private_source_report_never_overwrites_raw_csv_alias(self):
        # Existing local original bytes are never truncated by an accidental
        # --output ORIGINAL.csv or a hard-link alias to the same source.
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / "original.csv"
            original_bytes = (
                b"seq,qpc,thread,event,a0,a1,a2,a3\r\n"
                b"1,2500,41,synthetic_probe_unknown,0,0,0,0\r\n")
            raw.write_bytes(original_bytes)
            original_hash = hashlib.sha256(raw.read_bytes()).digest()
            with self.assertRaisesRegex(ValueError, "aliases original CSV"):
                source_report.main([str(raw), "--output", str(raw)])
            self.assertEqual(hashlib.sha256(raw.read_bytes()).digest(),
                             original_hash)
            alias = Path(folder) / "same-original-hardlink.json"
            os.link(raw, alias)
            with self.assertRaisesRegex(ValueError, "aliases original CSV"):
                source_report.main([str(raw), "--output", str(alias)])
            self.assertEqual(hashlib.sha256(raw.read_bytes()).digest(),
                             original_hash)

            # Full diagnostic publication is failure-atomic: an invalid
            # source cannot clobber an earlier good PRIVATE report.
            destination = Path(folder) / "PRIVATE_PREVIOUS.json"
            destination.write_text('{"old":"kept"}\n', encoding="utf-8")
            raw.write_bytes(b"broken ORIGINAL source, no schema")
            with self.assertRaises(ValueError):
                source_report.main([str(raw), "--output", str(destination)])
            self.assertEqual(destination.read_text("utf-8"),
                             '{"old":"kept"}\n')
            self.assertFalse(list(Path(folder).glob(
                "PRIVATE_PENDING_ORIGINAL_DRAW_*.json")))

    def test_direct_report_lease_covers_audit_and_private_publication(self):
        # Even on non-Windows, prove the independent entrypoint wraps the
        # entire audit AND atomic output, not merely a short preflight probe.
        from contextlib import contextmanager
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / "synthetic.csv"
            raw.write_bytes(
                b"seq,qpc,thread,event,a0,a1,a2,a3\\r\\n"
                b"1,2500,41,synthetic_probe_unknown,0,0,0,0\\r\\n")
            output = Path(folder) / "PRIVATE.json"
            live = [False]
            phases = []

            @contextmanager
            def lease(path):
                self.assertEqual(path, raw.resolve())
                self.assertFalse(live[0])
                live[0] = True
                phases.append("acquire")
                try:
                    yield
                finally:
                    live[0] = False
                    phases.append("release")

            original_write = source_report._write_private_report_atomic

            def audit(path):
                self.assertTrue(live[0])
                phases.append("audit")
                return audit_csv(path)

            def write(path, target, report):
                self.assertTrue(live[0])
                phases.append("publish")
                return original_write(path, target, report)

            with mock.patch.object(source_report,
                                   "_hold_original_x86_csv_direct_read_lease",
                                   lease), mock.patch.object(
                                       source_report, "audit_csv",
                                       side_effect=audit), mock.patch.object(
                                           source_report,
                                           "_write_private_report_atomic",
                                           side_effect=write):
                with redirect_stdout(io.StringIO()):
                    source_report.main(
                        [str(raw), "--output", str(output)])
            self.assertEqual(
                phases, ["acquire", "audit", "publish", "release"])
            self.assertEqual(json.loads(output.read_text("utf-8"))[
                "world_pass_proof"], "NOT_ESTABLISHED")
            self.assertFalse(live[0])

    @unittest.skipUnless(os.name == "nt", "requires actual Win32 sharing")
    def test_direct_report_refuses_live_writer_and_denies_reopen(self):
        # Actual Win32 handle contract; the producer remains GENERIC_WRITE
        # until emulator exit. These are synthetic bytes, not game evidence.
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        create = kernel32.CreateFileW
        create.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD,
            wintypes.HANDLE)
        create.restype = wintypes.HANDLE
        close = kernel32.CloseHandle
        close.argtypes = (wintypes.HANDLE,)
        close.restype = wintypes.BOOL
        invalid = ctypes.c_void_p(-1).value
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "native-synthetic.csv"
            path.write_bytes(
                b"seq,qpc,thread,event,a0,a1,a2,a3\\r\\n"
                b"1,2500,41,synthetic_probe_unknown,0,0,0,0\\r\\n")
            live_writer = create(str(path), 0x40000000, 1, None, 3, 0x80, None)
            self.assertNotIn(live_writer, (None, invalid))
            try:
                with self.assertRaisesRegex(ValueError, "STILL OPEN"):
                    source_report.main([str(path)])
            finally:
                self.assertTrue(close(live_writer))

            def verify_writer_stays_blocked(source):
                attempt = create(str(source), 0x40000000, 1,
                                 None, 3, 0x80, None)
                if attempt not in (None, invalid):
                    close(attempt)
                    self.fail("Direct source lease allowed a second writer")
                self.assertEqual(ctypes.get_last_error(), 32)
                return audit_csv(source)

            with mock.patch.object(source_report, "audit_csv",
                                   side_effect=verify_writer_stays_blocked):
                with redirect_stdout(io.StringIO()) as output:
                    source_report.main([str(path)])
            self.assertEqual(json.loads(output.getvalue())[
                "world_pass_proof"], "NOT_ESTABLISHED")

    def test_native_win32_logger_payload_format_matches_source_audit(self):
        # Keep the consumer in step with the real original x86 LogEvent ABI,
        # not only fabricated CSV fixtures. No emulator or ROM is required.
        producer = (Path(__file__).resolve().parents[1]
                    / "src" / "probe" / "logger.cpp").read_text(encoding="utf-8")
        self.assertIn("seq,qpc,thread,event,a0,a1,a2,a3", producer)
        self.assertIn("%llu,%lld,%lu,%.*s,%llu,%llu,%llu,%llu", producer)
        for argument in ("a0", "a1", "a2", "a3"):
            self.assertIn("static_cast<unsigned long long>(" + argument + ")",
                          producer)

    def write_rows(self, destination, rows):
        with destination.open("w", encoding="utf-8", newline="") as output:
            writer = csv.writer(output)
            writer.writerows(rows)

    def test_complete_logger_seq_and_non_decreasing_qpc_still_audit(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "synthetic.csv"
            self.write_rows(path, [self.HEADER, *self.ROWS])
            result = audit_csv(path)
            self.assertEqual(result["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertEqual(result["hmd_runtime_validation"], "UNTESTED")

    def test_native_unsigned64_payload_on_every_original_csv_event(self):
        # Real Win32 LogEvent uses %llu for all four payload fields,
        # including events that the offline source report does not consume.
        # Large legitimate bit-patterns must remain valid.
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "native-shaped-synthetic.csv"
            rows = [self.HEADER.copy(), *[v.copy() for v in self.ROWS]]
            rows[1][4:] = ["18446744073709551615", "0", "1", "4294967295"]
            self.write_rows(path, rows)
            report = audit_csv(path)
            self.assertEqual(report["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertEqual(report["hmd_runtime_validation"], "UNTESTED")

    def test_original_native_csv_wire_framing_rejects_parser_normalization(self):
        # The actual C++ logger writes literal unquoted ASCII columns and
        # CRLF-terminated rows. A generic CSV reader would accept each of
        # these non-native rewrites as the same values, falsely authenticating
        # source evidence. Every event is an ignored synthetic diagnostic.
        header = b"seq,qpc,thread,event,a0,a1,a2,a3\r\n"
        records = [
            b"1,2500,41,synthetic_probe_unknown,0,0,0,0",
            b"2,2500,42,synthetic_probe_unknown,0,0,0,0",
            b"3,2501,41,synthetic_probe_unknown,0,0,0,0",
        ]
        valid = header + b"\r\n".join(records) + b"\r\n"
        cases = {
            "quoted sequence": valid.replace(b"1,2500", b'"1",2500', 1),
            "quoted diagnostic": valid.replace(
                b"synthetic_probe_unknown", b'"synthetic_probe_unknown"', 1),
            "quoted unsigned payload": valid.replace(
                b",0,0,0,0\r\n", b',"0",0,0,0\r\n', 1),
            "utf8 signature": b"\xef\xbb\xbf" + valid,
            "missing final newline": valid[:-2],
            "blank unnumbered line": valid.replace(
                b"2,2500", b"\r\n2,2500", 1),
            "oversized physical line": valid.replace(
                b"synthetic_probe_unknown",
                b"synthetic_probe_" + b"x" * 600, 1),
            "embedded raw comma": valid.replace(
                b"synthetic_probe_unknown", b"synthetic,probe_unknown", 1),
        }
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "synthetic-original.csv"
            path.write_bytes(valid)
            self.assertEqual(audit_csv(path)["world_pass_proof"], "NOT_ESTABLISHED")
            for label, payload in cases.items():
                with self.subTest(label=label):
                    path.write_bytes(payload)
                    with self.assertRaisesRegex(
                            ValueError, "invalid original CSV native event payload|"
                            "invalid original CSV ordered event header|"
                            "not a complete Model2VR probe CSV"):
                        audit_csv(path)

    def test_native_payload_and_event_name_corruption_fails_even_if_ignored(self):
        # No original gameplay/ROM/PRIVATE bytes. The corrupt event is a
        # deliberately ignored diagnostic row, not a recognized D3D9 Draw.
        cases = (
            ("missing payload", 4, ""),
            ("negative payload", 5, "-1"),
            ("nonstandard hex", 6, "0x12"),
            ("plus sign", 7, "+1"),
            ("fractional value", 4, "1.0"),
            ("leading zeros", 5, "0002"),
            ("above uint64", 6, "18446744073709551616"),
            ("nonnumeric", 7, "NaN"),
            ("nonascii digits", 4, "１２"),
            ("whitespace", 5, " 1"),
            ("embedded csv newline", 6, "1\\n2"),
            ("empty event", 3, ""),
            ("event with spaces", 3, "bad event"),
            ("injected comma", 3, "bad,field"),
            ("event control character", 3, "bad\\nfield"),
            ("nonascii event", 3, "evént"),
            ("oversized event name", 3, "e" * 129),
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "native-shaped-synthetic.csv"
            for name, column, replacement in cases:
                with self.subTest(name=name):
                    rows = [self.HEADER.copy(), *[v.copy() for v in self.ROWS]]
                    rows[2][column] = replacement
                    self.write_rows(path, rows)
                    with self.assertRaisesRegex(
                            ValueError, "invalid original CSV native event payload"):
                        audit_csv(path)

    def test_corrupt_original_event_order_fails_closed_before_source_admission(self):
        cases = (
            ("duplicate seq", 2, 0, "1"),
            ("missing seq", 2, 0, "4"),
            ("noninitial seq", 1, 0, "2"),
            ("nonnumeric seq", 2, 0, "2.0"),
            ("negative seq", 2, 0, "-1"),
            ("seq with leading zero", 2, 0, "02"),
            ("qpc goes backwards", 3, 1, "2499"),
            ("negative qpc", 3, 1, "-1"),
            ("qpc above signed 64", 3, 1, "9223372036854775808"),
            ("nondecimal qpc", 3, 1, "NaN"),
            ("zero thread id", 3, 2, "0"),
            ("thread id above DWORD", 3, 2, "4294967296"),
            ("nonnumeric thread id", 3, 2, "2.0"),
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "synthetic.csv"
            for label, row, col, replacement in cases:
                with self.subTest(label=label):
                    data = [self.HEADER.copy()] + [v.copy() for v in self.ROWS]
                    data[row][col] = replacement
                    self.write_rows(path, data)
                    with self.assertRaisesRegex(ValueError, "original CSV ordered event header"):
                        audit_csv(path)

            for label in ("replayed", "dropped", "out_of_order",
                          "missing_qpc_header", "duplicate_header",
                          "reordered_header", "extra_column", "truncated_row"):
                with self.subTest(label=label):
                    data = [self.HEADER.copy()] + [v.copy() for v in self.ROWS]
                    if label == "replayed":
                        data.insert(2, data[1].copy())
                    elif label == "dropped":
                        data.pop(2)
                    elif label == "out_of_order":
                        data[1], data[2] = data[2], data[1]
                    elif label == "missing_qpc_header":
                        data[0].remove("qpc")
                    elif label == "duplicate_header":
                        data[0][1] = "seq"
                    elif label == "reordered_header":
                        data[0][1], data[0][2] = data[0][2], data[0][1]
                    elif label == "extra_column":
                        data[1].append("untrusted_extra")
                    elif label == "truncated_row":
                        data[1] = data[1][:6]
                    self.write_rows(path, data)
                    with self.assertRaisesRegex(
                        ValueError,
                        "original CSV ordered event header|complete Model2VR probe CSV",
                    ):
                        audit_csv(path)


if __name__ == "__main__":
    unittest.main()
