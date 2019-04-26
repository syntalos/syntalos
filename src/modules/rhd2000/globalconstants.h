//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.5
//  Copyright (C) 2013-2016 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef GLOBALCONSTANTS_H
#define GLOBALCONSTANTS_H

// Trigonometric constants
#define PI  3.14159265359
#define TWO_PI  6.28318530718
#define DEGREES_TO_RADIANS  0.0174532925199
#define RADIANS_TO_DEGREES  57.2957795132

// RHD2000 Evaluation Board constants
#define SAMPLES_PER_DATA_BLOCK  60
#define MAX_NUM_DATA_STREAMS  8

// Special Unicode characters, as QString data type
#define QSTRING_MU_SYMBOL  ((QString)((QChar)0x03bc))
#define QSTRING_OMEGA_SYMBOL  ((QString)((QChar)0x03a9))
#define QSTRING_ANGLE_SYMBOL  ((QString)((QChar)0x2220))
#define QSTRING_DEGREE_SYMBOL  ((QString)((QChar)0x00b0))
#define QSTRING_PLUSMINUS_SYMBOL  ((QString)((QChar)0x00b1))

// Saved data file constants
#define DATA_FILE_MAGIC_NUMBER  0xc6912702
#define DATA_FILE_MAIN_VERSION_NUMBER  1
#define DATA_FILE_SECONDARY_VERSION_NUMBER 5

// Saved settings file constants
#define SETTINGS_FILE_MAGIC_NUMBER  0x45ab12cd
#define SETTINGS_FILE_MAIN_VERSION_NUMBER  1
#define SETTINGS_FILE_SECONDARY_VERSION_NUMBER  5

// RHD2000 chip ID numbers from ROM register 63
#define CHIP_ID_RHD2132  1
#define CHIP_ID_RHD2216  2
#define CHIP_ID_RHD2164  4

// Constant used in software to denote RHD2164 MISO B data source
#define CHIP_ID_RHD2164_B  1000

// RHD2164 MISO ID numbers from ROM register 59
#define REGISTER_59_MISO_A  53
#define REGISTER_59_MISO_B  58

// Save File Format Enumeration
enum SaveFormat {
    SaveFormatIntan,
    SaveFormatFilePerSignalType,
    SaveFormatFilePerChannel
};

#endif // GLOBALCONSTANTS_H
