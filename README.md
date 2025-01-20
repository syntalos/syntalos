Syntalos
========

[![Syntalos Screenshot](contrib/screenshots/v0.8.4-ui-overview.png "Syntalos")](https://github.com/syntalos/syntalos/tree/master/contrib/screenshots)
![Build Test](https://github.com/syntalos/syntalos/actions/workflows/build-test.yml/badge.svg)

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

<a href="https://flathub.org/apps/org.syntalos.syntalos">
<img src="https://flathub.org/assets/badges/flathub-badge-en.png" width="140"/>
</a>

You can install Syntalos directly [from your App-Center](https://flathub.org/apps/org.syntalos.syntalos)
if the Flathub repository is set up on your Linux system.

We also provide prebuilt binaries as native packages as well as more detailed installation instructions
[in the Syntalos documentation](https://syntalos.org/docs/setup/install/).
The documentation also contains information how to best use Syntalos.

To make Syntalos work for your experimental setup, you can either create new modules and integration code, or
utilize already existing modules to acquire and transform data without any required coding!
You can find a list of built-in modules below.

### Modules

Syntalos modules are self-contained entities which can perform arbitrary data acquisition, processing and/or storage tasks.
All modules are supervised and driven by the Syntalos Engine and can communicate with each other using data streams.

You can find a list of all currently supported modules as well as their documentation [on our website](https://syntalos.org/docs/modules/).
Be aware that some modules require additional software to run, e.g. camera drivers.

### Citation

If you are using Syntalos for your research, we would be delighted if you would cite our software in your publication!
Please use the following publication in citations:

> Klumpp, M. *et al.* Syntalos: a software for precise synchronization of simultaneous multi-modal data acquisition and closed-loop interventions.
> *Nat Commun* **16**, 708 (2025).

DOI: https://doi.org/10.1038/s41467-025-56081-9

----

## Developers

This section is for everyone who wants to build Syntalos from source, or wants to change its code
to submit a change, bugfix or new module.

### Dependencies

 * C++20 compatible compiler
   (GCC >= 12 or Clang >= 16. GCC is recommended)
 * Meson (>= 0.64)
 * Qt5 (>= 5.12)
 * Qt5 Test
 * Qt5 OpenGL
 * Qt5 SVG
 * Qt5 SerialPort
 * GLib (>= 2.58)
 * [Iceoryx](https://github.com/eclipse-iceoryx/iceoryx) (>= 2.0)
 * Eigen3
 * [TOML++](https://github.com/marzer/tomlplusplus/)
 * OpenCV (>= 4.1)
 * FFmpeg (>= 4.1)
 * GStreamer (>= 1.0)
 * PipeWire
 * KF5 Archive
 * KF5 TextEditor
 * [pybind11](https://github.com/pybind/pybind11)
 * libusb (>= 1.0)
 * ImGui / ImPlot (optional, needed for plotting)

We recommend Debian 13 (Trixie) or Ubuntu 24.04 (Noble Numbat) to build & run Syntalos,
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
(Code should be valid C++20, use 4 spaces for indentation. With clang-format installed, `autoformat.py` can be used
to automatically format C++ and Python code)
