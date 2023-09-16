Installation on Windows
#######################

.. warning::
    It is not advised to run Syntalos on Windows, as it may perform significantly worse there compared
    to running it on Linux and has not been extensively tested.
    If you attempt to use it for real and time-critical experiments, please verify that the data
    you generate has proper timestamps and can be saved fast enough (or just run it on Linux directly,
    to avoid any potential issues).

    Installing Syntalos on Windows is however a convenient method to explore its UI without having to
    install Linux on a real machine first.

Install WSL2 on Windows
=======================

To install Syntalos, you need to install WSL2 first, which adds support for Linux applications to Windows.
You can `follow Microsofts instructions <https://learn.microsoft.com/de-de/windows/wsl/install>`_ to do that,
which boil down to searching for `PowerShell` in the Windows start menu, right clicking on it and selecting
*Run as Administrator*, and then typing ``wsl --install`` into the prompt.

After executing the command and a system reboot, you should be able to search for *Windows-Subsystem for Linux* in
the menu and launch it.

.. figure:: /graphics/wsl-firstrun.png
  :width: 460

You are greeted with a dialog that requires you to set a (lowercase) username and password. Enter a name
and password of your choice and remember it for the future.


Install Syntalos
================

.. note::
    For convenience, you might want to install the `Windows Terminal <https://apps.microsoft.com/store/detail/windows-terminal/9N0DX20HK701>`_
    from the Microsoft Store, as it is a lot more convient to use than the rather basic default console on Windows.

Launch *Windows-Subsystem for Linux*, or open the Windows Terminal and click on the down arrow
next to the new-tab plus sign and select *Ubuntu*.
You will be greeted by a shell prompt.
Enter the following commands one by one and execute them (you will be prompted for the password you just set
in the previous step):

.. code-block:: bash

    sudo add-apt-repository --yes ppa:flatpak/stable
    sudo apt update && sudo apt -y full-upgrade && sudo apt --purge -y autoremove
    sudo apt install -y flatpak xdg-desktop-portal-kde

Running these commands may take a while.
You should reboot your computer (or restart WSL) after running these commands.

Afterwards, you can actually install Syntalos by running these two commands:

.. code-block:: bash

    sudo flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
    sudo flatpak install io.github.bothlab.syntalos

While running the last command, you will need to confirm the installation twice by pressing *Enter*.

You are now all set to launching Syntalos from the command-line!
If you also want it to show up in the Windows menus, you can use this hack to make it happen (run it in the
WSL2 environment):

.. code-block:: bash

    sudo mkdir -p /usr/local/share/applications/
    sudo ln -s /var/lib/flatpak/exports/share/applications/io.github.bothlab.syntalos.desktop /usr/local/share/applications/

Run Syntalos
============

You can run Syntalos from the Windows start menu if you opted for the workaround to make it show up there.
Alternatively, you can launch it from the WSL command-line (*Ubuntu* by default) by running:

.. code-block:: bash

    flatpak run io.github.bothlab.syntalos

You should then see the Syntalos GUI to play around with:

.. figure:: /graphics/syntalos-on-windows-wsl2.avif
  :width: 460

To access your data on your Windows drives, navigate to ``/mnt`` in the file/directory picker, and select one of the drives.
E.g. your user data will be in ``/mnt/c/Users/<your-name>``.

.. note::
    The Windows version will run slower and has a few known glitches, like missing window icons and windows
    appearing in random places once created.
    When using hardware devices, the experience may also not (yet?) be as smooth as it is on Linux.
