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

#include <QFile>
#include <QIODevice>
#include <QDataStream>
#include <QGuiApplication>
#include <QDateTime>
#include <QtMath>
#include <QDebug>
#include <limits>
#include "rhxglobals.h"
#include "matfilewriter.h"

MatFileElement::MatFileElement(const QString &name_) :
    name(name_)
{
}

MatFileElement::~MatFileElement()
{
}

int MatFileElement::sizeOfDataTypeInBytes(StorageDataType dataType)
{
    switch (dataType) {
    case StorageDataType::StorageDataTypeInt8:
    case StorageDataType::StorageDataTypeUInt8:
    case StorageDataType::StorageDataTypeUtf8:
        return 1;
    case StorageDataType::StorageDataTypeInt16:
    case StorageDataType::StorageDataTypeUInt16:
    case StorageDataType::StorageDataTypeUtf16:
        return 2;
    case StorageDataType::StorageDataTypeInt32:
    case StorageDataType::StorageDataTypeUInt32:
    case StorageDataType::StorageDataTypeUtf32:
    case StorageDataType::StorageDataTypeSingle:
        return 4;
    case StorageDataType::StorageDataTypeInt64:
    case StorageDataType::StorageDataTypeUInt64:
    case StorageDataType::StorageDataTypeDouble:
        return 8;
    case StorageDataType::StorageDataTypeMatrix:
    case StorageDataType::StorageDataTypeCompressed:
    default:
        return 0;
    }
}

int MatFileElement::numPaddingBytes(int numDataBytes)
{
    const int BytesPerBoundary = 8;
    return BytesPerBoundary * qCeil((double)numDataBytes / (double)BytesPerBoundary) - numDataBytes;
}

void MatFileElement::writePaddingBytes(QDataStream& outStream, int numDataBytes)
{
    int numPadding = numPaddingBytes(numDataBytes);
    for (int i = 0; i < numPadding; ++i) {
        outStream << (int8_t) 0;
    }
}

StorageDataType MatFileElement::correspondingStorageDataType(MatlabDataType dataType)
{
    switch (dataType) {
    case MatlabDataTypeDouble:
        return StorageDataTypeDouble;
    case MatlabDataTypeSingle:
        return StorageDataTypeSingle;
    case MatlabDataTypeInt8:
        return StorageDataTypeInt8;
    case MatlabDataTypeUInt8:
        return StorageDataTypeUInt8;
    case MatlabDataTypeInt16:
        return StorageDataTypeInt16;
    case MatlabDataTypeUInt16:
        return StorageDataTypeUInt16;
    case MatlabDataTypeInt32:
        return StorageDataTypeInt32;
    case MatlabDataTypeUInt32:
        return StorageDataTypeUInt32;
    case MatlabDataTypeInt64:
        return StorageDataTypeInt64;
    case MatlabDataTypeUInt64:
        return StorageDataTypeUInt64;
    case MatlabDataTypeChar:
        return StorageDataTypeUtf8;
    default:
        return StorageDataTypeInvalid;
    }
}

MatlabDataType MatFileElement::correspondingMatlabDataType(StorageDataType dataType)
{
    switch (dataType) {
    case StorageDataTypeDouble:
        return MatlabDataTypeDouble;
    case StorageDataTypeSingle:
        return MatlabDataTypeSingle;
    case StorageDataTypeInt8:
        return MatlabDataTypeInt8;
    case StorageDataTypeUInt8:
        return MatlabDataTypeUInt8;
    case StorageDataTypeInt16:
        return MatlabDataTypeInt16;
    case StorageDataTypeUInt16:
        return MatlabDataTypeUInt16;
    case StorageDataTypeInt32:
        return MatlabDataTypeInt32;
    case StorageDataTypeUInt32:
        return MatlabDataTypeUInt32;
    case StorageDataTypeInt64:
        return MatlabDataTypeInt64;
    case StorageDataTypeUInt64:
        return MatlabDataTypeUInt64;
    default:
        return MatlabDataTypeInvalid;
    }
}

