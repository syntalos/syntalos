Version 2.1.1
-------------
Released: 2025-09-20

Features:
 * canvas: Optimize render performance
 * canvas: Improve OpenGL shader compatibility
 * camera-arv: udev: Disable powersaving on all USB cameras
 * Position module nodes in free and visible spaces, if possible

Bugfixes:
 * videorecorder: Prevent deadlock if uninitialized module is stopped prematurely
 * canvas: Do color-conversion on the CPU if we only have GLES
 * canvas: Ensure histogram is displayed correctly on GLES-only platforms
 * videotransform: Modernize code of transformers and make them more robust
 * videotransform: Fix possible data corruption when cropping, minimize deep copies
 * Fix reverse port connections (in->out) by the user when dragging in the GUI
 * camera-arv: Improve behavior if camera rescan is triggered by USB event

Miscellaneous:
 * ci: Build on Debian Testing (forky) as well
 * ppa: Only include +git part in version string if necessary

Version 2.1.0
-------------
Released: 2025-04-06

Features:
 * Port to Qt6
 * intan-rhx: Update to 3.4.0
 * Move all configuration under the new Syntalos.org umbrella
 * Allow users to globally configure IPC memory pools
 * Cross out disabled modules instead of changing their opacity

Bugfixes:
 * Prevent user interactions with the GUI while initializing slow modules
 * Prevent crash when destroying modules containing text editor views
 * Fix quirks with dynamic module loading & mlink perf in Flatpak sandboxes
 * cpp-wbench: Find libraries in a Flatpak sandbox too
 * cpp-wbench: Add missing state transition notifications to the template
 * intan-rhx: Use rpath to make the linker find new deps of lOkFrontpanel
 * debian: Fix PPA build on Ubuntu Noble

Miscellaneous:
 * logo: Make Syntalos app icon box more "Breeze-like"
 * sp210-pressuresense: Drop the Raspberry Pi Pico pictogram from the icon
 * deeplabcut-live: Update logo image
 * firmata: Make module icons a bit more generic
 * Add CITATION.cff data
 * Sync Flatpak recipe with Flathub

Version 2.0.2
-------------
Released: 2024-11-22

Features:
 * camera-arv: Display the camera ID when running
 * camera-tis: Use TcamStatistics data to derive timestamp
 * Add new ONIX Coax Commutator module

Bugfixes:
 * camera-tis: Add timeout when requesting samples & quit on error
 * camera-tis: Stop faster if frame retrieval is unreliable
 * camera-tis: Don't crash if camera connection failed and we try to save settings
 * camera-tis: Improve error reporting on pipeline start a bit
 * camera-arv: Explicitly skip loading any "DeviceTemperature" settings
 * camera-arv: Make changing gain/exposure a bit faster and more reliable
 * camera-arv: Don't pin to CPU cores and make acq threads realtime
 * camera-arv: Use Syntalos RtKit link & increase internal frame buffer
 * debian: Recommend kde-style-breeze for dark theme support
 * engine: Prevent deadlock in stream exporter when we stop too quickly
 * Fix OOM stop configuration option, it will now work again

Miscellaneous:
 * videorecorder: Don't use newly deprecated FFmpeg API, add fallback for older versions
 * engine: Don't use libusb without context

Version 2.0.1
-------------
Released: 2024-10-01

Features:
 * Drop module actions menu from API: No module is using this anymore
 * Add option to disable modules in a project without removing them
 * Allow user to define if a module failure should be fatal
 * Add "Rename" action to the modifiers menu
 * Ensure modifiers menu is disabled when module is selected & run is started
 * ui: Remember last-used Syntalos project location
 * galdur-stim: Add spike detection control support code
 * galdur-stim: Ensure we are on the right page when loading settings
 * example-py: Update Python example module
 * example-py: Demonstrate metadata forwarding

Bugfixes:
 * deb: Don't ignore module dependencies
 * intan-rhx: Make missing OpenCL platforms warning a lot less noisy
 * mlink: Always run Qt event loop when waiting for data
 * mlink: Tell modules when they are stopped, instead of terminating them
 * Fix bad time conversion for Frames transferred via IPC
 * devel-ltest: Don't crash if we are not connected to anything

Miscellaneous:
 * Add PPA package upload to release workflow
 * README: Just reference module list on the website instead of repeating it
 * Reorganize example configurations into device-dependent and independent

