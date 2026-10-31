import csv
import math
import statistics
import sys
from pathlib import Path

COLUMNS = [
    "sample", "start_ms", "pressure_mbar", "temperature_c",
    "ax_mps2", "ay_mps2", "az_mps2",
    "gx_radps", "gy_radps", "gz_radps",
]

CORRECTED_COLUMNS = [
    "gx_radps_corrected",
    "gy_radps_corrected",
    "gz_radps_corrected",
]

def analyze(path):
    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        columns = reader.fieldnames

        if columns not in (COLUMNS, COLUMNS + CORRECTED_COLUMNS):
            raise ValueError("CSV columns do not match a supported format.")

        rows = []

        for line_number, record in enumerate(reader, start=2):
            if None in record or any(
                record[column] in (None, "") for column in columns
            ):
                raise ValueError(f"Missing or extra fields on line {line_number}.")

            row = {column: float(record[column]) for column in columns}

            if not all(math.isfinite(value) for value in row.values()):
                raise ValueError(f"Non-finite number on line {line_number}.")

            for column in ("sample", "start_ms"):
                if row[column] < 0 or not row[column].is_integer():
                    raise ValueError(
                        f"{column} must be a nonnegative integer "
                        f"on line {line_number}."
                    )

            rows.append(row)

    if len(rows) < 2:
        raise ValueError("At least two samples are needed.")

    sample_steps = [
        current["sample"] - previous["sample"]
        for previous, current in zip(rows, rows[1:])
    ]
    intervals = [
        current["start_ms"] - previous["start_ms"]
        for previous, current in zip(rows, rows[1:])
    ]

    print(f"File: {path.name}")
    print(f"Rows: {len(rows)}")
    print(f"Columns: {len(columns)}")
    print(
        f"Sample range: {int(rows[0]['sample'])}"
        f" to {int(rows[-1]['sample'])}"
    )
    print(f"Consecutive sample numbers: {all(step == 1 for step in sample_steps)}")
    print(f"Strictly increasing timestamps: {all(dt > 0 for dt in intervals)}")
    print(
        "Interval min / mean / max (ms): "
        f"{min(intervals):.3f} / "
        f"{statistics.mean(intervals):.3f} / "
        f"{max(intervals):.3f}"
    )
    print(
        "First-to-last sample span (s): "
        f"{(rows[-1]['start_ms'] - rows[0]['start_ms']) / 1000:.3f}"
    )

    print("\nMeasurement,mean,sample_stddev,min,max")

    for column in columns[2:]:
        values = [row[column] for row in rows]
        print(
            f"{column},"
            f"{statistics.mean(values):.6f},"
            f"{statistics.stdev(values):.6f},"
            f"{min(values):.6f},"
            f"{max(values):.6f}"
        )

    magnitudes = [
        math.sqrt(sum(row[column] ** 2
                      for column in ("ax_mps2", "ay_mps2", "az_mps2")))
        for row in rows
    ]

    print(
        "\nMean acceleration magnitude (m/s^2): "
        f"{statistics.mean(magnitudes):.6f}"
    )
    print("\nAll samples retained. Values analyzed as recorded.")

    if columns == COLUMNS + CORRECTED_COLUMNS:
        print("File contains raw and firmware-corrected gyro values.")
    print("Variation includes noise, drift, and any actual movement.")
    print("Gyro means estimate offsets only if the board was stationary.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python3 analysis/analyze_csv.py data/SEN002.CSV")

    try:
        analyze(Path(sys.argv[1]))
    except (OSError, ValueError, csv.Error) as error:
        sys.exit(f"ERROR: {error}")