/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "zarrv3writer.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

using namespace Syntalos;

ZarrV3Array::ZarrV3Array(
    const QString &storeDir,
    const QString &arrayName,
    DType dtype,
    int64_t chunkSize,
    int nCols,
    QStringList dimNames)
    : m_arrayDir(storeDir + "/" + arrayName),
      m_dtype(dtype),
      m_chunkSize(chunkSize),
      m_nCols(nCols),
      m_dimNames(std::move(dimNames)),
      m_typeSize(0),
      m_cctx(nullptr),
      m_shardOffset(0),
      m_bufferOffset(0),
      m_totalRows(0),
      m_chunkIdx(0)
{
    // I am lazy... We can add proper portability later, if we ever need it.
    static_assert(std::endian::native == std::endian::little, "This Zarr writer only supports little-endian hosts");

    // we assume double is 64-bit, so, just make sure the compiler doesn't do weird things
    static_assert(sizeof(double) == 8, "This writer requires 64-bit double for Zarr float64");
    static_assert(std::numeric_limits<double>::is_iec559, "This writer requires IEEE-754 doubles");

    switch (dtype) {
    case DType::Float64:
        m_typeSize = sizeof(double);
        break;
    case DType::Int32:
        m_typeSize = sizeof(int32_t);
        break;
    case DType::UInt64:
        m_typeSize = sizeof(uint64_t);
        break;
    }
}

std::expected<void, QString> ZarrV3Array::open()
{
    // Create the shard directory layout and open the shard file for writing.
    // For 1-D arrays the shard lives at c/0; for 2-D arrays at c/0/0.
    QString shardPath;
    if (m_nCols == 1) {
        if (!QDir().mkpath(m_arrayDir + "/c"))
            return std::unexpected(QStringLiteral("Failed to create directory: ") + m_arrayDir + "/c");
        shardPath = m_arrayDir + "/c/0";
    } else {
        if (!QDir().mkpath(m_arrayDir + "/c/0"))
            return std::unexpected(QStringLiteral("Failed to create directory: ") + m_arrayDir + "/c/0");
        shardPath = m_arrayDir + "/c/0/0";
    }

    m_shardFile.setFileName(shardPath);
    if (!m_shardFile.open(QIODevice::WriteOnly))
        return std::unexpected(
            QStringLiteral("Failed to open shard file for writing: ") + shardPath + ": " + m_shardFile.errorString());

    m_cctx = ZSTD_createCCtx();
    if (!m_cctx)
        return std::unexpected(QStringLiteral("Failed to create ZSTD compression context"));
    ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, 3);
    ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_checksumFlag, 1);

    return {};
}

void ZarrV3Array::setAttributes(const QJsonObject &attrs)
{
    m_attributes = attrs;
}

void ZarrV3Array::appendBytes(const void *data, int64_t nRows)
{
    const auto *src = static_cast<const std::byte *>(data);
    const int64_t byteCount = nRows * m_nCols * m_typeSize;
    m_buffer.insert(m_buffer.end(), src, src + byteCount);

    const int64_t chunkBytes = m_chunkSize * m_nCols * m_typeSize;
    while ((int64_t)(m_buffer.size() - m_bufferOffset) >= chunkBytes) {
        writeChunk(m_buffer.data() + m_bufferOffset, m_chunkSize);
        m_bufferOffset += chunkBytes;
    }
}

bool ZarrV3Array::finalize()
{
    const int64_t remaining = (int64_t)m_buffer.size() - m_bufferOffset;
    if (remaining > 0) {
        const int64_t remainingRows = remaining / (m_nCols * m_typeSize);
        if (remainingRows > 0) {
            // Pad the partial final chunk to the full nominal chunk size with zeros
            // (fill_value = 0). zarr-python decompresses the full chunk and then
            // slices down to the actual array length using the shape in zarr.json.
            const size_t chunkBytes = static_cast<size_t>(m_chunkSize) * m_nCols * m_typeSize;
            ByteVector paddedChunk(chunkBytes, std::byte{0});
            const size_t remainingBytes = static_cast<size_t>(remainingRows) * m_nCols * m_typeSize;
            std::memcpy(paddedChunk.data(), m_buffer.data() + m_bufferOffset, remainingBytes);

            const int64_t trueTotal = m_totalRows + remainingRows;
            writeChunk(paddedChunk.data(), m_chunkSize); // increments m_totalRows by m_chunkSize
            m_totalRows = trueTotal;                     // correct back to actual row count
        }
    }

    m_buffer.clear();
    m_bufferOffset = 0;

    if (m_cctx != nullptr) {
        ZSTD_freeCCtx(m_cctx);
        m_cctx = nullptr;
    }

    // Append the shard index (N x 16 bytes: offset + length per inner chunk)
    // at the end of the shard file, then close it.
    if (m_shardFile.isOpen()) {
        m_shardFile.write(
            reinterpret_cast<const char *>(m_indexBuffer.data()), static_cast<qint64>(m_indexBuffer.size()));
        m_shardFile.close();
    }
    m_indexBuffer.clear();

    return writeMetadata();
}

int64_t ZarrV3Array::totalRows() const
{
    return m_totalRows;
}

