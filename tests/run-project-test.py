#!/usr/bin/env python3
#
# Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Syntalos Simple GUI Test Framework

This script runs tests based on TOML manifests.
"""

import sys
import os
import socket
import subprocess
import argparse
import tempfile
import time
import json
import tomllib
import zstandard as zstd
import fnmatch
import numpy as np


class TermColor:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def find_free_port():
    """Return an available TCP port on the local machine."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        return s.getsockname()[1]


def validate_toml_fields(file_path, required_fields):
    """Validate that a TOML file contains specific fields."""
    try:
        with open(file_path, "rb") as f:
            content = tomllib.load(f)

        for field_path in required_fields:
            # Support nested fields like "data.media_type"
            parts = field_path.split(".")
            current = content
            for part in parts:
                if isinstance(current, dict) and part in current:
                    current = current[part]
                else:
                    return False, f"TOML field '{field_path}' not found in {file_path}"
        return True, None
    except Exception as e:
        return False, f"Failed to validate TOML file {file_path}: {e}"


def validate_json_file(file_path, required_keys=None, schema=None, min_array_length=None):
    """Validate JSON file structure and content."""
    if file_path.endswith('.zst'):
        try:
            with open(file_path, "rb") as f:
                dctx = zstd.ZstdDecompressor()
                with dctx.stream_reader(f) as reader:
                    content_str = reader.read().decode('utf-8')
                    content = json.loads(content_str)
        except Exception as e:
            return False, f"Failed to decompress JSON file {file_path}: {e}"
    else:
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = json.load(f)
        except json.JSONDecodeError as e:
            return False, f"Invalid JSON in file {file_path}: {e}"
        except Exception as e:
            return False, f"Failed to validate JSON file {file_path}: {e}"

    if required_keys:
        for key in required_keys:
            if key not in content:
                return False, f"JSON file {file_path} missing required key '{key}'"

    if min_array_length:
        for key, min_len in min_array_length.items():
            if key not in content:
                return False, f"JSON file {file_path} missing array key '{key}'"
            if not isinstance(content[key], list):
                return False, f"JSON file {file_path} key '{key}' is not an array"
            if len(content[key]) < min_len:
                return (
                    False,
                    f"JSON file {file_path} array '{key}' length {len(content[key])} is less than {min_len}",
                )

    if schema:
        # Basic schema validation: check that specified keys exist and have expected types
        for key, expected_type in schema.items():
            if key not in content:
                return False, f"JSON file {file_path} missing key '{key}' required by schema"
            if not isinstance(content[key], expected_type):
                return (
                    False,
                    f"JSON file {file_path} key '{key}' has type {type(content[key]).__name__} but expected {expected_type.__name__}",
                )

    return True, None


def _resolve_dataset(collection, name):
    """Resolve a slash-separated EDL group/dataset name to an EDLDataset.

    Returns (dataset, None) on success or (None, error_message) on failure.
    """
    segments = name.split("/")
    node = collection
    for seg in segments[:-1]:
        node = node.group_by_name(seg)
        if node is None:
            return None, f"EDL group '{seg}' (in '{name}') not found"
    dataset = node.dataset_by_name(segments[-1])
    if dataset is None:
        return None, f"EDL dataset '{name}' not found"
    return dataset, None


def _check_tsync(tsf, file_label, min_entries=0, monotonic_column=None, strict_monotonic=True):
    """Validate a parsed edlio TSyncFile (entry count + optional monotonicity)."""
    n_entries = tsf.times.shape[0]
    if n_entries < min_entries:
        return (
            False,
            f"Tsync data of {file_label} has {n_entries} entries but expected at least {min_entries}",
        )

    if monotonic_column is not None:
        labels = tsf.time_labels
        if monotonic_column not in labels:
            return (
                False,
                f"Tsync data of {file_label} has no time column '{monotonic_column}'. Found: {labels}",
            )
        col = tsf.times[:, labels.index(monotonic_column)].astype(np.int64)
        diffs = np.diff(col)
        if strict_monotonic:
            ok = bool(np.all(diffs > 0))
            adverb = "strictly increasing"
        else:
            ok = bool(np.all(diffs >= 0))
            adverb = "non-decreasing"
        if not ok:
            return False, f"Tsync column '{monotonic_column}' of {file_label} is not {adverb}"

    return True, f"{n_entries} tsync entries, labels={tsf.time_labels}"