Version 2.0.0
-------------
Released: 2024-08-24

Notes:
 * This is the first release of the 2.0 series, which contains some significant API changes.
   Please have a look at the documentation of the Python interface specifically to adjust
   any of your custom code.
 * This release changes the used IPC mechanism, reducing its latency greatly and also allowing
   for a lot more capable out-of-process modules. If you are transferring very large chunks
   of data (e.g. excessively large images) between OOP modules (such as PyScript), you may
   encounter issues if the frequency of new data is too high.
   Please file bugs in this case, future releases will address these potential issues.
 * If you are developing modules: The Syntalos core has been broken up into multiple libraries
   and headers with stabilized API, which should make it a lot easier to develop modules.
   Please consult the C++ API documentation for more information!
 * Operations performed by many Syntalos modules should now have a lot lower latency. Give it a try!
 * Some user interface components have been refined and new modules have been introduced!
   Please have a look below for details, or explore the interface on your own.

Features:
 * videotransform: Add falsecolour/histogram-normalization transformations
 * Implement new IPC mechanism based on Iceoryx
 * Refactor and rework IPC interface and OOP module bindings
 * Rework streaming data type system
 * Refactor Syntalos core into multiple libraries
 * python: Rework Python integration, make Python module independent of pyworker
 * firmata-io: Make module run in a dedicated thread to improve latency
 * firmata-io: Use an event loop in the dedicated thread to listen for data
 * firmata-io: Update device list on USB hotplug events
 * Refactor data types, data storage and clock/sync helpers into their own shlib
 * Make port editor UI a shared UI component
 * pyscript: Use QTermWidget for Python output
 * Introduce a new module state for modules that don't participate in a run
 * cpp-workbench: Add new module for on-the-fly C++ module coding
 * engine: Allow SyThread to be backed by QThread, don't override error state
 * datactl: Add some convenience initializers for FirmataControl
 * Add categorization for modules
 * Update module selection dialog with filter & categorization
 * Categorize all modules
 * Add devel.latencytest: A simple diagnostic module to test (Firmata) latency
 * pyscript/cpp-workbench: Disable default editor save action
 * pyscript/cpp-workbench: Use toolbars instead of main menu in UI
 * Permit modules to limit port types in the port selector
 * upy-workbench: Add module to easily program a microcontroller in Python
 * upy-wbench: Use msec precision for device, but raise to µsec in Syntalos
 * canvas: Move settings into the main window view
 * canvas: Add simple image histogram display
 * Add camera-arv: Aravis-based GenICam support
 * camera-arv: Install udev rules for access to Basler cameras
 * Use higher-precision µsec timestamps for frames and data blocks
 * Use microsecond timestamps explicitly in more places
 * python: Switch from our own cv::Mat<->ndarray conversion to cvnp
 * camera-arv: Allow user access to FLIR cameras
 * galdur-stim: Add new module prototype for the old RT stim processor
 * python: Improve support for standalone Python modules
 * deeplabcut: Port to new Python API
 * python: Auto-reload main Python file of native py-modules on changes
 * ui: When filtering modules, select the first match by default
 * intan-rhx: Update to 3.3.2
 * Allow user to load a project anyway even if some settings failed to load

Bugfixes:
 * mlink: Forward output ports, fix Python type conversions
 * mlink: Make sure subscribers are allowed to block producers
 * engine: Cleanup resources a bit earlier to save memory on idle
 * Only bubble up the first error to the user per-module, to avoid spam
 * meson: Strip interfering private library linker lines from pc file
 * moduleapi: return the same port again if someone tries to register it twice
 * miniscope: Include frame index in emitted frames
 * Ensure module visual error state is preserved after a failed run
 * Replace, not extend UI lists when re-scanning serial devices
 * videorecorder: Properly handle encoder GAIN and flush last packets
 * Make module select dialog work better with fractional scaling displays
 * utils: Add trailing newline when serializing TOML tables
 * canvas: Render images with higher bit depths correctly
 * table: Don't crash if no input connection was made
 * camera-tis: Enable bidirectional time synchronization
 * camera-arv: Narrow gap between device and system timestamp via constant offset
 * Adjust clock-sync algorithm behavior to be more similar to freq-counter
 * Resolve int overflow when using hi-res times for FirmataData & in Python
 * debian: Use udev rules installed into /usr
 * timesync: clocksync: Smooth out correction offset a bit
 * timesync: Make fluke correction less aggressive, smoother and faster
 * timesync: clock: Attenuate correction speed to current framerate better
 * engine: Stop stream exporter later and safer
 * python: Improve start latency and make 'prepare' behave like in C++
 * engine: Process some UI events while waiting for threads to join
 * ui: Ensure graph view is always resized to make enough space for nodes
 * ui: Highlight last-added node after initialization
 * Try to work around cases where clone3 is blocked by seccomp
 * rtkit: Try to prefer the XDG portal over direct RtKit communication
 * intan-rhx: Fix rare crash if module is stopped before it was fully started