StorageDataType MatFileElement::smallestCompressedStorageDataType(int64_t minValue, int64_t maxValue)
{
    // Note: MATLAB allows only the following integer types to be used for compression of stored data:
    // UInt8, Int16, UInt16, or Int32.

    if (minValue >= (int64_t) numeric_limits<uint8_t>::lowest() && maxValue <= (int64_t) numeric_limits<uint8_t>::max()) {
        return StorageDataType::StorageDataTypeUInt8;
    }
    if (minValue >= (int64_t) numeric_limits<int16_t>::lowest() && maxValue <= (int64_t) numeric_limits<int16_t>::max()) {
        return StorageDataType::StorageDataTypeInt16;
    }
    if (minValue >= (int64_t) numeric_limits<uint16_t>::lowest() && maxValue <= (int64_t) numeric_limits<uint16_t>::max()) {
        return StorageDataType::StorageDataTypeUInt16;
    }
    if (minValue >= (int64_t) numeric_limits<int32_t>::lowest() && maxValue <= (int64_t) numeric_limits<int32_t>::max()) {
        return StorageDataType::StorageDataTypeInt32;
    }
    return StorageDataType::StorageDataTypeInt64;
}


MatFileNumericArray::MatFileNumericArray(const QString& name_, int rows_, int columns_, MatlabDataType matlabArrayType_) :
    MatFileElement(name_),
    rows(rows_),
    columns(columns_),
    transpose(false),
    storageDataType(StorageDataType::StorageDataTypeInt32),
    matlabDataType(matlabArrayType_)
{
}

void MatFileNumericArray::writeDataElementInfo(QDataStream& outStream) const
{
    // Tag
    outStream << (int32_t) StorageDataType::StorageDataTypeMatrix;
    outStream << (int32_t) sizeOfArrayDataInBytes();

    // Array flags
    outStream << (int32_t) StorageDataType::StorageDataTypeUInt32;
    outStream << (int32_t) 8;
    outStream << (uint32_t) matlabDataType;
    outStream << (uint32_t) 0;

    // Dimensions array
    outStream << (int32_t) StorageDataType::StorageDataTypeInt32;
    outStream << (int32_t) 8;
    if (!transpose) {
        outStream << (int32_t) rows;
        outStream << (int32_t) columns;
    } else {
        outStream << (int32_t) columns;
        outStream << (int32_t) rows;
    }

    // Array name
    int length = name.size();
    outStream << (int32_t) StorageDataType::StorageDataTypeInt8;
    outStream << (int32_t) length;
    QByteArray nameChars(name.toUtf8());
    for (int i = 0; i < length; ++i) {
        outStream << (int8_t) nameChars[i];
    }
    writePaddingBytes(outStream, length);

    // Sub-element tag for data
    outStream << (int32_t) storageDataType;
    outStream << rows * columns * sizeOfDataTypeInBytes(storageDataType);
}

int MatFileNumericArray::sizeOfArrayDataInBytes() const
{
    // Note: Size does not include element tag (8 bytes)
    int size = 40;  // size of array flags, dimensions array, and first part of array name
    size += 8 * qCeil((double) name.size() / 8.0);   // bytes to store array name
    size += 8;  // first part of array data
    int numDataBytes = rows * columns * sizeOfDataTypeInBytes(storageDataType);
    size += numDataBytes + numPaddingBytes(numDataBytes);    // data
    return size;
}


MatFileSparseArray::MatFileSparseArray(const QString& name_, int rows_, int columns_) :
    MatFileElement(name_),
    rows(rows_),
    columns(columns_),
    transpose(false)
{
}

