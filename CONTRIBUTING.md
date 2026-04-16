# Contributing to Syntalos

## Ways to contribute

There are many ways to contribute to Syntalos. You can [submit a bug report](https://github.com/syntalos/syntalos/issues),
help with [documentation and the website](https://github.com/syntalos/syntalos-web) or create new modules for your experiment
setup and share them with the community.
We also have a [discussion forum](https://github.com/syntalos/syntalos/discussions) if you have any questions.

## About this document

This document is for developers who want to contribute to the Syntalos codebase itself - either to fix bugs, add features,
or write new modules that ship with the project. It is not for people who just want to use the software or develop simple
standalone modules for their own experiment. Please see [the documentation](https://syntalos.org/docs/) for that.

As of 2026, this document has also been expanded with information that is relevant for AI systems (like the location
of files, and direct code and macro pointers).

## What is Syntalos

Syntalos is a Linux-native application for timestamp-synchronized parallel data acquisition from diverse sources
(cameras, electrophysiology amplifiers, Firmata-based serial interfaces, etc.), designed for scientific experiments,
particularly in-vivo behavior recordings.
All data is stored in a fixed directory structure called EDL (Experiment Directory Layout).

## Build System

Syntalos uses Meson (>= 1.4.0) and requires GCC >= 14 or Clang >= 19, and C++23 support.

```sh
# Install dependencies (Debian/Ubuntu only)
sudo ./tests/ci/install-deps-deb.sh

# Configure and build
mkdir build && cd build
meson setup --buildtype=debugoptimized -Doptimize-native=true ..
ninja

# Install
sudo ninja install
```

Optional modules are controlled via `-Dmodules=module1,module2,...`.
See `meson_options.txt` for the full list of optional modules.

Development-only flags in `meson_options.txt`:
- `-Dmaintainer=true` - strict compiler flags (`-Werror`)
- `-Dsanitize=address|leak|thread|undefined` - enable various sanitizers
- `-Dtracing=true` - enable `-pg` profiling
- `-Dgui-tests=true` - enable GUI project tests

Modules share a common Meson boilerplate defined in `modules/modsetup.meson`.
If you add a new module or change the shared definition, regenerate it:
```sh
./modules/update-symod-meson.py
```

## Testing

```sh
# Run all unit tests
cd build && meson test --print-errorlogs

# Run a single test by name
cd build && meson test sy-test-basic

# Run end-to-end GUI scenario test manually
./tests/run-project-test.py <scenario-dir>
```

Unit tests (Qt Test framework) are in `tests/*.cpp`.
Scenario-based integration tests live in `tests/scenarios/` as TOML manifests.

## Code Formatting

```sh
# Auto-format all C++ and Python code
./autoformat.py
```

- **C++**: clang-format (LLVM style, 4-space indent, 120-column limit) - config in `.clang-format`
- **Python**: Black (line length 100, Python 3.11+ target)
- **Indentation**: 4 spaces everywhere, no tabs

## Architecture Overview

### Repository Layout

```
src/          Core application (engine, GUI, module library, loaders). Private API.
src/fabric/   Module API for libraries, streams, shared (GUI) utilities. Private API.
src/datactl/  EDL storage, data types, timestamp/sync primitives. Public API.
src/mlink/    IPC layer for out-of-process (OOP) modules, utility code for OOP modules. Public API.
src/python/   pybind11 bindings for MLink API (pysy-mlink). Public API.
src/utils/    Shared utilities (TOML parsing, misc helpers). Private API.
modules/      One subdirectory per module.
tests/        Unit tests (Qt Test) and integration test scenarios.
tools/        Developer tools and useful utilities (crash reporter, metaview, etc.)
contrib/      Packaging (Debian, Flatpak) and miscellanous items.
vendor/       Vendored dependencies
```

### Core Components

**Engine** (`src/engine.h/cpp`) - Central orchestrator. Manages the module graph lifecycle
(`prepare` → `start` → run → `stop`), resource monitoring, and experiment metadata writing.

**Fabric** (`src/fabric/`) - Defines the module API (`moduleapi.h`), typed data streams
(`streams/`), port connections, and the `AbstractModule` base class. All in-process modules inherit
from `AbstractModule` and register via `SYNTALOS_DECLARE_MODULE` / `SYNTALOS_MODULE`.
The fabric library is linked into both the core application and shared-library modules; it is *not*
exposed to out-of-process (MLink/Python) modules.
It contains code that both library modules and the core application want to share / need access to.

**Module Library** (`src/modulelibrary.cpp`) - Discovers and loads all three module types at startup
by iterating module directories and reading each `module.toml`. Dispatches to the appropriate loader:
- `loadLibraryModInfo` → in-process shared-library modules (`type=library`)
- `moduleloader-py` → Python out-of-process modules (`type=python`)
- `moduleloader-ext` → compiled executable out-of-process modules (`type=executable`)

**Data Control** (`src/datactl/`) - EDL storage format, timestamp synchronization
(`syclock.h`, `timesync.h`), and stream data types (`datatypes.h`).

**MLink / IPC** (`src/mlink/`) - Out-of-process module support using iceoryx2 (zero-copy shared-memory
IPC). The `MLinkModule` base class in `src/fabric/mlinkmodule.h` is the master-side counterpart that
manages the worker process and bridges IPC channels to the engine's module graph.
The MLink library is public API and also contains convenience methods for building Syntalos out-of-process
modules that may or may not live in-tree.

**Python Bindings** (`src/python/`) - pybind11 bindings exposing the Syntalos-MLink API to Python
modules (`pysy-mlink`). Uses `cvnp` for zero-copy OpenCV ↔ NumPy frame sharing.

**GUI** (`src/mainwindow.cpp`, `src/flowgraphview.cpp`) - Qt main window with a flow-graph editor for
connecting modules visually.

### Module Types

Every module lives in `modules/<module-id>/` and is described by a `module.toml` file. There are
three kinds, selected by the `type` key.
A module ID is its unique, lower-case name (e.g. `zarrwriter`), while a module name is the human-readable
name that will be displayed in the UI (e.g. `Zarr Writer`).

---

#### 1. C++ library modules (`type = "library"`)

Compiled as a shared library (`.so`), loaded in-process at startup. They have full access to the
fabric API and run in the same address space as the engine.

```
modules/example-cpp/
  meson.build        # shared_library target; links syntalos_fabric_dep
  examplemodule.h    # declares ModuleInfo subclass + SYNTALOS_DECLARE_MODULE
  examplemodule.cpp  # implements ModuleInfo and AbstractModule subclass
  example-cpp.svg    # icon
```

`module.toml` for a library module:
```toml
[syntalos_module]
type    = "library"
main    = "libexample-cpp.so"   # relative path to the .so inside the module dir
```

The `ModuleInfo` subclass is the factory; `AbstractModule` is the runtime instance.
Key overrideable lifecycle methods on `AbstractModule`:
- `initialize()` - called once when the module is added to a project board
- `prepare(testSubject)` - allocate resources, validate port connections
- `start()` - called immediately before data acquisition begins
- `runThread(startWaitCondition)` - runs in a dedicated thread during acquisition; must honour `m_running`; only called if `driver()` is `THREAD_DEDICATED`.
- `stop()` - tear down the run; always called even if the run failed
- `showSettingsUi()` / `showDisplayUi()` - open settings or display windows

Expose capabilities by returning `ModuleFeature` flags from `features()`:
`SHOW_SETTINGS`, `SHOW_DISPLAY`, `REALTIME`, `CALL_UI_EVENTS`, `REQUEST_CPU_AFFINITY`, `PROHIBIT_CPU_AFFINITY`.

**Driver kind** — Override `driver()` to tell the engine how to schedule the module.
The default is `NONE` (runs on the GUI/main thread, suitable for very lightweight or UI-only modules).

| `ModuleDriverKind` | Execution model                                    | When to use                                                                                                                                                     |
|--------------------|----------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NONE`             | GUI/main thread                                    | Trivial modules with no blocking work                                                                                                                           |
| `THREAD_DEDICATED` | One thread per instance                            | Heavy acquisition or processing loops (`runThread` must poll `m_running` and sleep/block on I/O or incoming data)                                               |
| `EVENTS_DEDICATED` | Shared thread per module type (thread-pool bucket) | Event-driven modules that process data in callbacks; instances of the same module share a thread. Call `setEventsMaxModulesPerThread(n)` to cap the bucket size |
| `EVENTS_SHARED`    | Shared thread across arbitrary module types        | Lightweight event-driven modules that can coexist freely with others                                                                                            |

Only library modules can set a dedicated driver, all other modules run as separate process with a dedicated thread handling
communication on Syntalos' master side.

---

#### 2. Compiled MLink modules (`type = "executable"`)

Compiled as a standalone executable, linked against `syntalos-mlink`. Run out-of-process in persistent
mode: the engine spawns the binary when the module is initialized, and keeps it alive between experiment
runs. The worker registers its own ports and communicates state changes back to the engine via IPC.

```
modules/example-mlink/
  meson.build         # executable target; links syntalos_mlink_dep
  module.toml         # type=executable, binary=<name>
  example-main.cpp    # inherits SyntalosLinkModule from <syntalos-mlink>
  example-mlink.svg   # icon
```

`module.toml` for an executable module:
```toml
[syntalos_module]
type        = "executable"
name        = "My Module"
description = "What it does."
icon        = "my-module.svg"
binary      = "my-module"          # executable name, resolved relative to module dir
categories  = 'category1;category2'
features    = ['show-settings']    # optional
```

In code, the module inherits `SyntalosLinkModule` and registers ports in its constructor.
Ports are discovered dynamically by the master side via IPC - no port declaration in the TOML is needed.

---

#### 3. Python MLink modules (`type = "python"`)

Run out-of-process via a shared Python worker binary. The worker is persistent: it starts on module
initialization, loads the Python script immediately, and stays alive between experiment runs (reloading
the script automatically if it changes on disk).

```
modules/example-py/
  module.toml         # type=python, main=<script>
  mod-main.py         # entry point; imports syntalos_mlink as syl
  icon.svg
  requirements.txt    # optional: PyPI packages to install in a venv
```

`module.toml` for a Python module:
```toml
[syntalos_module]
type        = "python"
name        = "My Python Module"
description = "What it does."
icon        = "icon.svg"
main        = "mod-main.py"
use_venv    = false           # set true to install requirements.txt in a venv
categories  = 'category1'
features    = ['show-settings', 'show-display']
```

Ports are registered in the Python script at module level (i.e. as top-level code that runs
when the script is first loaded), not in `module.toml`:
```python
import syntalos_mlink as syl

# Register ports at module load time so Syntalos knows the topology.
# IDs must match what get_input_port / get_output_port use later.
syl.register_input_port('frames-in', 'Frames', 'Frame')
syl.register_output_port('rows-out', 'Results', 'TableRow')
```

---

### Module Lifecycle Summary

```
initialize()        ← module added to board; start persistent worker if needed
  prepare()         ← run is about to begin; validate connections, allocate resources
    start()         ← acquisition begins
    runThread()     ← runs in dedicated thread; check m_running for clean exit
    stop()          ← run ended (normal or error); always called
  prepare() ...     ← next run
~destructor         ← module removed from board; terminate worker
```

### Ports and Streams

Modules expose typed `StreamInputPort` / `StreamOutputPort` instances.
Data types are defined in `src/datactl/datatypes.h`:
`Frame`, `TableRow`, `ControlCommand`, `FirmataData`, `FloatSignalBlock`, `IntSignalBlock`, etc.

In C++ library modules, register ports in the module constructor:
```cpp
m_inPort  = registerInputPort<Frame>("frames-in", "Frames");
m_outPort = registerOutputPort<TableRow>("rows-out", "Results");
```

### Data Flow

1. The engine calls `prepare()` on all modules (allocate resources, validate connections).
2. On `start()`, each module's `runThread()` executes in its own thread.
3. Data flows through typed, buffered stream connections between ports.
4. Timestamp sync is maintained via a global `SyncTimer`; modules receive a `SynchronizerStrategy` to
   handle hardware or software sync.
5. All data is written to an EDL directory tree with TOML metadata sidecars.

### IPC for Out-of-Process Modules

Out-of-process modules (both executable and Python) communicate with the engine via iceoryx2
shared-memory IPC. The master side is `MLinkModule` (`src/fabric/mlinkmodule.h`); the worker side
uses `SyntalosLink` / `SyntalosLinkModule` from `<syntalos-mlink>`. Channels are keyed on
`<module-id>_<index>/<channel-name>`.

Key control channels (defined in `src/mlink/ipc-types-private.h`):
- `ERROR_CHANNEL_ID` - worker → master error events
- `STATE_CHANNEL_ID` - worker → master state changes
- `IN_PORT_CHANGE_CHANNEL_ID` / `OUT_PORT_CHANGE_CHANNEL_ID` - port registration (executable modules)
- `SETTINGS_CHANGE_CHANNEL_ID` - worker → master settings updates
- `PREPARE_START_CALL_ID`, `SHOW_SETTINGS_CALL_ID`, `SHOW_DISPLAY_CALL_ID` - master → worker RPC

## Important Conventions

- Code lives in the `Syntalos` namespace.
- New functions should prefer C++23's std::expected for error handling over throwing exceptions / bool returns + error state variables.
- Never fail silently - modules must surface errors via `raiseError()`, no silent returns.
- Modules must not block each other; long-running work goes in the module's own thread.
- `runThread()` must periodically check `m_running` (or `waitCondition`) to allow clean shutdown.
- `runThread()` is not a `QThread` - never use GUI from it or assume signals/slots work. Use `std::mutex` and atomic operations if needed.
- Settings are serialized as opaque `QByteArray` blobs via `serializeSettings` / `loadSettings`.
  For out-of-process modules this blob is passed to the worker on each `prepare()` call.
- Use `processUiEvents()` (not raw `QCoreApplication::processEvents()`) inside long-running
  `initialize()` paths that show dialogs.
- Modules that may be run in a Flatpak sandbox must not assume `/usr/`-relative paths;
  use helpers from `utils/misc.h` like `hostUdevRuleExists()` to check for required host files.