Miscellaneous:
 * Bump OS and compiler dependency
 * Refine library dependencies to speed up linking a bit
 * Add helper script to update shared code in module Meson definitions
 * intan-rhx: Bump default recording interval to 20min
 * modules: Disable notes for module MOC processing by default
 * Rename component-ID to reflect the software's new home
 * docs: Move documentation to separate repository
 * Update imgui
 * tests: Add some demo projects
 * metainfo: Update homepage URL
 * Rename website and ID to syntalos.org

Version 1.1.0
-------------
Released: 2024-02-22

Notes:
 * The "traceplot" module has been replaced with "plot-timeseries".
   Older projects may need a bit of manual adjustment to the new module.

Features:
 * plot-timeseries: Add efficient timeseries plotting module
 * sp210-pressuresense: Add module for the differential pressure sensor
 * Add SP210 pressure sensor firmware
 * jsonwriter: Add module to save data as compressed, Pandas-compatible JSON
 * jsonwriter: Allow encoding of NaN values as NaN
 * traceplot: Drop the module, plot-timeseries replaces it
 * Make all modules provide required metadata to work with the new timeplot module
 * plot-timeseries: Allow user to configure GUI update interval & buffer size
 * intan-rhx: Sync with upstream 3.3.1
 * intan-rhx: Try to sacrifice display refresh speed before buffer overrun
 * intan-rhx: Default to high-efficiency plotting mode
 * table: Make data visibility and data storage configurable
 * miniscope: Improve BNO block stream transfer efficiency
 * streams: Make suspension API accessible to variant streams
 * Switch to C++20
 * datasource: Add demo float signal data source
 * Load project when project file is passed as parameter

Bugfixes:
 * Fix module loader on older versions of Flatpak / Ubuntu 20.04
 * stream: Make integer sizes for signal types more clearly defined
 * engine: Protect better against wrong event API use with dead subscriptions
 * engine: Attempt emergency save in case of fatal module/thread stalls
 * videorecorder: Improve error reporting for bad metadata
 * Don't prematurely unlock UI on module error
 * Don't draw "fake" connections when user connects ports during a run
 * sysinfo: Don't overflow UI if CPU supports a lot of AVX instructions
 * Rename a few instances of "MazeAmaze"
 * intan-rhx: Work around crash in GUI race condition
 * intan-rhx: Work around widget rendering crash upon module reload on GNOME
 * intan-rhx: Improve error reporting
 * Improve UI when raising very large, complex error messages
 * Fail immediately if any two modules try to write to the same EDL dataset
 * Disable some more UI-blocking dialog actions during a run

Miscellaneous:
 * docs: Add time series plotter module documentation
 * Increase default size of module select dialog/entries a bit
 * Update dependency information
 * docs: Add a few more examples how to read data without edlio
 * Set a better initial position for managed settings windows

Version 1.0.1
-------------
Released: 2023-12-22

Features:
 * Add signal block serialization support
 * canvas: Port to modern OpenGL
 * canvas: Port display functions to work with GLES
 * canvas: Add setting to highlight saturated pixels
 * moduleapi: Auto-name windows if window convenience methods are used

Bugfixes:
 * videorecorder: Set framerate correctly if AVI container is used
 * camera-generic: Ensure pixel format is (re)set before framerate is changed
 * camera-generic: Ensure image dimensions are actually applied again
 * deb: Set build-deps, so shlibdeps are populated correctly

Miscellaneous:
 * docs: Document the tsync binary format
 * docs: Add "Windows" installation instructions
 * Make options to manage custom Python modules and venvs more obvious

Version 1.0.0
-------------
Released: 2023-08-08

