"""Original Win32 x86 source CSV event-order integrity regression.

Use entirely synthetic rows; never package private Daytona source captures.
"""
import csv
import tempfile
import unittest
from pathlib import Path

from tools.model2_draw_source_report import audit_csv


class OriginalCSVEventOrderTests(unittest.TestCase):
    HEADER = ["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"]
    ROWS = [
        ["1", "2500", "41", "synthetic_probe_unknown", "0", "0", "0", "0"],
        ["2", "2500", "42", "synthetic_probe_unknown", "0", "0", "0", "0"],
        ["3", "2501", "41", "synthetic_probe_unknown", "0", "0", "0", "0"],
    ]

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
