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
src/datactl/  EDL storage, data types, timestamp/sync primitives. Public API. Qt-free.
src/mlink/    IPC layer for out-of-process (OOP) modules, utility code for OOP modules. Public API. Qt-free.
src/python/   pybind11 bindings for MLink API (pysy-mlink). Public API.
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
(`syclock.h`, `timesync.h`), stream data types (`datatypes.h`), and UUID/system utilities.
This library has no Qt dependency and is safe to link from both Qt and non-Qt components.

**MLink / IPC** (`src/mlink/`) - Out-of-process module support using iceoryx2 (zero-copy shared-memory
IPC). The `MLinkModule` base class in `src/fabric/mlinkmodule.h` is the master-side counterpart that
manages the worker process and bridges IPC channels to the engine's module graph.
The MLink library is public API, has no Qt dependency, and also contains convenience methods for
building Syntalos out-of-process modules that may or may not live in-tree.

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

**Logging** — Library modules get a per-module `QuillLogger *m_log` member (set up automatically).
Use the Quill log macros with this logger:
```cpp
LOG_INFO(m_log, "Loaded device: {}", deviceName);
LOG_WARNING(m_log, "Unexpected value: {}", val);
LOG_ERROR(m_log, "Failed to open port: {}", portId);
LOG_DEBUG(m_log, "Frame {} received", frameCount);
```
Do not use `qDebug()` / `qWarning()` / `qCritical()` in modules; those bypass the logging subsystem.
For code outside a module class that still needs a logger, use `getLogger()` from `fabric/logging.h`.

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

In code, the module inherits `SyntalosLinkModule` from `<syntalos-mlink>` and registers ports in its
constructor. Ports are discovered dynamically by the master side via IPC.
Prefer the `OrAbort` variants for port registration in constructors; they terminate
immediately with an error message if registration fails.

---

#### 3. Python MLink modules (`type = "python"`)

Run out-of-process as a standalone Python script. The process is persistent: it starts on module
initialization, runs the script immediately, and stays alive between experiment runs.

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

The script is a self-contained Python program with a `main()` entry point. Call `syl.init_link()`
once early to obtain the `SyntalosLink` object, register all ports on it, attach callbacks, then
enter the event loop:

```python
import sys
import syntalos_mlink as syl

def main() -> int:
    syLink = syl.init_link()

    # Register ports at init time so Syntalos knows the topology.
    iport = syLink.register_input_port('frames-in', 'Frames', syl.DataType.Frame)
    oport = syLink.register_output_port('rows-out', 'Results', syl.DataType.TableRow)

    # Register lifecycle and settings callbacks, then hand over to the event loop.
    syLink.on_prepare = ...
    syLink.on_start   = ...
    syLink.on_stop    = ...
    syLink.on_save_settings = ...  # serialize settings to bytes
    syLink.on_load_settings = ...  # deserialize settings from bytes

    syLink.await_data_forever()    # signals IDLE and blocks until shutdown
    return 0

if __name__ == '__main__':
    sys.exit(main())
```

Key points:
- Port registration happens in `__init__` (or at module level) - the master side reads
  the topology immediately after the script starts.
- `iport.on_data` is the preferred way to receive data via a callback.
- `on_save_settings` / `on_load_settings` both receive a `baseDir: pathlib.Path` argument with the
   directory where the `.syct` file will be stored. Use this sparingly for external data.
- If the module shows a Qt GUI, create the `QApplication` before calling `init_link()` and pass
  `app.processEvents` to `await_data_forever()` so the GUI stays responsive.

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
All supported data types are defined in `src/datactl/datatypes.h`:
`Frame`, `TableRow`, `ControlCommand`, `FirmataData`, `FloatSignalBlock`, `IntSignalBlock`, etc.

Stream metadata is carried as `MetaStringMap` (a `std::map<std::string, MetaValue>`) where
`MetaValue` is a `std::variant`. These types are defined in `src/datactl/streammeta.h`.
Use `setMetadataVar()` on output ports to publish metadata; read it via `metadataValue()` /
`metadataValueOr<T>()` on input ports.