Features:
 * firmata-io: Allow variable pulse lengths and improve logging
 * pyscript: Add some convenience functions for Firmata handling
 * python: syio: Make Firmata convenience functions members of OutputPort
 * pyscript: Use smaller sample script, move explanations to docs
 * miniscope: Implement support for orientation sensor
 * miniscope: Require libminiscope >= 0.5.0
 * canvas: Implement start/stop/pause controls
 * videorecorder: Disable video slicing by default

Bugfixes:
 * utils: Don't delay thread for too long in delay()
 * firmata: Ensure pin pulse lengths are clamped to a maximum length

Miscellaneous:
 * ci: Update action versions
 * Relax module ABI check slightly
 * Add example C++ module
 * Autoformat Python code
 * Autoformat C/C++ code
 * docs: Add small tutorials for beginners
 * docs: Add initial timing verification documentation
 * docs: Add simple documentation for all non-developer modules
 * Add help shortcut to online discussion page

Version 0.8.4
-------------
Released: 2023-05-26

Features:
 * Add USB connection diagram display to diagnostics toolbox
 * Display smaller status steps when saving a file
 * Allow module icons to be auto-loaded from files instead of embedded resources
 * Allow user to annotate experiment runs with custom comments
 * Drop info message when Syntalos is running in a Flatpak sandbox
 * Make user module locations Flatpak-aware
 * Add emergency fuse to stop if low on system memory
 * Add code for simple Pi Pico-based testpulse generator
 * Add simple circuit drawing for test pulse generator
 * audiosrc: Add simple audio signal generator
 * camera-tis: Port to new tcam 1.0 API
 * intan-rhx: Update to v3.2.0
 * videotransform: crop: Improve performance
 * videorecorder: Make AV1 codec work, allow hardware acceleration for it

Bugfixes:
 * Make dark/light color scheme switching work with recent Breeze versions
 * Try to break thread deadlocks in misbehaving modules
 * Set no-omit-frame-pointers explicitly when tracing
 * Stop using deprecated Python symbols and migrate to PyConfig
 * Don't complain when eventfd read fails with EAGAIN
 * Allow the direct use of PipeWire in Syntalos
 * Mark HPET as acceptable clocksource
 * Display the proper project name in title, even if it has dots
 * camera-tis: Check for presence of udev rules when module is initialized
 * camera-tis: Skip save/load of read-only properties
 * camera-generic: Allow manually overriding exposure mode as quirk setting
 * camera-generic: Don't scale settings values
 * camera-generic: Allow user to set a capture pixel format (if possible)
 * videotransform: Don't deselect the UI when launching an experiment
 * intan-rhx: Add missing break statements
 * videorecorder: Stop hardcoding render node used for VAAPI
 * videorecorder: Don't crash if deferred encoder initialization fails
 * videorecorder: Make combined hardware & deferred encoding work properly

Miscellaneous:
 * Apply some stricter compile-time checks
 * Move module icons out of global resources and load from files
 * timesync: Adjust times a bit more slowly
 * timesync: Re-fill buffer vector completely after time adjustment/calibration
 * timesync: intan: Don't wait too long after time adjustment
 * Update rwqueue implementation to latest upstream
 * docs: Update install instructions, add Flatpak info
 * intan-rhx: Silence warnings due to upstream code issues
 * intan-rhx: Reduce time sync tolerance slightly
 * canvas: Slightly modernize OpenGL
 * videorecorder: Only show model name in DRI device selection

Version 0.8.3
-------------
Released: 2022-09-01

Features:
 * Add initial code to make Syntalos play well with Flatpak sandboxes
 * canvas: Allow for smaller minimum display window size
 * Implement crash reporter tool
 * crashreport: Add a special mode to debug freezes as well
 * videorecorder: Allow using the AV1 codec again
 * Simplify elided label code and make it accessible to modules
 * miniscope: Display device name for camera ID
 * sysinfo: Allow user to easily copy system information
 * runcmd: Allow running commands on host if in Flatpak sandbox
 * intan-rhx: Verify udev rules are present and warn the user if not
 * intan-rhx: Synchronize with upstream Intan RHX 3.1.0
 * Provide pre-rendered logo icons in addition to vector graphic
 * debian: Build separate syntalos-hwsupport package
 * Include hardware support package install state in sysinfo dialog
 * Allow loading modules from user locations
 * Add a few quicklaunch actions for useful tools and websites

