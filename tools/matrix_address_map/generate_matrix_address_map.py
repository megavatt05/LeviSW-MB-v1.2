#!/usr/bin/env python3
"""Validate the 80-apartment commissioning CSV and emit a C address-map header."""

from __future__ import annotations

import argparse
import csv
from datetime import date
import json
from pathlib import Path
import sys
from typing import Any

REQUIRED_COLUMNS = (
    "apartment",
    "decade_d",
    "unit_e",
    "d_test_point",
    "e_test_point",
    "d_connector_pin",
    "e_connector_pin",
    "continuity_verified",
    "operator",
    "reviewer",
    "verification_date",
    "pcb_revision",
    "harness_revision",
    "commissioning_record",
    "notes",
)

COMMON_METADATA = (
    "operator",
    "reviewer",
    "verification_date",
    "pcb_revision",
    "harness_revision",
    "commissioning_record",
)


def _required(row: dict[str, str], field: str, row_number: int) -> str:
    value = (row.get(field) or "").strip()
    if not value:
        raise ValueError(f"CSV row {row_number}: field '{field}' is required")
    return value


def _integer(row: dict[str, str], field: str, row_number: int) -> int:
    value = _required(row, field, row_number)
    try:
        return int(value, 10)
    except ValueError as exc:
        raise ValueError(
            f"CSV row {row_number}: '{field}' must be a decimal integer"
        ) from exc


def _c_string(value: str) -> str:
    if not value.isascii() or any(ord(ch) < 0x20 for ch in value):
        raise ValueError("C map metadata must contain printable ASCII only")
    return json.dumps(value, ensure_ascii=True)


def load_and_validate(csv_path: Path) -> tuple[dict[str, str], list[tuple[int, int]]]:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        columns = tuple(reader.fieldnames or ())
        missing = [column for column in REQUIRED_COLUMNS if column not in columns]
        if missing:
            raise ValueError("CSV is missing required columns: " + ", ".join(missing))
        rows = list(reader)

    if len(rows) != 80:
        raise ValueError(f"CSV must contain exactly 80 apartment rows; got {len(rows)}")

    by_apartment: dict[int, tuple[int, int]] = {}
    used_crosspoints: set[tuple[int, int]] = set()
    common: dict[str, str] | None = None

    for index, row in enumerate(rows, start=2):
        apartment = _integer(row, "apartment", index)
        if apartment < 1 or apartment > 80:
            raise ValueError(f"CSV row {index}: apartment must be in range 1..80")
        if apartment in by_apartment:
            raise ValueError(f"CSV row {index}: duplicate apartment {apartment}")

        decade = _integer(row, "decade_d", index)
        unit = _integer(row, "unit_e", index)
        if not 0 <= decade <= 7:
            raise ValueError(f"CSV row {index}: decade_d must be in range 0..7")
        if not 0 <= unit <= 9:
            raise ValueError(f"CSV row {index}: unit_e must be in range 0..9")

        expected_d_tp = f"TP{27 + decade}"
        expected_e_tp = f"TP{35 + unit}"
        actual_d_tp = _required(row, "d_test_point", index).upper()
        actual_e_tp = _required(row, "e_test_point", index).upper()
        if actual_d_tp != expected_d_tp:
            raise ValueError(
                f"CSV row {index}: D{decade} must use {expected_d_tp}, not {actual_d_tp}"
            )
        if actual_e_tp != expected_e_tp:
            raise ValueError(
                f"CSV row {index}: E{unit} must use {expected_e_tp}, not {actual_e_tp}"
            )

        _required(row, "d_connector_pin", index)
        _required(row, "e_connector_pin", index)

        continuity = _required(row, "continuity_verified", index).lower()
        if continuity not in {"1", "true", "yes", "pass"}:
            raise ValueError(
                f"CSV row {index}: continuity_verified must be YES/TRUE/1/PASS"
            )

        metadata = {
            field: _required(row, field, index)
            for field in COMMON_METADATA
        }
        if common is None:
            common = metadata
        elif metadata != common:
            raise ValueError(
                f"CSV row {index}: commissioning metadata differs between rows"
            )

        crosspoint = (decade, unit)
        if crosspoint in used_crosspoints:
            raise ValueError(
                f"CSV row {index}: duplicate D{decade} x E{unit} crosspoint"
            )
        used_crosspoints.add(crosspoint)
        by_apartment[apartment] = crosspoint

    missing_apartments = sorted(set(range(1, 81)) - set(by_apartment))
    if missing_apartments:
        raise ValueError(
            "CSV is missing apartments: "
            + ", ".join(str(value) for value in missing_apartments)
        )
    if len(used_crosspoints) != 80:
        raise ValueError("Every D/E crosspoint must be used exactly once")

    assert common is not None
    if common["operator"].casefold() == common["reviewer"].casefold():
        raise ValueError("reviewer must be a different person from operator")
    try:
        date.fromisoformat(common["verification_date"])
    except ValueError as exc:
        raise ValueError("verification_date must use ISO format YYYY-MM-DD") from exc

    ordered = [by_apartment[apartment] for apartment in range(1, 81)]
    return common, ordered


def generate_header(metadata: dict[str, str],
                    coordinates: list[tuple[int, int]]) -> str:
    lines = [
        "/* Generated from the reviewed matrix commissioning CSV. DO NOT EDIT. */",
        "#ifndef LEVISW_MB_V1_2_APARTMENT_MAP_GENERATED_H",
        "#define LEVISW_MB_V1_2_APARTMENT_MAP_GENERATED_H",
        "",
        '#include "matrix_manager.h"',
        "",
        "static const matrix_manager_address_map_t levisw_mb_v1_2_apartment_map = {",
        "    .version = MATRIX_MANAGER_ADDRESS_MAP_VERSION,",
        "    .verified = true,",
        f'    .pcb_revision = {_c_string(metadata["pcb_revision"])},',
        f'    .harness_revision = {_c_string(metadata["harness_revision"])},',
        f'    .commissioning_record = {_c_string(metadata["commissioning_record"])},',
        "    .apartments = {",
    ]
    for apartment, (decade, unit) in enumerate(coordinates, start=1):
        lines.append(
            f"        [{apartment - 1}U] = "
            f"{{ .decade = {decade}U, .unit = {unit}U }}, /* flat {apartment} */"
        )
    lines.extend([
        "    },",
        "};",
        "",
        "#endif /* LEVISW_MB_V1_2_APARTMENT_MAP_GENERATED_H */",
        "",
    ])
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate the 80-flat matrix map CSV and generate a C header."
    )
    parser.add_argument("csv_file", type=Path, help="completed commissioning CSV")
    parser.add_argument("output_header", type=Path, help="generated C header path")
    args = parser.parse_args(argv)

    try:
        metadata, coordinates = load_and_validate(args.csv_file)
        content = generate_header(metadata, coordinates)
        args.output_header.parent.mkdir(parents=True, exist_ok=True)
        args.output_header.write_text(content, encoding="utf-8", newline="\n")
    except (OSError, ValueError) as exc:
        print(f"matrix map generation failed: {exc}", file=sys.stderr)
        return 2

    print(
        f"Generated verified map for 80 flats "
        f"(PCB {metadata['pcb_revision']}, harness {metadata['harness_revision']}, "
        f"record {metadata['commissioning_record']})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