void MatFileSparseArray::writeDataElementInfo(QDataStream& outStream) const
{
    // Tag
    outStream << (int32_t) StorageDataType::StorageDataTypeMatrix;
    outStream << (int32_t) sizeOfArrayDataInBytes();

    // Array flags
    outStream << (int32_t) StorageDataType::StorageDataTypeUInt32;
    outStream << (int32_t) 8;
    outStream << (uint32_t) MatlabDataTypeSparse;
    outStream << (uint32_t) qMax(numNonZeroElements, 1);    // must write 1 for special case of all-zero sparse array

    // Dimensions array
    outStream << (int32_t) StorageDataType::StorageDataTypeInt32;
    outStream << (int32_t) 8;
    if (!transpose) {
        outStream << (int32_t) rows;
        outStream << (int32_t) columns;
    } else {
        outStream << (int32_t) columns;
        outStream << (int32_t) rows;
    }

    // Array name
    int length = name.size();
    outStream << (int32_t) StorageDataType::StorageDataTypeInt8;
    outStream << (int32_t) length;
    QByteArray nameChars(name.toUtf8());
    for (int i = 0; i < length; ++i) {
        outStream << (int8_t) nameChars[i];
    }
    writePaddingBytes(outStream, length);

    // Row index for nonzero values (ir)
    outStream << (int32_t) StorageDataType::StorageDataTypeInt32;
    int numRowBytes = numNonZeroElements * sizeOfDataTypeInBytes(StorageDataTypeInt32);
    outStream << (int32_t) numRowBytes;
    if (!transpose) {
        for (int j = 0; j < columns; ++j) {
            for (int i = 0; i < rows; ++i) {
                if (dataArray[i][j] != 0) {
                    outStream << (int32_t) i;
                }
            }
        }
    } else {
        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < columns; ++i) {
                if (dataArray[j][i] != 0) {
                    outStream << (int32_t) i;
                }
            }
        }
    }
    writePaddingBytes(outStream, numRowBytes);

    // Column index for nonzero value (jc)
    outStream << (int32_t) StorageDataType::StorageDataTypeInt32;
    int numColumnBytes = (transpose ? (rows + 1) : (columns + 1)) * sizeOfDataTypeInBytes(StorageDataTypeInt32);
    outStream << (int32_t) numColumnBytes;
    outStream << (int32_t) 0;  // first jc index is always zero
    int numNonZeroElementsSoFar = 0;
    if (!transpose) {
        for (int j = 0; j < columns; ++j) {
            for (int i = 0; i < rows; ++i) {
                if (dataArray[i][j] != 0) ++numNonZeroElementsSoFar;
            }
            outStream << (int32_t) numNonZeroElementsSoFar;
        }
    } else {
        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < columns; ++i) {
                if (dataArray[j][i] != 0) ++numNonZeroElementsSoFar;
            }
            outStream << (int32_t) numNonZeroElementsSoFar;
        }
    }
    writePaddingBytes(outStream, numColumnBytes);

    // Sub-element tag for data
    outStream << (int32_t) storageDataType;
    outStream << numNonZeroElements * sizeOfDataTypeInBytes(storageDataType);
}

int MatFileSparseArray::sizeOfArrayDataInBytes() const
{
    // Note: Size does not include element tag (8 bytes).
    int size = 40;  // size of array flags, dimensions array, and first part of array name
    size += 8 * qCeil((double) name.size() / 8.0);   // bytes to store array name
    size += 8;  // first part of row index for nonzero values (ir) subelement
    int numRowIndexBytes = numNonZeroElements * sizeOfDataTypeInBytes(StorageDataTypeInt32);
    size += numRowIndexBytes + numPaddingBytes(numRowIndexBytes);
    size += 8;  // first part of column index for nonzero values (jc) subelement
    int numColumns = transpose ? rows : columns;
    int numColumnIndexBytes = (numColumns + 1) * sizeOfDataTypeInBytes(StorageDataTypeInt32);
    size += numColumnIndexBytes + numPaddingBytes(numColumnIndexBytes);
    size += 8;  // first part of array data
    int numDataBytes = numNonZeroElements * sizeOfDataTypeInBytes(storageDataType);
    size += numDataBytes + numPaddingBytes(numDataBytes);    // data
    return size;
}


