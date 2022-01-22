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