In C++ library modules, register ports in the module constructor:
```cpp
m_inPort  = registerInputPort<Frame>("frames-in", "Frames");
m_outPort = registerOutputPort<TableRow>("rows-out", "Results");
```

**Dormant ports** — A port (or its connected stream) can be declared dormant for the current run.
A dormant output port will not publish any data; a connected input port that reads from a dormant
stream will also appear dormant. Dormancy propagates transitively across connections, so a module
that outputs nothing can automatically silence downstream modules that depend solely on its data.
Use `StreamOutputPort::setDormant(true)` in `prepare()` to mark a port inactive for the upcoming
run, and `AbstractModule::setStateDormant()` to signal that the module itself is dormant.
Check `VarStreamInputPort::isDormant()` in a module's `prepare()` to react to upstream dormancy.

### Data Flow

1. The engine calls `prepare()` on all modules (allocate resources, validate connections).
2. On `start()`, each module's `runThread()` executes in its own thread.
3. Data flows through typed, buffered stream connections between ports.
4. Timestamp sync is maintained via a global `SyncTimer`; modules receive a `SynchronizerStrategy` to
   handle hardware or software sync.
5. All data is written to an EDL directory tree with TOML metadata sidecars.

### IPC for Out-of-Process (MLink) Modules

Out-of-process modules (both executable and Python) communicate with the engine via iceoryx2
shared-memory IPC. The master side is `MLinkModule` (`src/fabric/mlinkmodule.h`); the worker side
uses `SyntalosLink` / `SyntalosLinkModule` from `<syntalos-mlink>`. Channels are keyed on
`<module-id>_<index>/<channel-name>`.

Key control channels (defined in `src/mlink/ipc-types-private.h`):
- `ERROR_CHANNEL_ID` - worker → master error events
- `STATE_CHANNEL_ID` - worker → master state changes
- `IN_PORT_CHANGE_CHANNEL_ID` / `OUT_PORT_CHANGE_CHANNEL_ID` - port registration (executable modules)
- `SAVE_SETTINGS_CALL_ID` / `LOAD_SETTINGS_CALL_ID` - master ↔ worker settings serialization RPC
- `PREPARE_RUN_CALL_ID`, `START_CALL_ID`, `STOP_CALL_ID` - master → worker lifecycle RPC
- `SHOW_SETTINGS_CALL_ID`, `SHOW_DISPLAY_CALL_ID` - master → worker UI RPC

**Async start** - By default the engine calls `start()` on all OOP modules in parallel so that
initial acquisition timestamps are as closely aligned as possible. Modules that have circular
metadata dependencies and modify stream metadata in `start()` (rare!) must opt out by calling
`slink->setAllowAsyncStart(false)` (C++) or setting `modLink.allow_async_start = False` (Python)
before the first run. When async-start is disabled for a module, the engine waits for its `start()`
to complete before proceeding to the next module.

### Data Routing Topology

Data routing between modules is non-trivial because in-process (library) and out-of-process (MLink)
modules must interoperate. Below is a description of every combination and the machinery behind it.

#### Background: participants in an IOX data service

Each iceoryx2 data service has a fixed capacity (`max_publishers`, `max_subscribers`, `max_nodes`).
`makeIpcServiceTopology()` (`src/mlink/ipc-types-private.h`) pre-computes these limits from
connection counts.

#### Case 1 - Library → Library

No IPC involved. Both modules share the same address space; the output port's `DataStream` carries data
in-process via `StreamSubscription` objects. The `StreamExporter` is not involved.

#### Case 2 - Library → MLink (non-MLink source, MLink destination)

The **`StreamExporter`** (`src/fabric/streamexporter.cpp`) bridges an in-process stream to iceoryx2.
During engine preparation, `StreamExporter::publishStreamByPort(iport)` is called for each input
port of every MLink destination module:

1. The exporter creates an IOX **publisher** on service `<src-mod-id>/<channel-id>`.
2. It subscribes to the source port's `DataStream` in-process.
3. Its event-loop thread reads each `StreamSubscription` and forwards the raw bytes to the IOX publisher.
4. The destination MLink worker receives a `ConnectInputRequest` (via `markIncomingForExport`) and
   opens an IOX **subscriber** on that service, receiving data.

If the same source output port has multiple MLink consumers, only one iceoryx publisher is created
(iceoryx handles fan-out). Additional destination subscriptions become "drain-only" entries that are
suspended immediately after the first dispatch so their in-process queues do not fill up.

#### Case 3 - MLink → Library (MLink source, non-MLink destination)

The MLink worker process owns the IOX **publisher**. The master must bring data back
into the in-process stream graph so that the library destination module can read it:

1. `MLinkModule::registerOutPortForwarders()` (`src/fabric/mlinkmodule.cpp`) creates an IOX
   **subscriber** on the source worker's output service, attached to the master's IOX node.
2. The master's `runThread()` waits on a `WaitSet`; when data arrives it calls
   `ps.oport->streamVar()->pushRawData(...)` to push the bytes into the in-process `DataStream`.
3. Library destination modules read that stream normally via their `StreamSubscription`.

#### Case 4 - MLink → MLink (MLink source, all destinations are also MLink)

Both ends have IOX workers. The master forwarder from Case 3 is **not created** - it would
receive data just to push it into in-process queues that nobody reads.

`registerOutPortForwarders()` checks `oport->subscriberPorts()` (see below) and skips forwarder
creation when every connected input port's owner is an `MLinkModule`.

The destination workers still connect directly to the source's iceoryx service via
`ConnectInputRequest` (sent by `markIncomingForExport`), so data flows worker-to-worker with
no master involvement in the data path.

The in-process `StreamSubscription` objects that were created when connections were drawn in the GUI
still exist. The `StreamExporter` detects the MLink source in `publishStreamByPort()` and registers a
**drain-and-suspend** entry for each such subscription so it is suspended after the first dispatch
and never accumulates data.

#### Case 5 - MLink → Mixed (MLink source, some MLink and some library destinations)

This is a combination of Cases 3 and 4:

1. The master forwarder subscriber **is** created so that library destinations can receive
   data via the in-process `DataStream`.
2. MLink destinations still connect directly to the source's IOX service via `ConnectInputRequest`.
3. Their in-process `StreamSubscription` objects would accumulate unread data. The `StreamExporter`
   registers drain-and-suspend entries for them (same mechanism as Case 4) to prevent memory
   buildup and false "hot connection" readings.

## Important Conventions

- Code lives in the `Syntalos` namespace.
- New functions should prefer C++23's std::expected for error handling over throwing exceptions / bool returns + error state variables.
- Never fail silently - modules must surface errors via `raiseError()`, no silent returns.
- Modules must not block each other; long-running work goes in the module's own thread.
- `runThread()` must periodically check `m_running` (or `waitCondition`) to allow clean shutdown.
- `runThread()` is not a `QThread` - never use GUI from it or assume signals/slots work. Use `std::mutex` and atomic operations if needed.
- Settings are serialized as structured `QVariantHash` and/or `QByteArray` for library modules via `serializeSettings` / `loadSettings`.
  Out-of-process modules (executable and Python) serialize to an opaque `ByteVector` blob. The master side
  requests serialization and deserialization from the worker via the `SaveSettings` / `LoadSettings` IPC RPC
  calls (see `src/mlink/ipc-types-private.h`). Python modules register `syLink.on_save_settings` /
  `syLink.on_load_settings` to participate; if unregistered, no settings are saved.
- Use `processUiEvents()` (not raw `QCoreApplication::processEvents()`) inside long-running
  `initialize()` paths that show dialogs.
- Modules that may be run in a Flatpak sandbox must not assume `/usr/`-relative paths;
  use helpers from `fabric/utils/misc.h` like `hostUdevRuleExists()` to check for required host files.