def validate_edl_dataset(dataset, spec):
    """Validate a single EDLDataset against a manifest spec dict.

    Supported keys: 'media_type', 'tsync', 'video', 'csv'.
    Reads the data through edlio's own readers, which also self-validate file
    integrity (tsync xxh3 checksums, decodable video, ...).
    """
    name = spec.get("name", dataset.name)

    # Reject unknown keys so a typo (e.g. 'tsnyc') can't silently skip a check
    # and let the dataset pass without being validated at all.
    known_keys = {"name", "media_type", "tsync", "video", "csv"}
    unknown_keys = set(spec) - known_keys
    if unknown_keys:
        return (
            False,
            f"Dataset '{name}' spec has unknown key(s): {sorted(unknown_keys)}. "
            f"Supported: {sorted(known_keys)}",
        )

    # primary-data media type assertion
    if "media_type" in spec:
        actual = dataset.data.media_type
        if actual != spec["media_type"]:
            return (
                False,
                f"Dataset '{name}' has media type '{actual}', expected '{spec['media_type']}'",
            )

    # timesync auxiliary data
    if "tsync" in spec:
        cfg = spec["tsync"]
        try:
            tsf = next(dataset.read_aux_data("tsync"))
        except StopIteration:
            return False, f"Dataset '{name}' has no tsync auxiliary data"
        except Exception as e:
            return False, f"Failed to read tsync data of dataset '{name}': {e}"
        ok, msg = _check_tsync(
            tsf,
            f"dataset '{name}'",
            min_entries=cfg.get("min_entries", 0),
            monotonic_column=cfg.get("monotonic_column"),
            strict_monotonic=cfg.get("strict_monotonic", True),
        )
        if not ok:
            return False, msg
        print(f"    OK: dataset '{name}' tsync: {msg}")

    # video data: decode through edlio and count frames
    if "video" in spec:
        cfg = spec["video"]
        min_frames = cfg.get("min_frames", 0)
        try:
            n_frames = sum(1 for _ in dataset.read_data())
        except Exception as e:
            return False, f"Failed to read video data of dataset '{name}': {e}"
        if n_frames < min_frames:
            return (
                False,
                f"Dataset '{name}' video has {n_frames} frames but expected at least {min_frames}",
            )
        print(f"    OK: dataset '{name}' video: {n_frames} frames decoded")

    # CSV table data (edlio yields rows as list[str], ';'-delimited, row 0 = header)
    if "csv" in spec:
        cfg = spec["csv"]
        try:
            rows = list(dataset.read_data())
        except Exception as e:
            return False, f"Failed to read CSV data of dataset '{name}': {e}"
        if not rows:
            return False, f"Dataset '{name}' CSV is empty"
        headers = rows[0]
        data_rows = rows[1:]
        for header in cfg.get("expected_headers", []):
            if header not in headers:
                return (
                    False,
                    f"Dataset '{name}' CSV missing expected header '{header}'. Found: {headers}",
                )
        min_rows = cfg.get("min_rows", 0)
        if len(data_rows) < min_rows:
            return (
                False,
                f"Dataset '{name}' CSV has {len(data_rows)} data rows but expected at least {min_rows}",
            )
        print(f"    OK: dataset '{name}' CSV: {len(data_rows)} data rows")

    return True, None


