#!/usr/bin/env python3
"""Plot p50 and p99 round-trip latency vs payload size from a summary.csv.

usage: scripts/plot.py results/latest/summary.csv [out.png]
needs: matplotlib
"""
import csv
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else src.rsplit("/", 1)[0] + "/latency.png"

    data = defaultdict(lambda: {"size": [], "p50": [], "p99": []})
    with open(src, newline="") as f:
        for row in csv.DictReader(f):
            d = data[row["transport"]]
            d["size"].append(int(row["size_bytes"]))
            d["p50"].append(float(row["p50_us"]))
            d["p99"].append(float(row["p99_us"]))

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
    for ax, metric in zip(axes, ("p50", "p99")):
        for transport, d in sorted(data.items()):
            pts = sorted(zip(d["size"], d[metric]))
            ax.plot([p[0] for p in pts], [p[1] for p in pts], marker="o", label=transport)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("payload (bytes)")
        ax.set_title(f"{metric} round-trip latency")
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("latency (µs)")
    axes[1].legend()
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
