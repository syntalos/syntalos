Syntalos
========

[![Syntalos Screenshot](contrib/screenshots/v0.8.4-ui-overview.png "Syntalos")](https://github.com/bothlab/syntalos/tree/master/contrib/screenshots)
![Build Test](https://github.com/bothlab/syntalos/workflows/Build%20Test/badge.svg?branch=master)

Syntalos (formerly known as MazeAmaze) is a software for timestamp-synchronized parallel data acquisition from diverse data sources,
such as cameras, microendoscopes, Intan electrophysiology amplifiers or Firmata-based serial interfaces.
The software also allows user-defined closed-loop interventions via its built-in Python scripting support.
It is specifically designed for use in (neuro)scientific in vivo behavior experiments.

Syntalos is built with a set of core principles in mind:
 * Timestamps of all data sources of an experiment should be synchronized (within tolerance limits), so data at
   specific points in time can be directly compared. If hardware synchronization is unavailable, a software solution is used.
 * A data acquistion task must not block a different acquistion or processing task.
 * Data is stored in a fixed directory structure (Experiment Directory Layout, EDL) with all metadata alongside the data.
 * The software must account for experimenter error and have sane failure modes (autocorrect bad experimenter input, never have a component fail silently, ...)
 * The software must never auto-adjust parameters without logging the fact
 * Syntalos is Linux-native: It is written to be run on Linux, which enables it to make use of some beneficial Linux-specific functionality
   to increase its robustness or performance (a Windows port is likely possible, but would need extensive testing).

## Users

<a href="https://flathub.org/apps/io.github.bothlab.syntalos">
<img src="https://flathub.org/assets/badges/flathub-badge-en.png" width="140"/>
</a>

You can install Syntalos directly [from your App-Center](https://flathub.org/apps/io.github.bothlab.syntalos)
if the Flathub repository is set up on your Linux system.

We also provide prebuilt binaries as native packages as well as more detailed installation instructions
[in the Syntalos documentation](https://syntalos.readthedocs.io/en/latest/install.html).
The documentation also contains information how to best use Syntalos.

To make Syntalos work for your experimental setup, you can either create new modules and integration code, or
utilize already existing modules to acquire and transform data without any required coding!
You can find a list of built-in modules below.

### Modules

Syntalos modules are self-contained entities which can perform arbitrary data acquisition, processing and/or storage tasks.
All modules are supervised and driven by the Syntalos Engine and can communicate with each other using data streams.

Currently, the following modules are built-in or can be enabled at build-time:
 * *audiosource*: Play various acoustic signals.
 * *canvas*: An OpenGL-based display for single images and image streams. Will display frame times and framerates for interactive monitoring.
 * *camera-flir*: Use (USB) cameras from [FLIR Systems](https://www.flir.com/) which can be addressed via their Spinnaker SDK.
 * *camera-generic*: Use any camera compatible with UVC or the OpenCV videocapture abstraction (e.g. simple webcams).
 * *camera-tis*: Use a (USB) industrial camera from [The Imaging Source](https://www.theimagingsource.com/) to acquire a video stream.
 * *camera-ueye*: Record video with an uEye industrial camera from [IDS](https://ids-imaging.com) (this module is unmaintained!).
 * *deeplabcut-live*: Example Python module for live animal tracking using [DeepLabCut-live](https://github.com/DeepLabCut/DeepLabCut-live).
 * *firmata-io*: Control a (wired) serial device that speaks the [Firmata](http://firmata.org/wiki/Main_Page) protocol, usually an Arduino.
   This module can be controlled with a custom user script via the Python module.
 * *firmata-userctl*: Manually send commands to a *firmata-io* module to change pin states using a simple GUI.
 * *intan-rhx*: Use an [RHD2000 USB Interface Board](http://intantech.com/RHD2000_USB_interface_board.html) by [IntanTech](http://intantech.com/)
   for biopotential recordings of up to 256 channels.
 * *jsonwriter*: Write data to Pandas-compatible JSON files.
 * *miniscope*: Perform calcium imaging in awake, freely moving animals using devices from the [UCLA Miniscope Project](https://github.com/Aharoni-Lab/Miniscope-v4/wiki).
 * *plot-timeseries*: Plot signal traces in real time.
 * *pyscript*: Run arbitrary Python 3 code and send automation commands to other modules in the active experiment.
 * *runcmd*: Run any external command when the experiment was started.
 * *table*: Display & record tabular data in a standardized CSV format.
 * *sp210-pressuresense*: Support for the SP210 differential pressure sensor driven by a Raspberry Pi Pico.
 * *triled-tracker*: Track an animal via three LEDs mounted on a headstage and save various behavior parameters.
 * *videorecorder*: Record image streams from cameras to video files in various formats.
 * *videotransformer*: Perform common transformations on frames, such as cropping & scaling.

## Developers

This section is for everyone who wants to build Syntalos from source, or wants to change its code
to submit a change, bugfix or new module.

### Dependencies

 * C++17 compatible compiler
   (GCC >= 7.1 or Clang >= 4. GCC is recommended)
 * Meson (>= 0.64)
 * Qt5 (>= 5.12)
 * Qt5 Test
 * Qt5 OpenGL
 * Qt5 SVG
 * Qt5 Remote Objects
 * Qt5 SerialPort
 * GLib (>= 2.58)
 * Eigen3
 * [TOML++](https://github.com/marzer/tomlplusplus/)
 * FFmpeg (>= 4.1)
 * GStreamer (>= 1.0)
 * PipeWire
 * OpenCV (>= 4.1)
 * KF5 Archive
 * KF5 TextEditor
 * [pybind11](https://github.com/pybind/pybind11)
 * libusb (>= 1.0)
 * ImGui / ImPlot (optional, needed for plotting)

We recommend Debian 12 (Bookworm) or Ubuntu 22.04 (Jammy Jellyfish) to build & run Syntalos,
but any Linux distribution that has a recent enough C++ compiler and Qt version
should work.
On Ubuntu, you can get some updated dependencies by adding the Syntalos PPA: `sudo add-apt-repository -y ppa:ximion/syntalos`

Some modules may require additional dependencies on libraries to communicate with hardware devices, or to implement
their repective features.
In case you get a dependency error when running `meson`, install the missing dependency or try to build with less modules enabled.

Before attempting to build Syntalos, ensure all dependencies (and their development files) are installed on your system.
If you are using Debian or Ubuntu, you may choose to locally run the system package installation script that the CI system uses:
`sudo ./tests/ci/install-deps-deb.sh`. *IMPORTANT:* This script runs APT with fsync/sync disabled to speed up package installation,
but this leaves the system vulnerable to data corruption in case of an unexpected power outage or other issues during installation.
If you are concerned by this, please install the packages mentioned in the script file manually.

After installing all dependencies, you should be able to build the software after configuring the build for your platform using Meson:
```sh
mkdir build && cd build
meson --buildtype=debugoptimized -Doptimize-native=true ..
ninja
sudo ninja install
```

Modules can be enabled and disabled via the `-Dmodules` flag - refer to `meson_options.txt` for a list of possible,
comma-separated values.

Pull-requests for new modules, bugfixes or any changes are very welcome!
(Code should be valid C++17, use 4 spaces for indentation)