bool ZarrV3Array::writeChunk(const void *data, int64_t nRows)
{
    if (!m_shardFile.isOpen())
        return false;

    const size_t srcSize = static_cast<size_t>(nRows) * m_nCols * m_typeSize;
    const size_t destCapacity = ZSTD_compressBound(srcSize);
    ByteVector dest(destCapacity);

    const size_t csize = ZSTD_compress2(m_cctx, dest.data(), destCapacity, data, srcSize);
    if (ZSTD_isError(csize))
        return false;

    // Record (byte_offset, byte_length) in the shard index as raw little-endian bytes
    const uint64_t indexOffset = m_shardOffset;
    const uint64_t indexLength = static_cast<uint64_t>(csize);
    const auto *offsetBytes = reinterpret_cast<const std::byte *>(&indexOffset);
    const auto *lengthBytes = reinterpret_cast<const std::byte *>(&indexLength);
    m_indexBuffer.insert(m_indexBuffer.end(), offsetBytes, offsetBytes + sizeof(uint64_t));
    m_indexBuffer.insert(m_indexBuffer.end(), lengthBytes, lengthBytes + sizeof(uint64_t));

    // Append compressed data to the open shard file
    const qint64 written = m_shardFile.write(reinterpret_cast<const char *>(dest.data()), static_cast<qint64>(csize));
    if (written != static_cast<qint64>(csize))
        return false;

    m_shardOffset += csize;
    m_totalRows += nRows;
    m_chunkIdx++;
    return true;
}

bool ZarrV3Array::writeMetadata()
{
    QJsonObject meta;
    meta["zarr_format"] = 3;
    meta["node_type"] = QStringLiteral("array");

    // Shape: [total_rows] for 1-D, [total_rows, n_channels] for 2-D
    QJsonArray shape;
    shape.append(static_cast<qint64>(m_totalRows));
    if (m_nCols > 1)
        shape.append(static_cast<qint64>(m_nCols));
    meta["shape"] = shape;

    // Data type
    QString dtypeStr;
    switch (m_dtype) {
    case DType::Float64:
        dtypeStr = QStringLiteral("float64");
        break;
    case DType::Int32:
        dtypeStr = QStringLiteral("int32");
        break;
    case DType::UInt64:
        dtypeStr = QStringLiteral("uint64");
        break;
    }
    meta["data_type"] = dtypeStr;

    // Outer chunk grid: one shard per array.  The shard shape must be at least
    // as large as the array shape.  We use m_chunkIdx * m_chunkSize which is
    // the zero-padded total (>= m_totalRows).
    QJsonArray outerChunkShape;
    outerChunkShape.append(static_cast<qint64>(m_chunkIdx * m_chunkSize));
    if (m_nCols > 1)
        outerChunkShape.append(static_cast<qint64>(m_nCols));

    QJsonObject chunkConfig;
    chunkConfig["chunk_shape"] = outerChunkShape;

    QJsonObject chunkGrid;
    chunkGrid["name"] = QStringLiteral("regular");
    chunkGrid["configuration"] = chunkConfig;
    meta["chunk_grid"] = chunkGrid;

    // Default chunk key encoding with "/" separator
    QJsonObject keyEncConf;
    keyEncConf["separator"] = QStringLiteral("/");
    QJsonObject keyEnc;
    keyEnc["name"] = QStringLiteral("default");
    keyEnc["configuration"] = keyEncConf;
    meta["chunk_key_encoding"] = keyEnc;

    // Fill value
    if (m_dtype == DType::Float64)
        meta["fill_value"] = 0.0;
    else
        meta["fill_value"] = 0;

    // Inner chunk shape (one inner chunk = one time-slice of chunkSize rows)
    QJsonArray innerChunkShape;
    innerChunkShape.append(static_cast<qint64>(m_chunkSize));
    if (m_nCols > 1)
        innerChunkShape.append(static_cast<qint64>(m_nCols));

    // Codec chain for each inner chunk: bytes (little-endian, C-order) -> zstd
    QJsonObject bytesConf;
    bytesConf["endian"] = QStringLiteral("little");
    QJsonObject bytesCodec;
    bytesCodec["name"] = QStringLiteral("bytes");
    bytesCodec["configuration"] = bytesConf;

    QJsonObject zstdConf;
    zstdConf["level"] = 3;
    zstdConf["checksum"] = true;
    QJsonObject zstdCodec;
    zstdCodec["name"] = QStringLiteral("zstd");
    zstdCodec["configuration"] = zstdConf;

    QJsonArray innerCodecs;
    innerCodecs.append(bytesCodec);
    innerCodecs.append(zstdCodec);

    // Index codec: raw little-endian bytes (no additional transformation)
    QJsonArray indexCodecs;
    indexCodecs.append(bytesCodec);

    QJsonObject shardConf;
    shardConf["chunk_shape"] = innerChunkShape;
    shardConf["codecs"] = innerCodecs;
    shardConf["index_codecs"] = indexCodecs;
    shardConf["index_location"] = QStringLiteral("end");

    QJsonObject shardCodec;
    shardCodec["name"] = QStringLiteral("sharding_indexed");
    shardCodec["configuration"] = shardConf;

    QJsonArray codecs;
    codecs.append(shardCodec);
    meta["codecs"] = codecs;

    // Optional dimension names - must match the actual number of dimensions
    if (!m_dimNames.isEmpty()) {
        const int ndim = (m_nCols > 1) ? 2 : 1;
        QJsonArray dims;
        for (int i = 0; i < ndim && i < m_dimNames.size(); ++i)
            dims.append(m_dimNames[i]);
        meta["dimension_names"] = dims;
    }

    // Optional user attributes (signal names, units, etc.)
    if (!m_attributes.isEmpty())
        meta["attributes"] = m_attributes;

    QFile f(m_arrayDir + "/zarr.json");
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(meta).toJson());
    f.close();
    return true;
}

bool zarrWriteRootGroupMetadata(const QString &storePath)
{
    QDir dir;
    if (!dir.mkpath(storePath))
        return false;

    QJsonObject meta;
    meta["zarr_format"] = 3;
    meta["node_type"] = QStringLiteral("group");
    meta["attributes"] = QJsonObject();

    QFile f(storePath + "/zarr.json");
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(meta).toJson());
    f.close();
    return true;
}