def _find_edl_collection_root(export_dir):
    """Locate the EDL collection root (the dir holding the top-level manifest.toml).

    Syntalos exports into a date-named subdirectory (e.g. <export_dir>/2026-06-12/),
    so the collection root is usually one level below the export directory.
    """
    if os.path.exists(os.path.join(export_dir, "manifest.toml")):
        return export_dir
    for entry in sorted(os.listdir(export_dir)):
        sub = os.path.join(export_dir, entry)
        if os.path.isdir(sub) and os.path.exists(os.path.join(sub, "manifest.toml")):
            return sub
    return export_dir


def validate_expected_datasets(export_dir, dataset_specs):
    """Validate datasets by loading the EDL collection and reading through edlio."""
    try:
        import edlio
    except ImportError as e:
        return False, f"Cannot validate EDL datasets: required package missing: {e}"

    collection_root = _find_edl_collection_root(export_dir)
    try:
        collection = edlio.load(collection_root)
    except Exception as e:
        return False, f"Failed to load EDL collection at {collection_root}: {e}"

    for spec in dataset_specs:
        name = spec.get("name")
        if not name:
            return False, f"Dataset spec is missing the 'name' key: {spec}"
        dataset, err = _resolve_dataset(collection, name)
        if dataset is None:
            return False, err
        ok, msg = validate_edl_dataset(dataset, spec)
        if not ok:
            return False, msg

    return True, None


def path_matches(actual_path, expected_pattern):
    """Match expected output paths with optional wildcard support."""
    if fnmatch.fnmatch(actual_path, f"*/{expected_pattern}"):
        return True
    return False


def check_expected_output(export_dir, expected_files):
    if not os.path.exists(export_dir):
        return False, f"Export directory {export_dir} does not exist"

    actual_files = []
    for root, _, files in os.walk(export_dir):
        for file in files:
            actual_files.append(os.path.relpath(os.path.join(root, file), export_dir))

    for expected in expected_files:
        if isinstance(expected, str):
            # Just check if file exists
            found = False
            for actual in actual_files:
                if path_matches(actual, expected):
                    found = True
                    break
            if not found:
                return (
                    False,
                    f"Expected output file matching '{expected}' not found in {export_dir}. Found: {actual_files}",
                )
        elif isinstance(expected, dict):
            # Detailed checks
            path_pattern = expected.get("path")
            if not path_pattern:
                continue

            found_path = None
            for actual in actual_files:
                if path_matches(actual, path_pattern):
                    found_path = os.path.join(export_dir, actual)
                    break

            if not found_path:
                return (
                    False,
                    f"Expected output file matching '{path_pattern}' not found in {export_dir}. Found: {actual_files}",
                )

            # File existence check
            if not os.path.exists(found_path):
                return False, f"File {found_path} does not exist"

            # Minimum size check
            if "min_size_bytes" in expected:
                file_size = os.path.getsize(found_path)
                if file_size < expected["min_size_bytes"]:
                    return (
                        False,
                        f"File {found_path} size {file_size} bytes is smaller than expected {expected['min_size_bytes']} bytes",
                    )

            # TOML field validation
            if "toml_fields" in expected:
                success, msg = validate_toml_fields(found_path, expected["toml_fields"])
                if not success:
                    return False, msg

            # JSON validation
            if "json_validation" in expected:
                json_config = expected["json_validation"]
                required_keys = json_config.get("required_keys")
                schema = json_config.get("schema")
                min_array_length = json_config.get("min_array_length")
                success, msg = validate_json_file(
                    found_path,
                    required_keys=required_keys,
                    schema=schema,
                    min_array_length=min_array_length,
                )
                if not success:
                    return False, msg

    print(f"  Generated files: {actual_files}")
    return True, None


def print_stdout_stderr(stdout_data=None, stderr_data=None):
    if stdout_data:
        print("  --- STDOUT ---", file=sys.stderr)
        print(stdout_data, file=sys.stderr)
    if stderr_data:
        print("  --- STDERR ---", file=sys.stderr)
        print(stderr_data, file=sys.stderr)


