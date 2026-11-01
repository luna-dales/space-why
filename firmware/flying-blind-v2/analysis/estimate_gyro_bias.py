# Estimate a fixed gyro bias from one stationary recording and save a JSON
# profile beside this script. The source CSV remains unchanged.
# Usage: python analysis/estimate_gyro_bias.py data/SEN004.CSV
# A profile must be tested on independent data before being trusted.
import csv
import json
import math
import statistics
import sys
from pathlib import Path

GYRO_COLUMNS = ["gx_radps", "gy_radps", "gz_radps"]


# Reads only raw gyro columns; the board must actually have been stationary.
# Numeric checks cannot distinguish a constant rotation from a fixed bias.
def estimate_bias(source):
    with source.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        # Accept any layout containing the raw gyro fields, but do not use
        # corrected columns to estimate another correction.
        if not reader.fieldnames or not all(
            column in reader.fieldnames for column in GYRO_COLUMNS
        ):
            raise ValueError("Required gyro columns are missing.")

        rows = []

        for line_number, record in enumerate(reader, start=2):
            if None in record:
                raise ValueError(f"Extra fields on line {line_number}.")

            row = {}

            for column in GYRO_COLUMNS:
                value = float(record[column])

                if not math.isfinite(value):
                    raise ValueError(f"Invalid value on line {line_number}.")

                row[column] = value

            rows.append(row)

    if len(rows) < 2:
        raise ValueError("At least two samples are needed.")

    # Arithmetic means use ALL samples. Movement and startup transients can
    # bias these estimates; this script does not automatically discard them.
    offsets = {
        column: statistics.mean(row[column] for row in rows)
        for column in GYRO_COLUMNS
    }

    # Sample standard deviations describe within-recording variation, not
    # uncertainty across temperatures, power cycles, or different assemblies.
    deviations = {
        column: statistics.stdev(row[column] for row in rows)
        for column in GYRO_COLUMNS
    }

    # Store provenance, units, correction convention, and experimental status
    # with the numbers. A matching filename is not a content checksum or a
    # guarantee that two recordings were collected independently.
    profile = {
        "source_file": source.name,
        "sample_count": len(rows),
        "units": "rad/s",
        "method": "Arithmetic mean of all samples; board assumed stationary",
        "correction": "corrected = raw - offset",
        "offsets": offsets,
        "sample_standard_deviations": deviations,
        "status": "Experimental; independent validation pending",
    }

    output = Path(__file__).resolve().parent / f"{source.stem}-gyro-bias.json"

    # Preserve previously used profiles. This existence check is intended
    # for a single-user workflow, not concurrent profile-generation processes.
    if output.exists():
        raise ValueError(
            f"{output.name} already exists. Inspect it before replacing it."
        )

    # JSON retains the numerical means; six-decimal terminal output below
    # is presentation rounding only. No firmware constants are updated here.
    output.write_text(
        json.dumps(profile, indent=2) + "\n",
        encoding="utf-8"
    )

    for column, offset in offsets.items():
        print(f"{column} offset: {offset:.6f} rad/s")

    print(f"Samples used: {len(rows)}")
    print(f"Saved: {output}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(
            "Usage: python analysis/estimate_gyro_bias.py data/SEN004.CSV"
        )

    try:
        estimate_bias(Path(sys.argv[1]))
    except (OSError, ValueError, TypeError, csv.Error) as error:
        sys.exit(f"ERROR: {error}")