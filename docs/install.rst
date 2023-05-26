Installation
############

You can find the source code and precompiled binaries for some distribution
for the latest Syntalos releases at the `Syntalos GitHub releases page <https://github.com/bothlab/syntalos/releases>`_.

Installing via Flathub / from the App-Center
============================================
Syntalos is avaulable as Flatpak bundle for download via Flathub.
The Flatpak'ed version will run on any Linux distribution, but due to Flatpak's sandbox
constraints may need some additional external software installed for certain hardware
to work properly. If these components are needed, Syntalos will notify the user.

You can `view & download Syntalos on Flathub.org <https://flathub.org/apps/io.github.bothlab.syntalos>`_,
or install it directly from you software store application (GNOME Software or KDE Discover), if Flatpak is
set up with Flathub, which it is on most distributions (with the notable exception being Ubuntu).

If Flathub is not set up, you can find instructions how to set it up `here <https://flatpak.org/setup/>`_.

For the command-line way to install Syntalos using Flatpak you may execute these commands:

.. code-block:: bash

    # replace this command with the distribution's native package manager to install Flatpak
    sudo apt install flatpak
    # set up Flathub
    sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
    # install Syntalos
    flatpak install flathub io.github.bothlab.syntalos

Installing via Packages
=======================

Debian
------
Ensure to have Debian 12 (Bookworm) or later, then install the package via your graphical package manager or via
the command-line: ``sudo apt install ./syntalos_*.deb``. You can then launch Syntalos from the application menu
or command-line. A ``-dbgsym`` package is provided to easily produce debug backtraces in case of crashes, but this
package is optional and does not need to be installed.

Ubuntu
------
Ensure you are on Ubuntu 22.04 (Jammy Jellyfish) or later.

After the PPA is registered, you should be able to install the package via your graphical package manager or
the command-line: ``sudo apt install ./syntalos_*.deb``.
You can then launch Syntalos from the application menu.

Module dependencies
===================
Some modules require external software to work and will not run or register without it being installed.
You can check for any failures during module load by clicking on *Diagnostics ➝ Module Loader* in the main window.
If any issues are listed there, modules failed to load.

ImagingSource Cameras
---------------------
If you want to use any cameras from `The ImagingSource <https://www.theimagingsource.com/>`_, you will need their
`tiscamera <https://github.com/TheImagingSource/tiscamera>`_ software.
The vendor provides binaries and source code, so you can either install the provided packages or built the component
from source.

UCLA Miniscopes
---------------
For Miniscope support, Syntalos uses the `libminiscope` library provided by `PoMiDAQ <https://github.com/bothlab/pomidaq>`_.
To use a `UCLA Miniscope <http://miniscope.org/>`_, compile the project - or just the library - from source or install
the provided binary package.

Building from source
====================
We recommend Debian 11 (Bullseye) or Ubuntu 20.04 (Focal Fossa) to run Syntalos, but any Linux distribution that has a
recent enough C++ compiler and Qt version should work.

Some modules may add additional dependencies for libraries to talk to individual devices or for a certain special feature.
In case you get a dependency error when running meson, install the missing dependency or try to build with less modules enabled.

Before attempting to build Syntalos, ensure all dependencies (and their development files) are installed on your system.
If you are using Debian or Ubuntu, you may choose to locally run the system package installation script that
the CI system uses: ``sudo ./tests/ci/install-deps-deb.sh``.

.. warning::
    This script runs APT with fsync/sync disabled to speed up package installation, but this leaves the system
    vulnerable to data corruption in case of an unexpected power outage or other issues during installation.
    If you are concerned by this, please install the packages mentioned in the script file manually.

Users of other Linux distributions can refer to the *README.md* file in the project's root directory for a list
of required dependencies, or look into the mentioned ``install-deps-deb.sh`` helper script and adjust it to install
the necessary things.

After installing all dependencies, you should be able to build the software after configuring the build with Meson for your platform:

.. code-block:: bash

    mkdir build && cd build
    meson --buildtype=debugoptimized -Doptimize-native=true ..
    ninja
    sudo ninja install

Modules can be enabled and disabled via the ``-Dmodules`` flag - refer to ``meson_options.txt`` for a list of possible,
comma-separated values.