def run_test(syntalos_bin, manifest_path):
    print(f"Running test manifest: {manifest_path}")

    with open(manifest_path, "rb") as f:
        try:
            manifest = tomllib.load(f)
        except Exception as e:
            print(f"Failed to parse TOML manifest {manifest_path}: {e}", file=sys.stderr)
            return False

    test_config = manifest.get("test", {})
    name = test_config.get("name", os.path.basename(manifest_path))
    project_file = test_config.get("project_file")
    run_duration = test_config.get("run_duration_sec", 10)
    timeout = test_config.get("timeout_sec", (run_duration * 2) + 30)
    expected_exit = test_config.get("expected_exit_code", 0)
    expected_datasets = test_config.get("expected_datasets", [])
    expected_output = test_config.get("expected_output_files", [])
    expected_stdout_contains = test_config.get("expected_stdout_contains", [])
    expected_stderr_contains = test_config.get("expected_stderr_contains", [])
    validator_script = test_config.get("validator_script")
    use_dynamic_net_ports = test_config.get("use_dynamic_net_ports", False)
    companion_cfg = test_config.get("companion_process")
    # companion_triggers_run: companion starts AFTER Syntalos (reversed order),
    # drives the run, and Syntalos is terminated once the companion finishes.
    companion_triggers_run = test_config.get("companion_triggers_run", False)

    # String-form datasets are simple manifest.toml existence checks; dict-form
    # datasets are validated through edlio (read & verify tsync/video/CSV data).
    dataset_specs = [d for d in expected_datasets if isinstance(d, dict)]
    for dataset in expected_datasets:
        if isinstance(dataset, str):
            expected_output.append(f"{dataset}/manifest.toml")

    if not project_file:
        print(f"Error: 'project_file' not specified in {manifest_path}", file=sys.stderr)
        return False

    # Resolve project_file relative to the manifest directory
    manifest_dir = os.path.dirname(os.path.abspath(manifest_path))
    project_path = os.path.join(manifest_dir, project_file)

    if not os.path.exists(project_path):
        print(f"Error: Project file {project_path} not found.", file=sys.stderr)
        return False

    # resolve dynamic network ports if requested
    net_control_port = None
    net_feedback_port = None
    if use_dynamic_net_ports:
        net_control_port = find_free_port()
        net_feedback_port = find_free_port()
        print(f"  Dynamic net ports: control={net_control_port}, feedback={net_feedback_port}")

    sytmp_prefix = "sytest_{}_".format(str(name).replace(" ", "").replace("/", "."))
    with tempfile.TemporaryDirectory(dir="/var/tmp", prefix=sytmp_prefix) as export_dir:
        cmd = [syntalos_bin, "--verbose", "--non-interactive", "--export-dir", export_dir]
        if not companion_triggers_run:
            # Normal mode: Syntalos controls its own run duration
            cmd += ["--run-for", str(run_duration)]
        if net_control_port is not None:
            cmd += ["--net-control-port", str(net_control_port)]
        if net_feedback_port is not None:
            cmd += ["--net-feedback-port", str(net_feedback_port)]
        cmd.append(project_path)

        print(f"  Project: {project_file}")
        print(f"  Command: {' '.join(cmd)}")
        print(f"  Expected exit code: {expected_exit}")
        print(f"  Timeout: {timeout}s (Run duration: {run_duration}s)")
        if expected_output:
            print(f"  Expected output files: {expected_output}")

        start_time = time.time()
        companion_proc = None
        companion_stdout = ""
        companion_stderr = ""

        try:

            def build_companion_cmd():
                companion_script = companion_cfg.get("script")
                if not companion_script:
                    return None, "companion_process.script not specified"
                companion_path = os.path.join(manifest_dir, companion_script)
                if not os.path.exists(companion_path):
                    return None, f"Companion script not found: {companion_path}"
                c_cmd = [sys.executable, companion_path]
                if net_control_port is not None:
                    c_cmd += ["--cmd-port", str(net_control_port)]
                if net_feedback_port is not None:
                    c_cmd += ["--fb-port", str(net_feedback_port)]
                return c_cmd, None

            startup_wait = companion_cfg.get("startup_wait_sec", 1.0) if companion_cfg else 0

            if companion_cfg and not companion_triggers_run:
                # Normal order: companion starts first, then Syntalos
                companion_cmd, err = build_companion_cmd()
                if err:
                    print(f"  [!] {err}", file=sys.stderr)
                    return False
                print(f"  Starting companion: {' '.join(companion_cmd)}")
                companion_proc = subprocess.Popen(
                    companion_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
                )
                time.sleep(startup_wait)

            # Start Syntalos
            process = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )

            if companion_cfg and companion_triggers_run:
                # Reversed order: Syntalos starts first; companion drives the run
                companion_cmd, err = build_companion_cmd()
                if err:
                    print(f"  [!] {err}", file=sys.stderr)
                    process.kill()
                    process.communicate()
                    return False
                time.sleep(startup_wait)
                print(f"  Starting companion controller: {' '.join(companion_cmd)}")
                companion_proc = subprocess.Popen(
                    companion_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
                )

            if companion_triggers_run:
                # Wait for the companion to finish, then terminate Syntalos
                companion_remaining = timeout
                try:
                    companion_stdout, companion_stderr = companion_proc.communicate(
                        timeout=companion_remaining
                    )
                except subprocess.TimeoutExpired:
                    print(f"  [!] Companion controller timed out", file=sys.stderr)
                    companion_proc.kill()
                    companion_stdout, companion_stderr = companion_proc.communicate()
                    process.kill()
                    process.communicate()
                    return False

                if companion_proc.returncode != 0:
                    print(
                        f"  [!] Companion controller failed with exit code {companion_proc.returncode}",
                        file=sys.stderr,
                    )
                    print_stdout_stderr(companion_stdout, companion_stderr)
                    process.kill()
                    process.communicate()
                    return False

                # Give Syntalos a moment to finalize data, then terminate it
                time.sleep(2.0)
                process.terminate()
                try:
                    stdout_data, stderr_data = process.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    stdout_data, stderr_data = process.communicate()
                exit_code = 0  # exit code not meaningful when we terminate Syntalos
            else:
                try:
                    stdout_data, stderr_data = process.communicate(timeout=timeout)
                    exit_code = process.returncode
                except subprocess.TimeoutExpired:
                    print(f"  [!] Test timed out after {timeout} seconds", file=sys.stderr)
                    process.kill()
                    process.communicate()
                    if companion_proc:
                        companion_proc.kill()
                        companion_proc.communicate()
                    return False

            # wait for companion to finish after Syntalos exits (normal order only)
            if companion_proc and not companion_triggers_run:
                companion_remaining = max(5, timeout - int(time.time() - start_time))
                try:
                    companion_stdout, companion_stderr = companion_proc.communicate(
                        timeout=companion_remaining
                    )
                except subprocess.TimeoutExpired:
                    print("  [!] Companion process timed out - killing", file=sys.stderr)
                    companion_proc.kill()
                    companion_stdout, companion_stderr = companion_proc.communicate()

            runtime = time.time() - start_time
            print(f"  Finished in {runtime:.2f}s with exit code {exit_code}")

            if exit_code != expected_exit:
                print(
                    f"  [!] Exit code mismatch: expected {expected_exit}, got {exit_code}",
                    file=sys.stderr,
                )
                print_stdout_stderr(stdout_data, stderr_data)
                return False

            if expected_stdout_contains:
                for text in expected_stdout_contains:
                    if text not in stdout_data:
                        print(f"  [!] Expected text not found in STDOUT: '{text}'", file=sys.stderr)
                        print_stdout_stderr(stdout_data)
                        return False

            if expected_stderr_contains:
                for text in expected_stderr_contains:
                    if text not in stderr_data:
                        print(f"  [!] Expected text not found in STDERR: '{text}'", file=sys.stderr)
                        print_stdout_stderr(None, stderr_data)
                        return False

            # validate companion process output
            if companion_cfg:
                companion_expected_stdout = companion_cfg.get("expected_stdout_contains", [])
                if companion_proc and companion_proc.returncode != 0:
                    print(
                        f"  [!] Companion process exited with code {companion_proc.returncode}",
                        file=sys.stderr,
                    )
                    print_stdout_stderr(companion_stdout, companion_stderr)
                    return False
                for text in companion_expected_stdout:
                    if text not in companion_stdout:
                        print(
                            f"  [!] Expected text not found in companion STDOUT: '{text}'",
                            file=sys.stderr,
                        )
                        print_stdout_stderr(companion_stdout, companion_stderr)
                        return False
                if companion_stdout:
                    print("  Companion output:")
                    for line in companion_stdout.splitlines():
                        print(f"    {line}")

            if expected_output:
                success, msg = check_expected_output(export_dir, expected_output)
                if not success:
                    print_stdout_stderr(stdout_data, stderr_data)
                    print(f"  [!] Output validation failed: {msg}", file=sys.stderr)
                    return False
                else:
                    print("  [+] Output validation passed")

            if dataset_specs:
                success, msg = validate_expected_datasets(export_dir, dataset_specs)
                if not success:
                    print_stdout_stderr(stdout_data, stderr_data)
                    print(f"  [!] Dataset validation failed: {msg}", file=sys.stderr)
                    return False
                else:
                    print("  [+] Dataset validation passed")

            if validator_script:
                validator_path = os.path.join(manifest_dir, "validators", validator_script)
                if not os.path.exists(validator_path):
                    print(f"  [!] Validator script not found: {validator_path}", file=sys.stderr)
                    return False
                print(f"  Running validator: {validator_script}")
                val_result = subprocess.run(
                    [sys.executable, validator_path, export_dir],
                    capture_output=True,
                    text=True,
                )
                if val_result.returncode != 0:
                    print_stdout_stderr(stdout_data, stderr_data)
                    print("\n")
                    print(f"  [!] Validator {validator_script} failed:", file=sys.stderr)
                    if val_result.stdout:
                        print(val_result.stdout, file=sys.stderr)
                    if val_result.stderr:
                        print(val_result.stderr, file=sys.stderr)
                    return False
                else:
                    if val_result.stdout:
                        print(val_result.stdout.rstrip())
                    print(f"  [+] Validator {validator_script} passed")

            print(f"  [+] Test {name} passed!")
            return True

        except Exception as e:
            print(f"  [!] Error running test: {e}", file=sys.stderr)
            if companion_proc and companion_proc.poll() is None:
                companion_proc.kill()
                companion_proc.communicate()
            return False


