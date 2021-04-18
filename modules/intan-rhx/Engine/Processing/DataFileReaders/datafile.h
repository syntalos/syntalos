//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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

#ifndef DATAFILE_H
#define DATAFILE_H

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QDataStream>
#include <QString>

using namespace std;

class DataFile
{
public:
    DataFile(const QString& fileName_);
    ~DataFile();

    QString getFileName() const { return QFileInfo(fileName).baseName(); }
    int64_t fileSize() const { return file->size(); }
    int64_t pos() const { return file->pos(); }
    void seek(int64_t pos) { file->seek(pos); }
    bool isOpen() const { return open; }
    bool atEnd() const { return file->atEnd(); }
    uint16_t readWord() const { uint16_t word; *dataStream >> word; return word; }
    int16_t readSignedWord() const { int16_t word; *dataStream >> word; return word; }
    int32_t readTimeStamp() const { int32_t timeStamp; *dataStream >> timeStamp; return timeStamp; }
    void close();

private:
    QString fileName;
    QFile* file;
    QDataStream* dataStream;
    bool open;
};

#endif // DATAFILE_H
