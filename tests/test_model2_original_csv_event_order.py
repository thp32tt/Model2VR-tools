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

    def test_corrupt_original_event_order_fails_closed_before_source_admission(self):
        cases = (
            ("duplicate seq", 2, 0, "1"),
            ("missing seq", 2, 0, "4"),
            ("noninitial seq", 1, 0, "2"),
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
                          "extra_column", "truncated_row"):
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
