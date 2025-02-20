//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <iostream>
#include "datafile.h"


DataFile::DataFile(const QString& fileName_) :
    fileName(fileName_)
{
    file = nullptr;
    dataStream = nullptr;

    file = new QFile(fileName);
    if (!file->open(QIODevice::ReadOnly)) {
        open = false;
        std::cerr << "DataFile: Cannot open file " << fileName.toStdString() << " for reading: " <<
                qPrintable(file->errorString()) << '\n';
    } else {
        open = true;
    }
    dataStream = new QDataStream(file);

    // Maintain bit-level compatibility with existing code.
    dataStream->setVersion(QDataStream::Qt_5_11);

    // Set to little endian mode for compatibilty with MATLAB, which is little endian on all platforms.
    dataStream->setByteOrder(QDataStream::LittleEndian);
}

DataFile::~DataFile()
{
    close();
}

void DataFile::close()
{
    if (!file) return;
    file->close();
    if (dataStream) {
        delete dataStream;
        dataStream = nullptr;
    }
    delete file;
    file = nullptr;
}
