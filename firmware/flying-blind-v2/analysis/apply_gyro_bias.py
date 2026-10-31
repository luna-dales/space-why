import csv
import json
import math
import statistics
import sys
from pathlib import Path

from analyze_csv import COLUMNS

GYRO_COLUMNS = ["gx_radps", "gy_radps", "gz_radps"]


def apply_bias(source, profile_path):
    profile = json.loads(profile_path.read_text(encoding="utf-8"))

    if profile["units"] != "rad/s":
        raise ValueError("Calibration units must be rad/s.")

    if profile["source_file"] == source.name:
        raise ValueError("Use an independent validation recording.")

    offsets = {
        column: float(profile["offsets"][column])
        for column in GYRO_COLUMNS
    }

    if not all(math.isfinite(value) for value in offsets.values()):
        raise ValueError("Calibration contains invalid offsets.")

    with source.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        if reader.fieldnames != COLUMNS:
            raise ValueError("Unexpected CSV columns.")

        rows = list(reader)

    if len(rows) < 2:
        raise ValueError("At least two samples are needed.")

    corrected_columns = [
        f"{column}_corrected" for column in GYRO_COLUMNS
    ]

    for line_number, row in enumerate(rows, start=2):
        if None in row or any(
            row[column] in (None, "") for column in COLUMNS
        ):
            raise ValueError(f"Missing or extra fields on line {line_number}.")

        if not all(math.isfinite(float(row[column])) for column in COLUMNS):
            raise ValueError(f"Invalid number on line {line_number}.")

        for column in GYRO_COLUMNS:
            row[f"{column}_corrected"] = (
                float(row[column]) - offsets[column]
            )

    output = (
        Path(__file__).resolve().parent
        / f"{source.stem}-gyro-corrected.csv"
    )

    # Exclusive creation protects an existing derived dataset.
    with output.open("x", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(
            file, fieldnames=COLUMNS + corrected_columns
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"Validation recording: {source.name}")
    print(f"Calibration profile: {profile_path.name}")
    print(f"Calibration recording: {profile['source_file']}")
    print(f"Samples: {len(rows)}")
    print("\nAxis,raw_mean,offset,corrected_mean,corrected_sample_stddev")

    for column in GYRO_COLUMNS:
        raw = [float(row[column]) for row in rows]
        corrected = [row[f"{column}_corrected"] for row in rows]

        print(
            f"{column},"
            f"{statistics.mean(raw):.9f},"
            f"{offsets[column]:.9f},"
            f"{statistics.mean(corrected):.9f},"
            f"{statistics.stdev(corrected):.9f}"
        )

    print(f"\nSaved: {output}")
    print("Units: rad/s. All samples retained.")
    print("Raw source and calibration profile were not changed.")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(
            "Usage: python analysis/apply_gyro_bias.py "
            "data/SEN006.CSV analysis/SEN004-gyro-bias.json"
        )

    try:
        apply_bias(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, KeyError, TypeError, csv.Error) as error:
        sys.exit(f"ERROR: {error}")