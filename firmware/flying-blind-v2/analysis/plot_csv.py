import csv
import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt

from analyze_csv import COLUMNS


def plot_recording(path):
    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        if reader.fieldnames != COLUMNS:
            raise ValueError("Unexpected CSV columns.")

        rows = []

        for line_number, record in enumerate(reader, start=2):
            if None in record or any(
                record[column] in (None, "") for column in COLUMNS
            ):
                raise ValueError(f"Missing or extra fields on line {line_number}.")

            row = {column: float(record[column]) for column in COLUMNS}

            if not all(math.isfinite(value) for value in row.values()):
                raise ValueError(f"Non-finite value on line {line_number}.")

            rows.append(row)

    if len(rows) < 2:
        raise ValueError("At least two samples are needed.")

    # Plot time relative to the first saved acquisition.
    first_ms = rows[0]["start_ms"]
    times = [(row["start_ms"] - first_ms) / 1000 for row in rows]

    intervals = [
        current["start_ms"] - previous["start_ms"]
        for previous, current in zip(rows, rows[1:])
    ]

    if any(interval <= 0 for interval in intervals):
        raise ValueError("Timestamps must increase within one session.")

    fig, axes = plt.subplots(4, 1, figsize=(11, 12), sharex=True)
    fig.suptitle(f"Flying Blind V2 — {path.name}", fontsize=16)

    axes[0].plot(
        times, [row["pressure_mbar"] for row in rows],
        marker=".", linewidth=1
    )
    axes[0].set_ylabel("Pressure (mbar)")
    axes[0].ticklabel_format(axis="y", useOffset=False)

    for column, label in [
        ("ax_mps2", "X"),
        ("ay_mps2", "Y"),
        ("az_mps2", "Z"),
    ]:
        axes[1].plot(
            times, [row[column] for row in rows],
            label=label, linewidth=1
        )

    axes[1].set_ylabel("Acceleration (m/s²)")
    axes[1].legend(loc="best")

    for column, label in [
        ("gx_radps", "X"),
        ("gy_radps", "Y"),
        ("gz_radps", "Z"),
    ]:
        axes[2].plot(
            times, [row[column] for row in rows],
            label=label, marker=".", linewidth=1
        )

    axes[2].set_ylabel("Angular velocity (rad/s)")
    axes[2].legend(loc="best")

    axes[3].plot(times[1:], intervals, marker=".", linewidth=1)
    axes[3].set_ylabel("Sample interval (ms)")
    axes[3].set_xlabel("Time since first saved acquisition (s)")

    for axis in axes:
        axis.grid(True, alpha=0.3)

    fig.text(
        0.5, 0.015,
        "Raw recorded values. All samples retained. "
        "Acceleration includes gravity.",
        ha="center", fontsize=10
    )
    fig.tight_layout(rect=(0, 0.04, 1, 0.96))

    output = Path(__file__).resolve().parent / f"{path.stem}-plots.png"
    fig.savefig(output, dpi=180)
    plt.close(fig)
    print(f"Saved: {output}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python analysis/plot_csv.py data/SEN002.CSV")

    try:
        plot_recording(Path(sys.argv[1]))
    except (OSError, ValueError, csv.Error) as error:
        sys.exit(f"ERROR: {error}")