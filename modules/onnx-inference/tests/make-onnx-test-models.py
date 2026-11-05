#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Generate the tiny ONNX models the ONNX inference tests run.

The models are a few hundred bytes each and built at test time, so no binary
model files need to live in the repository.
"""

import os
import sys

import onnx
from onnx import TensorProto, helper

# Debian stable ships ONNX Runtime 1.21 and onnx 1.17, so we stay with what both understand
OPSET = 17
IR_VERSION = 8


def save_model(graph, out_dir, fname, metadata=None):
    model = helper.make_model(
        graph,
        opset_imports=[helper.make_opsetid("", OPSET)],
        ir_version=IR_VERSION,
        producer_name="syntalos-tests",
    )
    for key, value in (metadata or {}).items():
        entry = model.metadata_props.add()
        entry.key = key
        entry.value = value
    onnx.checker.check_model(model)
    onnx.save(model, os.path.join(out_dir, fname))


def make_identity_image(out_dir):
    """Float NCHW image in, the same tensor out. Checks adapter numbers end to end."""
    inp = helper.make_tensor_value_info("images", TensorProto.FLOAT, [1, 3, 64, 64])
    out = helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, 3, 64, 64])
    node = helper.make_node("Identity", ["images"], ["output"])
    graph = helper.make_graph([node], "identity-image", [inp], [out])
    save_model(graph, out_dir, "identity-image.onnx")


def make_identity_u8_nhwc(out_dir):
    """uint8 NHWC grayscale image in, the same tensor out."""
    inp = helper.make_tensor_value_info("pixels", TensorProto.UINT8, [1, 32, 48, 1])
    out = helper.make_tensor_value_info("same", TensorProto.UINT8, [1, 32, 48, 1])
    node = helper.make_node("Identity", ["pixels"], ["same"])
    graph = helper.make_graph([node], "identity-u8-nhwc", [inp], [out])
    save_model(graph, out_dir, "identity-u8-nhwc.onnx")


def make_channel_mean(out_dir):
    """
    Float NCHW image with dynamic height and width in, the mean of each channel out.
    Behaves like a three-class classifier whose scores are known for any input.
    """
    inp = helper.make_tensor_value_info("images", TensorProto.FLOAT, [1, 3, "height", "width"])
    out = helper.make_tensor_value_info("scores", TensorProto.FLOAT, [1, 3])
    node = helper.make_node("ReduceMean", ["images"], ["scores"], axes=[2, 3], keepdims=0)
    graph = helper.make_graph([node], "channel-mean", [inp], [out])
    save_model(
        graph,
        out_dir,
        "channel-mean.onnx",
        metadata={
            "description": "Syntalos test model",
            "names": "{0: 'red', 1: 'green', 2: 'blue'}",
        },
    )


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} OUTPUT_DIR", file=sys.stderr)
        return 1

    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    make_identity_image(out_dir)
    make_identity_u8_nhwc(out_dir)
    make_channel_mean(out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