MatFileUInt8SparseArray::MatFileUInt8SparseArray(const QString& name_, const vector<vector<uint8_t> >& dataArray_) :
    MatFileSparseArray(name_, (int) dataArray_.size(), (int) dataArray_[0].size())
{
    storageDataType = StorageDataTypeUInt8;
    numNonZeroElements = 0;
    dataArray.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataArray[i].resize(columns);
        for (int j = 0; j < columns; ++j) {
            int32_t data = (int32_t) dataArray_[i][j];
            if (data != 0) ++numNonZeroElements;
            dataArray[i][j] = data;
        }
    }
}

void MatFileUInt8SparseArray::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    if (!transpose) {
        for (int j = 0; j < columns; ++j) {
            for (int i = 0; i < rows; ++i) {
                if (dataArray[i][j] != 0) {
                    outStream << (uint8_t) dataArray[i][j];
                }
            }
        }
    } else {
        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < columns; ++i) {
                if (dataArray[j][i] != 0) {
                    outStream << (uint8_t) dataArray[j][i];
                }
            }
        }
    }

    int numDataBytes = numNonZeroElements * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileIntegerScalar::MatFileIntegerScalar(const QString& name_, int64_t dataValue_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, 1, 1, matlabDataType_),
    dataValue(dataValue_)
{
    if (matlabDataType == MatlabDataTypeSingle) {
        qDebug() << "Error: MatFileIntegerScalar does not support single-precision floating-point array types.";
    }
    if (matlabDataType == MatlabDataTypeDouble) {
        storageDataType = smallestCompressedStorageDataType(dataValue, dataValue);
    } else {
        storageDataType = correspondingStorageDataType(matlabDataType);
    }
}

void MatFileIntegerScalar::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    switch (storageDataType) {
    case StorageDataTypeInt8:
        outStream << (qint8) dataValue;
        break;
    case StorageDataTypeUInt8:
        outStream << (quint8) dataValue;
        break;
    case StorageDataTypeInt16:
        outStream << (qint16) dataValue;
        break;
    case StorageDataTypeUInt16:
        outStream << (quint16) dataValue;
        break;
    case StorageDataTypeInt32:
        outStream << (qint32) dataValue;
        break;
    case StorageDataTypeUInt32:
        outStream << (quint32) dataValue;
        break;
    case StorageDataTypeInt64:
        outStream << (qint64) dataValue;
        break;
    case StorageDataTypeUInt64:
        outStream << (quint64) dataValue;
        break;
    default:
        return;
    }
    writePaddingBytes(outStream, sizeOfDataTypeInBytes(storageDataType));
}


MatFileRealScalar::MatFileRealScalar(const QString& name_, double dataValue_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, 1, 1, matlabDataType_),
    dataValue(dataValue_)
{
    if (matlabDataType != MatlabDataTypeSingle && matlabDataType != MatlabDataTypeDouble) {
        qDebug() << "Error: MatFileRealScalar does not support non-floating-point array types.";
    }
    storageDataType = correspondingStorageDataType(matlabDataType);
}

void MatFileRealScalar::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    switch (storageDataType) {
    case StorageDataTypeSingle:
        outStream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        outStream << dataValue;
        break;
    case StorageDataTypeDouble:
        outStream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        outStream << dataValue;
        break;
    default:
        return;
    }
    writePaddingBytes(outStream, sizeOfDataTypeInBytes(storageDataType));
}


MatFileString::MatFileString(const QString& name_, const QString& text_) :
    MatFileNumericArray(name_, 1, text_.length(), MatlabDataTypeChar),
    text(text_)
{
    storageDataType = correspondingStorageDataType(matlabDataType);
}

void MatFileString::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    QByteArray dataChars(text.toUtf8());
    for (int i = 0; i < columns; ++i) {
        outStream << (int8_t) dataChars[i];
    }
    writePaddingBytes(outStream, columns * sizeOfDataTypeInBytes(storageDataType));
}


MatFileUInt8Vector::MatFileUInt8Vector(const QString& name_, const vector<uint8_t>& dataVector_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataVector_.size(), 1, matlabDataType_)
{
    storageDataType = correspondingStorageDataType(matlabDataType);
    dataVector.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataVector[i] = dataVector_[i];
    }
}

