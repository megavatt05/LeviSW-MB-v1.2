import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import generate_matrix_address_map as generator  # noqa: E402


def sample_rows():
    rows = []
    for apartment in range(1, 81):
        index = apartment - 1
        decade, unit = divmod(index, 10)
        rows.append({
            "apartment": str(apartment),
            "decade_d": str(decade),
            "unit_e": str(unit),
            "d_test_point": f"TP{27 + decade}",
            "e_test_point": f"TP{35 + unit}",
            "d_connector_pin": f"XP2:{decade + 1}",
            "e_connector_pin": f"XP3:{unit + 1}",
            "continuity_verified": "YES",
            "operator": "Engineer-A",
            "reviewer": "Engineer-B",
            "verification_date": "2026-11-23",
            "pcb_revision": "LeviSW-MB-v1.2",
            "harness_revision": "Harness-R2",
            "commissioning_record": "COM-2026-001",
            "notes": "",
        })
    return rows


class MatrixAddressMapGeneratorTests(unittest.TestCase):
    def write_csv(self, path: Path, rows):
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=generator.REQUIRED_COLUMNS)
            writer.writeheader()
            writer.writerows(rows)

    def test_valid_map_generates_verified_c_header(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "map.csv"
            self.write_csv(source, sample_rows())
            metadata, coordinates = generator.load_and_validate(source)
            output = generator.generate_header(metadata, coordinates)
            self.assertIn(".verified = true", output)
            self.assertIn("flat 47", output)
            self.assertIn("[46U] = { .decade = 4U, .unit = 6U }", output)
            self.assertIn('COM-2026-001', output)

    def test_duplicate_crosspoint_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "map.csv"
            rows = sample_rows()
            rows[46]["decade_d"] = rows[0]["decade_d"]
            rows[46]["unit_e"] = rows[0]["unit_e"]
            rows[46]["d_test_point"] = rows[0]["d_test_point"]
            rows[46]["e_test_point"] = rows[0]["e_test_point"]
            self.write_csv(source, rows)
            with self.assertRaisesRegex(ValueError, "duplicate D0 x E0"):
                generator.load_and_validate(source)

    def test_wrong_testpoint_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "map.csv"
            rows = sample_rows()
            rows[3]["d_test_point"] = "TP34"
            self.write_csv(source, rows)
            with self.assertRaisesRegex(ValueError, "D0 must use TP27"):
                generator.load_and_validate(source)

    def test_missing_continuity_check_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "map.csv"
            rows = sample_rows()
            rows[10]["continuity_verified"] = ""
            self.write_csv(source, rows)
            with self.assertRaisesRegex(ValueError, "continuity_verified"):
                generator.load_and_validate(source)

    def test_same_operator_and_reviewer_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "map.csv"
            rows = sample_rows()
            for row in rows:
                row["reviewer"] = row["operator"]
            self.write_csv(source, rows)
            with self.assertRaisesRegex(ValueError, "different person"):
                generator.load_and_validate(source)

    def test_missing_apartment_row_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "map.csv"
            rows = sample_rows()[:-1]
            self.write_csv(source, rows)
            with self.assertRaisesRegex(ValueError, "exactly 80"):
                generator.load_and_validate(source)


if __name__ == "__main__":
    unittest.main()
