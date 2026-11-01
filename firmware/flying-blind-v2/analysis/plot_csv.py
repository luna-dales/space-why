# Render one PNG from a recorded SEN, CAL, or KAL session.
# Usage: python analysis/plot_csv.py data/KAL000.CSV
# Reads the source without changing it. The PNG is saved beside this script
# and an existing PNG of the same session name is replaced on rerun.
import csv
import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt

from analyze_csv import CORRECTED_COLUMNS, SUPPORTED_COLUMNS


# Layout grows with the schema: four base panels, one optional corrected-
# gyro panel, and two optional altitude/velocity panels. Every panel shares
# a time axis, but vertical scales are independent.
def plot_recording(path):
    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)
        columns = reader.fieldnames

        if columns not in SUPPORTED_COLUMNS:
            raise ValueError("Unexpected CSV columns.")

        # Presence is inferred only after exact schema validation. Current
        # KAL format includes the corrected-gyro columns as well.
        has_corrections = all(column in columns for column in CORRECTED_COLUMNS)
        has_kalman = "kf_altitude_m" in columns
        rows = []

        # Validate all columns, including ones not plotted, so an invalid
        # recording cannot appear healthy just because its bad field is hidden.
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

    # Do not draw a single continuous session across a timestamp reset.
    # Gaps remain visible in the timing panel rather than being interpolated.
    if any(interval <= 0 for interval in intervals):
        raise ValueError(
            "Timestamps must increase within one session."
        )

    panel_count = (5 if has_corrections else 4) + (2 if has_kalman else 0)

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
    # Show full pressure labels instead of a small residual plus an offset,
    # which is easy to misread when inspecting a screenshot.
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

    # Subtracting a fixed bias shifts the mean without reducing random noise.
    # Independent autoscaling makes that noise look larger in this panel;
    # compare numeric scales when judging raw against corrected curves.
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

    # Both altitude curves use the same session reference and y scale.
    # A smoother curve is not evidence of better physical accuracy. Velocity
    # is inferred from barometric history, not an independent IMU measurement.
    if has_kalman:
        axes[4].plot(times, [row["baro_altitude_m"] for row in rows],
                     label="Barometric altitude", marker=".", linewidth=1)
        axes[4].plot(times, [row["kf_altitude_m"] for row in rows],
                     label="Kalman altitude", linewidth=1.5)
        axes[4].set_ylabel("Relative altitude (m)")
        axes[4].legend(loc="best")
        axes[5].plot(times, [row["kf_velocity_mps"] for row in rows],
                     label="Estimated vertical velocity", linewidth=1)
        axes[5].axhline(0, color="black", linestyle="--", linewidth=0.8)
        axes[5].set_ylabel("Vertical velocity (m/s)")
        axes[5].legend(loc="best")

    # Timing is always last; negative indexing accommodates all layouts.
    timing_axis = axes[-1]
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
        "Gyro panels use independent vertical scales.\n"
        + ("Experimental barometer-only Kalman; IMU not fused."
           if has_kalman else ""),
        ha="center",
        fontsize=10,
    )

    fig.tight_layout(rect=(0, 0.05, 1, 0.96))

    # __file__ makes output location independent of the terminal directory.
    # Close the figure after saving so repeated calls do not retain figures.
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