void MatFileUInt8Vector::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    for (int i = 0; i < rows; ++i) {
        outStream << (uint8_t) dataVector[i];
    }

    int numDataBytes = rows * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileUInt16Vector::MatFileUInt16Vector(const QString& name_, const vector<uint16_t>& dataVector_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataVector_.size(), 1, matlabDataType_)
{
    storageDataType = correspondingStorageDataType(matlabDataType);
    dataVector.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataVector[i] = dataVector_[i];
    }
}

void MatFileUInt16Vector::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    for (int i = 0; i < rows; ++i) {
        outStream << (uint16_t) dataVector[i];
    }

    int numDataBytes = rows * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileInt16Vector::MatFileInt16Vector(const QString& name_, const vector<int16_t>& dataVector_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataVector_.size(), 1, matlabDataType_)
{
    storageDataType = correspondingStorageDataType(matlabDataType);
    dataVector.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataVector[i] = dataVector_[i];
    }
}

void MatFileInt16Vector::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    for (int i = 0; i < rows; ++i) {
        outStream << (int16_t) dataVector[i];
    }

    int numDataBytes = rows * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileInt32Vector::MatFileInt32Vector(const QString& name_, const vector<int32_t>& dataVector_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataVector_.size(), 1, matlabDataType_)
{
    storageDataType = correspondingStorageDataType(matlabDataType);
    dataVector.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataVector[i] = dataVector_[i];
    }
}

void MatFileInt32Vector::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    for (int i = 0; i < rows; ++i) {
        outStream << (int32_t) dataVector[i];
    }

    int numDataBytes = rows * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileRealVector::MatFileRealVector(const QString& name_, const vector<float>& dataVector_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataVector_.size(), 1, matlabDataType_)
{
    if (matlabDataType != MatlabDataTypeSingle && matlabDataType != MatlabDataTypeDouble) {
        qDebug() << "Error: MatFileRealVector does not support non-floating-point array types.";
    }
    storageDataType = correspondingStorageDataType(matlabDataType);

    dataVector.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataVector[i] = dataVector_[i];
    }
}

void MatFileRealVector::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    if (storageDataType == StorageDataTypeSingle) {
        outStream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    } else {
        outStream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    }
    for (int i = 0; i < rows; ++i) {
        outStream << dataVector[i];
    }

    int numDataBytes = rows * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileRealArray::MatFileRealArray(const QString& name_, const vector<vector<float> >& dataArray_, MatlabDataType matlabDataType_) :
    MatFileNumericArray(name_, (int) dataArray_.size(), (int) dataArray_[0].size(), matlabDataType_)
{
    if (matlabDataType != MatlabDataTypeSingle && matlabDataType != MatlabDataTypeDouble) {
        qDebug() << "Error: MatFileRealArray does not support non-floating-point array types.";
    }
    storageDataType = correspondingStorageDataType(matlabDataType);
    dataArray.resize(rows);
    for (int i = 0; i < rows; ++i) {
        dataArray[i].resize(columns);
        for (int j = 0; j < columns; ++j) {
            dataArray[i][j] = dataArray_[i][j];
        }
    }
}

void MatFileRealArray::writeElement(QDataStream& outStream)
{
    writeDataElementInfo(outStream);

    if (storageDataType == StorageDataTypeSingle) {
        outStream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    } else {
        outStream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    }
    if (transpose) {
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < columns; ++j) {
                outStream << dataArray[i][j];
            }
        }
    } else {
        for (int j = 0; j < columns; ++j) {
            for (int i = 0; i < rows; ++i) {
                outStream << dataArray[i][j];
            }
        }
    }
    int numDataBytes = rows * columns * sizeOfDataTypeInBytes(storageDataType);
    writePaddingBytes(outStream, numDataBytes);
}


MatFileWriter::MatFileWriter()
{
}

MatFileWriter::~MatFileWriter()
{
    removeAllElements();
}

void MatFileWriter::removeAllElements()
{
    for (int i = 0; i < (int) elements.size(); ++i) {
        delete elements[i];
    }
    elements.clear();
}

