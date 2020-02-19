Syntalos
========
(formerly known as MazeAmaze)

Syntalos is a software for in-vivo behavior tracking, electrophysiology and control of automated
mazes.
It was developed initially as a quick-and-dirty solution for M-maze experiments, as the previously used
tool was unable to keep timing synchronized and was highly unstable.
Over time, Syntalos evolved into a swiss-army knife for behavior experiments in our lab, gaining more and
more features and eventually receiving a major refactoring to make the tool more modular and extensible.
This has allowed it to be used by others for new types of experiments as well.

Syntalos is built around a set of core goals:
 * Have a fixed directory structure to store experiment data in, with very limited configuration options
 * Keep individual modules as much in sync as possible, ensuring timestamps in e.g. electrophysiology match the ones from video recording
 * Save all auxiliary experiment parameters to experiment result folders as well
 * Be fast: If necessary, automatically sacrifice live display for accurate and performant data recording
 * Be Linux-native: No additional time has been spent to make this software portable to non-Linux platforms. While it may
   be possible to make it run on other systems, the main priority is to make it work well on Linux.

The new version of Syntalos is modular and features a set of components to be added to create new experiment scenarios.
Currently, the following modules are built-in:
 * *rhd2000*: Use an [RHD2000 USB Interface Board](http://intantech.com/RHD2000_USB_interface_board.html) by [IntanTech](http://intantech.com/)
   for biopotential recordings of up to 256 channels.
 * *firmata-io*: Control a serial device that speaks the [Firmata](http://firmata.org/wiki/Main_Page) protocol, usually an Arduino.
   This module can be controled via the Python module.
 * *pyscript*: Run arbitrary Python 3 code and access a (limited) amount of Syntalos features and modules from Python.
 * *traceplot*: Plot long-running traces recorded via the *rhd2000* module.
 * *runcmd*: Run arbitrary commands at varios stages of the experiment.
 * *triled-tracker*: Track an animal via three LEDs mounted on an Intan amplifier board headstage and save various behavior parameters.
 * *videorecorder*: Record data from cameras to video files in various formats.
 * *genericcamera*: Record video from any normal camera supported by Linux' V4L API, e.g. a regular webcam.
 * *ueyecamera*: Record video with an uEye industrial camera from [IDS](https://ids-imaging.com).

## Developers

[![Build Status](https://travis-ci.org/bothlab/mazeamaze.svg?branch=master)](https://travis-ci.org/bothlab/mazeamaze)

### Dependencies

 * cmake (>= 3.12)
 * Qt5 (>= 5.10)
 * Qt5 SerialPort
 * Qt5 Charts
 * Qt5 SVG
 * Qt5 OpenGL
 * Boost {Containers, Python3} (>= 1.66)
 * FFmpeg (>= 4.1)
 * OpenCV (>= 4.1)
 * ZeroMQ
 * KF5 Archive
 * KF5 TextEditor

Before attempting to build Syntalos, ensure all dependencies (and their development files) are installed on your system.
You should then be able to build the software after configuring the build with cmake for your platform:
```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
sudo make install
```

Pull-requests are very welcome! (Code should be valid C++14, use 4 spaces for indentation)

## Users

Currently, Syntalos needs to be compiled manually and we do not provide automatic binary builds yet.
This will likely change if the software becomes used more in more labs.
