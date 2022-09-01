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