int MatFileWriter::addElement(MatFileElement* element)
{
    elements.push_back(element);
    return (int) elements.size() - 1;     // Return index to added element.
}

int MatFileWriter::addIntegerScalar(const QString& name_, int64_t dataValue_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileIntegerScalar(name_, dataValue_, matlabDataType_));
}

int MatFileWriter::addRealScalar(const QString& name_, double dataValue_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileRealScalar(name_, dataValue_, matlabDataType_));
}

int MatFileWriter::addString(const QString& name_, const QString& text_)
{
    return addElement(new MatFileString(name_, text_));
}

int MatFileWriter::addUInt8Vector(const QString& name_, const vector<uint8_t>& dataVector_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileUInt8Vector(name_, dataVector_, matlabDataType_));
}

int MatFileWriter::addUInt16Vector(const QString& name_, const vector<uint16_t>& dataVector_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileUInt16Vector(name_, dataVector_, matlabDataType_));
}

int MatFileWriter::addInt16Vector(const QString& name_, const vector<int16_t>& dataVector_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileInt16Vector(name_, dataVector_, matlabDataType_));
}

int MatFileWriter::addInt32Vector(const QString& name_, const vector<int32_t>& dataVector_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileInt32Vector(name_, dataVector_, matlabDataType_));
}

int MatFileWriter::addRealVector(const QString& name_, const vector<float>& dataVector_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileRealVector(name_, dataVector_, matlabDataType_));
}

int MatFileWriter::addRealArray(const QString& name_, const vector<vector<float> >& dataArray_, MatlabDataType matlabDataType_)
{
    return addElement(new MatFileRealArray(name_, dataArray_, matlabDataType_));
}

int MatFileWriter::addUInt8SparseArray(const QString& name_, const vector<vector<uint8_t> >& dataArray_)
{
    return addElement(new MatFileUInt8SparseArray(name_, dataArray_));
}

void MatFileWriter::transposeLastElement()
{
    if (!elements.empty()) {
        elements[elements.size() - 1]->setTransposed(true);
    }
}

bool MatFileWriter::writeFile(QString fileName)
{
    if (elements.empty()) {
        qDebug() << "MatFileWriter::writeFile: Elements must be added before writing.";
        return false;
    }

    if (fileName.right(4).toLower() != ".mat") fileName.append(".mat");
    QFile saveFile(fileName);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qDebug() << "MatFileWriter::writeFile: Could not open save file " << fileName;
        return false;
    }
    QDataStream outStream(&saveFile);
    outStream.setVersion(QDataStream::Qt_5_11);
    outStream.setByteOrder(QDataStream::LittleEndian);
    outStream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    writeHeader(outStream);
    for (int i = 0; i < (int) elements.size(); ++i) {
        elements[i]->writeElement(outStream);
    }

    saveFile.close();
    return true;
}

void MatFileWriter::writeHeader(QDataStream& outStream) const
{
    // Assemble header text field.
    const int MaxHeaderLength = 116;
    QString headerText = "MATLAB 5.0 MAT-file, Platform: " + QGuiApplication::platformName() + ", Created on: " +
            QDateTime::currentDateTime().toString() + " by " + ApplicationName + " version " + SoftwareVersion;
    int length = headerText.length();
    if (length > MaxHeaderLength) {
        headerText = headerText.left(MaxHeaderLength);
        length = MaxHeaderLength;
    }

    QByteArray header(headerText.toUtf8());
    for (int i = length; i < MaxHeaderLength; ++i) {
        header.append(' ');     // pad header text with spaces
    }

    // Write header.
    for (int i = 0; i < MaxHeaderLength; ++i) {
        outStream << (uint8_t) header[i];
    }

    const qint64 SubsystemDataOffsetField = 0;
    const quint16 VersionField = 0x0100u;
    const quint16 EndianIndicator = 0x4d49u;   // 'MI' in ASCII; used in MAT-file header to detect endianness of file

    outStream << SubsystemDataOffsetField;
    outStream << VersionField;
    outStream << EndianIndicator;
}
