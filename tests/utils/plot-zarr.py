#!/usr/bin/env python3

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import zarr


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot Zarr arrays containing 'data' and 'timestamps'."
    )
    parser.add_argument("zarr_path", type=Path, help="Path to the Zarr group/array store")
    parser.add_argument(
        "-c",
        "--channel",
        type=int,
        default=None,
        help="Only plot one data column/channel",
    )
    parser.add_argument(
        "--max-points",
        type=int,
        default=None,
        help="Downsample to at most this many points for faster plotting",
    )
    args = parser.parse_args()

    root = zarr.open(args.zarr_path, mode="r")

    data = root["data"]
    timestamps = np.asarray(root["timestamps"]).squeeze()

    data_arr = np.asarray(data)
    if data_arr.ndim == 1:
        data_arr = data_arr[:, None]

    if len(timestamps) != data_arr.shape[0]:
        raise ValueError(
            f"timestamps length {len(timestamps)} does not match "
            f"data sample count {data_arr.shape[0]}"
        )

    step = 1
    if args.max_points is not None and len(timestamps) > args.max_points:
        step = int(np.ceil(len(timestamps) / args.max_points))

    t = timestamps[::step]
    y = data_arr[::step]

    plt.figure()

    if args.channel is not None:
        plt.plot(t, y[:, args.channel], label=f"channel {args.channel}")
    else:
        for i in range(y.shape[1]):
            plt.plot(t, y[:, i], label=f"channel {i}")

    plt.xlabel("timestamp")
    plt.ylabel("data")
    plt.title(args.zarr_path.name)
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