def check_display():
    has_display = os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")
    if not has_display:
        print(
            "  [!] WARNING: No display detected. Tests may fail.",
            file=sys.stderr,
        )


def main():
    parser = argparse.ArgumentParser(
        description="Run Syntalos project tests based on TOML manifests"
    )
    parser.add_argument("--syntalos-bin", required=True, help="Path to the Syntalos executable")
    parser.add_argument(
        "--manifest", action="append", required=True, help="Path to the test manifest TOML file(s)"
    )
    args = parser.parse_args()

    if not os.path.exists(args.syntalos_bin):
        print(f"Error: Syntalos executable not found at {args.syntalos_bin}", file=sys.stderr)
        sys.exit(1)

    # print a warning if there is no GUI output
    check_display()

    failed = 0
    passed = 0
    for manifest_path in args.manifest:
        if run_test(args.syntalos_bin, manifest_path):
            passed += 1
        else:
            failed += 1

    if passed + failed == 1:
        text = f"{TermColor.OKGREEN}PASSED" if passed >= 1 else f"{TermColor.FAIL}FAILED"
        print(f"\n{text}{TermColor.ENDC}")
    else:
        print("\n--- Summary ---")
        print(f"Passed: {passed}")
        print(f"Failed: {failed}")
        print(f"Total:  {passed + failed}")

    if failed > 0:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
