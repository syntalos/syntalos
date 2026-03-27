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
import subprocess
import argparse
import tempfile
import time
import json
import csv
import tomllib
import zstandard as zstd
import fnmatch


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


def validate_csv_file(file_path, min_rows=0, expected_headers=None):
    """Validate CSV file structure."""
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            reader = csv.reader(f, delimiter=";")
            rows = list(reader)

        if not rows:
            return False, f"CSV file {file_path} is empty"

        headers = rows[0]
        data_rows = rows[1:]

        if len(data_rows) < min_rows:
            return (
                False,
                f"CSV file {file_path} has {len(data_rows)} data rows but expected at least {min_rows}",
            )

        if expected_headers:
            for expected_header in expected_headers:
                if expected_header not in headers:
                    return (
                        False,
                        f"CSV file {file_path} missing expected header '{expected_header}'. Found: {headers}",
                    )

        # Validate first row contains data in the expected columns
        if len(data_rows) > 0 and expected_headers:
            first_row = data_rows[0]
            for expected_header in expected_headers:
                col_idx = headers.index(expected_header)
                if col_idx >= len(first_row) or first_row[col_idx].strip() == '':
                    return (
                        False,
                        f"CSV file {file_path} first row is missing data for column '{expected_header}'",
                    )

        return True, None
    except Exception as e:
        return False, f"Failed to validate CSV file {file_path}: {e}"


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

            # Text content check
            if "contains_text" in expected:
                try:
                    with open(found_path, "r", encoding="utf-8") as f:
                        content = f.read()
                        for text in expected["contains_text"]:
                            if text not in content:
                                return (
                                    False,
                                    f"File {found_path} does not contain expected text: '{text}'",
                                )
                except UnicodeDecodeError:
                    return (
                        False,
                        f"File {found_path} could not be read as text for 'contains_text' check",
                    )

            # TOML field validation
            if "toml_fields" in expected:
                success, msg = validate_toml_fields(found_path, expected["toml_fields"])
                if not success:
                    return False, msg

            # CSV validation
            if "csv_validation" in expected:
                csv_config = expected["csv_validation"]
                min_rows = csv_config.get("min_rows", 0)
                headers = csv_config.get("expected_headers")
                success, msg = validate_csv_file(
                    found_path, min_rows=min_rows, expected_headers=headers
                )
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
    timeout = test_config.get("timeout_sec", run_duration + 30)
    expected_exit = test_config.get("expected_exit_code", 0)
    expected_datasets = test_config.get("expected_datasets", [])
    expected_output = test_config.get("expected_output_files", [])
    expected_stdout_contains = test_config.get("expected_stdout_contains", [])
    expected_stderr_contains = test_config.get("expected_stderr_contains", [])

    # Add expected datasets as simple file existence checks
    for dataset in expected_datasets:
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

    sytmp_prefix = "sytest_{}_".format(str(name).replace(" ", "").replace("/", "."))
    with tempfile.TemporaryDirectory(dir="/var/tmp", prefix=sytmp_prefix) as export_dir:
        cmd = [
            syntalos_bin,
            "--non-interactive",
            "--export-dir",
            export_dir,
            "--run-for",
            str(run_duration),
            project_path,
        ]

        print(f"  Project: {project_file}")
        print(f"  Command: {' '.join(cmd)}")
        print(f"  Expected exit code: {expected_exit}")
        print(f"  Timeout: {timeout}s (Run duration: {run_duration}s)")
        if expected_output:
            print(f"  Expected output files: {expected_output}")

        start_time = time.time()
        try:
            process = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )

            try:
                stdout_data, stderr_data = process.communicate(timeout=timeout)
                exit_code = process.returncode
            except subprocess.TimeoutExpired:
                print(f"  [!] Test timed out after {timeout} seconds", file=sys.stderr)
                process.kill()
                process.communicate()
                return False

            runtime = time.time() - start_time
            print(f"  Finished in {runtime:.2f}s with exit code {exit_code}")

            if exit_code != expected_exit:
                print(
                    f"  [!] Exit code mismatch: expected {expected_exit}, got {exit_code}",
                    file=sys.stderr,
                )
                if stdout_data:
                    print("  --- STDOUT ---", file=sys.stderr)
                    print(stdout_data, file=sys.stderr)
                if stderr_data:
                    print("  --- STDERR ---", file=sys.stderr)
                    print(stderr_data, file=sys.stderr)
                return False

            if expected_stdout_contains:
                for text in expected_stdout_contains:
                    if text not in stdout_data:
                        print(f"  [!] Expected text not found in STDOUT: '{text}'", file=sys.stderr)
                        print("  --- STDOUT ---")
                        print(stdout_data)
                        return False

            if expected_stderr_contains:
                for text in expected_stderr_contains:
                    if text not in stderr_data:
                        print(f"  [!] Expected text not found in STDERR: '{text}'", file=sys.stderr)
                        print("  --- STDERR ---")
                        print(stderr_data)
                        return False

            if expected_output:
                success, msg = check_expected_output(export_dir, expected_output)
                if not success:
                    print(f"  [!] Output validation failed: {msg}", file=sys.stderr)
                    return False
                else:
                    print("  [+] Output validation passed")

            print(f"  [+] Test {name} passed!")
            return True

        except Exception as e:
            print(f"  [!] Error running test: {e}", file=sys.stderr)
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

    print("\n--- Summary ---")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Total:  {passed + failed}")

    if failed > 0:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
