import csv
import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt

from analyze_csv import COLUMNS, CORRECTED_COLUMNS


def plot_recording(path):
    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)
        columns = reader.fieldnames

        if columns not in (COLUMNS, COLUMNS + CORRECTED_COLUMNS):
            raise ValueError("Unexpected CSV columns.")

        has_corrections = columns == COLUMNS + CORRECTED_COLUMNS
        rows = []

        for line_number, record in enumerate(reader, start=2):
            if None in record or any(
                record[column] in (None, "") for column in columns
            ):
                raise ValueError(
                    f"Missing or extra fields on line {line_number}."
                )

            row = {
                column: float(record[column])
                for column in columns
            }

            if not all(math.isfinite(value) for value in row.values()):
                raise ValueError(
                    f"Non-finite value on line {line_number}."
                )

            rows.append(row)

    if len(rows) < 2:
        raise ValueError("At least two samples are needed.")

    # Zero on the plot is the first saved acquisition, not startup.
    first_ms = rows[0]["start_ms"]
    times = [
        (row["start_ms"] - first_ms) / 1000
        for row in rows
    ]

    intervals = [
        current["start_ms"] - previous["start_ms"]
        for previous, current in zip(rows, rows[1:])
    ]

    if any(interval <= 0 for interval in intervals):
        raise ValueError(
            "Timestamps must increase within one session."
        )

    panel_count = 5 if has_corrections else 4

    fig, axes = plt.subplots(
        panel_count,
        1,
        figsize=(11, 3 * panel_count),
        sharex=True,
    )

    fig.suptitle(
        f"Flying Blind V2 — {path.name}",
        fontsize=16,
    )

    # Panel 1: pressure.
    axes[0].plot(
        times,
        [row["pressure_mbar"] for row in rows],
        marker=".",
        linewidth=1,
    )
    axes[0].set_ylabel("Pressure (mbar)")
    axes[0].ticklabel_format(axis="y", useOffset=False)

    # Panel 2: raw acceleration, including gravity.
    for column, label in [
        ("ax_mps2", "X"),
        ("ay_mps2", "Y"),
        ("az_mps2", "Z"),
    ]:
        axes[1].plot(
            times,
            [row[column] for row in rows],
            label=label,
            linewidth=1,
        )

    axes[1].set_ylabel("Acceleration (m/s²)")
    axes[1].legend(loc="best")

    # Panel 3: raw gyro.
    for column, label in [
        ("gx_radps", "X"),
        ("gy_radps", "Y"),
        ("gz_radps", "Z"),
    ]:
        axes[2].plot(
            times,
            [row[column] for row in rows],
            label=label,
            marker=".",
            linewidth=1,
        )

    axes[2].set_ylabel("Raw gyro (rad/s)")
    axes[2].legend(loc="best")

    # Optional panel 4: corrected gyro.
    timing_axis = axes[3]

    if has_corrections:
        for column, label in [
            ("gx_radps_corrected", "X"),
            ("gy_radps_corrected", "Y"),
            ("gz_radps_corrected", "Z"),
        ]:
            axes[3].plot(
                times,
                [row[column] for row in rows],
                label=label,
                marker=".",
                linewidth=1,
            )

        axes[3].axhline(
            0,
            color="black",
            linestyle="--",
            linewidth=0.8,
        )
        axes[3].set_ylabel("Corrected gyro (rad/s)")
        axes[3].legend(loc="best")
        timing_axis = axes[4]

    # Final panel: interval ending at each acquisition.
    timing_axis.plot(
        times[1:],
        intervals,
        marker=".",
        linewidth=1,
    )
    timing_axis.set_ylabel("Sample interval (ms)")
    timing_axis.set_xlabel(
        "Time since first saved acquisition (s)"
    )

    for axis in axes:
        axis.grid(True, alpha=0.3)

    fig.text(
        0.5,
        0.015,
        "Recorded values. All samples retained. "
        "Acceleration includes gravity.\n"
        "Corrected gyro shown when available. "
        "Gyro panels use independent vertical scales.",
        ha="center",
        fontsize=10,
    )

    fig.tight_layout(rect=(0, 0.05, 1, 0.96))

    output = (
        Path(__file__).resolve().parent
        / f"{path.stem}-plots.png"
    )

    fig.savefig(output, dpi=180)
    plt.close(fig)

    print(f"Saved: {output}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(
            "Usage: python analysis/plot_csv.py data/CAL000.CSV"
        )

    try:
        plot_recording(Path(sys.argv[1]))
    except (OSError, ValueError, csv.Error) as error:
        sys.exit(f"ERROR: {error}")