Bugfixes:
 * table: Adjust module icon to dark mode
 * Defer error emission of module errors emitted while running to shutdown
 * canvas: Try even harder to resume image display when we are too slow
 * Find Numpy in more places
 * videotransform: Improve UI a bit and validate crop ROI earlier
 * videotransform: Enforce lower width/height limits
 * debian: Pull in gstreamer1.0-plugins-bad to ensure TIS cameras work
 * Make keyboard-selecting entries possible in all list views
 * engine: Defer storage group destruction until all modules are stopped
 * crashreport: Add hack to grab ptrace permission when analyzing freeze
 * engine: Steal EDL storage groups only after all mod-threads have joined
 * videorecorder: Copy frame before color conversion to prevent crash
 * videorecorder: Use OpenCV's own code for encoder input alignment
 * videorecorder: Unconditionally run pixel format conversion
 * videorecorder: vaapi: hevc: Actually apply constant-quality setting
 * videorecorder: Use gobal headers for hwaccell again
 * Fix build with FFmpeg 5.x
 * Fix rare ordering-related build failure
 * videotransform: Fix single transform entry being non-selectable post-run
 * videorecorder: Try a few more times to reach a new encoder process
 * videorecorder: Fix wrong orientation of encode progress bar with recent Qt5
 * table: Raise window correctly via double-click on module box
 * Ensure leftover temporary data in /var/tmp is cleaned up on reboot
 * Ensure ephemeral run data is not stored on a tmpfs-backed temporary location
 * Apply uaccess tag to udev rules

Miscellaneous:
 * canvas: Reduce display fps accuracy
 * intan-rhx: Tweak defaults for Syntalos
 * metainfo: Automatically add release information
 * Show Flatpak-needs-testing message when run as Flatpak bundle
 * Update Syntalos logo design

Version 0.8.2
-------------
Released: 2022-01-22

Features:
 * py: Allow floats to be emitted as table values
 * deeplabcut: Add very simple settings UI
 * Ensure more x86_64 extensions are enabled if possible and useful
 * Use very-cheap vectorization cost model only when it's available
 * timesync: Unconditionally write the last timestamp pair to tsync files
 * Allow intervals of less than a minute for interval runs
 * camera-tis: Save & restore all device settings
 * Add support for dark themes
 * Allow user to select a dark color scheme, more darkness fixes

Bugfixes:
 * intan-rhx: Don't halt recording if display window is closed
 * intan-rhx: Udev rules should not be executable
 * intan-rhx: Give save thread some time to write the last data to disk
 * intan-rhx: Add hard barrier for tsync timestamps to not go out of bounds
 * intan-rhx: Alter initial offset calculation to be more reliable
 * py: Improve logging, fix venv support
 * py: Increase time we wait until assuming a worker has died
 * py: Show settings correctly if they are called multiple times
 * py: Allow modules to use Qt by injecting the system bindings into their venvs
 * encodehelper: Atomically replace old attributes file
 * Immediately stop interval run if error occurred or run was cancelled
 * Don't create unneeded instance copies in loops
 * Fix build with recent GCC / Glibc versions
 * Adjust to TOML++ API changes
 * timesync: counter: Consider block count when setting initial offset
 * Tweak alignment for a few icons

Miscellaneous:
 * Improve license explanation in "About" dialog
 * Use the same app version format for our EDL metadata and about dialog

Version 0.8.1
-------------
Released: 2021-09-10

Features:
 * Auto-switch to registered subject list if no override is set
 * Add easy way for modules to react to USB hotplug events
 * Add feature to auto-repeat an experiment run at set intervals
 * intan-rhx: Merge 3.0.4 upstream changes

Bugfixes:
 * Correctly list all aux data files in EDL manifests
 * Ensure experiment ID and animal ID are always trimmed
 * encodehelper: Never have two threads write to the same metadata file
 * edl: Don't write empty author email if no email is set
 * camera-flir: Fix build with recent versions of the Spinnaker SDK
 * videorecorder: Fix deprecation warning
 * Fix docs generation by ignoring even more code for API docs

Miscellaneous:
 * Mention the CI package install script in README

Version 0.8.0
-------------
Released: 2021-05-24

Notes:
 * NEWS file created
 * Project is now known as Syntalos
