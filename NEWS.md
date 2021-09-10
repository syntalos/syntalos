Version 0.8.1
-------------
Released: 2021-09-10

Features:
 * Auto-switch to registered subject list if no override is set
 * Add easy way for modules to react to USB hotplug events
 * Add feature to auto-repeat an experiment run at set intervals

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
