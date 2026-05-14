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
      m_totalRows(0),
      m_chunkIdx(0),
      m_chunksSinceMeta(0),
      m_lastMetaCheckpoint(std::chrono::steady_clock::now()),
      m_hasError(false)
{
    // I am lazy... We can add proper portability later, if we ever need it.
    static_assert(std::endian::native == std::endian::little, "This Zarr writer only supports little-endian hosts");

    // we assume double is 64-bit, so, just make sure the compiler doesn't do weird things
    static_assert(sizeof(double) == 8, "This writer requires 64-bit double for Zarr float64");
    static_assert(std::numeric_limits<double>::is_iec559, "This writer requires IEEE-754 doubles");

    switch (dtype) {
    case DType::Int32:
        m_typeSize = sizeof(int32_t);
        break;
    case DType::UInt16:
        m_typeSize = sizeof(uint16_t);
        break;
    case DType::UInt64:
        m_typeSize = sizeof(uint64_t);
        break;
    case DType::Float32:
        m_typeSize = sizeof(float);
        break;
    case DType::Float64:
        m_typeSize = sizeof(double);
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

    // Size the reusable ZSTD output buffer once - chunkBytes is constant for the run.
    const size_t chunkBytes = static_cast<size_t>(m_chunkSize) * m_nCols * m_typeSize;
    m_compressedScratch.resize(ZSTD_compressBound(chunkBytes));

    m_chunksSinceMeta = 0;
    m_lastMetaCheckpoint = std::chrono::steady_clock::now();
    m_hasError = false;
    m_errorMessage.clear();

    return {};
}

void ZarrV3Array::setAttributes(const QJsonObject &attrs)
{
    m_attributes = attrs;
}

void ZarrV3Array::appendBytes(const void *data, int64_t nRows)
{
    if (m_hasError)
        return;

    const auto *src = static_cast<const std::byte *>(data);
    const int64_t byteCount = nRows * m_nCols * m_typeSize;
    m_buffer.insert(m_buffer.end(), src, src + byteCount);

    const int64_t chunkBytes = m_chunkSize * m_nCols * m_typeSize;
    while ((int64_t)m_buffer.size() >= chunkBytes) {
        if (!writeChunk(m_buffer.data(), m_chunkSize))
            return; // setError() already called inside writeChunk on failure
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + chunkBytes);
    }
}

bool ZarrV3Array::finalize()
{
    if (m_hasError) {
        // If an earlier write failed, the file cursor may be inside a corrupt region
        // (a partial chunk that never reached its full size) and our in-memory counters
        // are out of sync with disk. The on-disk store is whatever the last successful
        // Tier-A/B checkpoint left - already self-consistent - so the safest thing is
        // to release resources and not touch the file any further.
        m_buffer.clear();
        if (m_cctx != nullptr) {
            ZSTD_freeCCtx(m_cctx);
            m_cctx = nullptr;
        }
        if (m_shardFile.isOpen())
            m_shardFile.close();
        m_indexBuffer.clear();
        return false;
    }

    if (!m_buffer.empty()) {
        const int64_t remainingRows = (int64_t)m_buffer.size() / (m_nCols * m_typeSize);
        if (remainingRows > 0) {
            // Pad the partial final chunk to the full nominal chunk size with zeros
            // (fill_value = 0). zarr-python decompresses the full chunk and then
            // slices down to the actual array length using the shape in zarr.json.
            const size_t chunkBytes = static_cast<size_t>(m_chunkSize) * m_nCols * m_typeSize;
            ByteVector paddedChunk(chunkBytes, std::byte{0});
            std::memcpy(paddedChunk.data(), m_buffer.data(), m_buffer.size());

            const int64_t trueTotal = m_totalRows + remainingRows;
            writeChunk(paddedChunk.data(), m_chunkSize); // increments m_totalRows by m_chunkSize
            m_totalRows = trueTotal;                     // correct back to actual row count
        }
    }

    m_buffer.clear();

    if (m_cctx != nullptr) {
        ZSTD_freeCCtx(m_cctx);
        m_cctx = nullptr;
    }

    // Write the final shard index, then close the file.
    if (m_shardFile.isOpen()) {
        const qint64 idxWritten = m_shardFile.write(
            reinterpret_cast<const char *>(m_indexBuffer.data()),
            static_cast<qint64>(m_indexBuffer.size()));
        if (idxWritten != static_cast<qint64>(m_indexBuffer.size()))
            setError(QStringLiteral("Short write of final shard index: ") + m_shardFile.errorString());
        m_shardFile.close();
    }
    m_indexBuffer.clear();

    // Force a zarr.json rewrite with the final, corrected m_totalRows
    // (which may differ from what the last Tier-B checkpoint wrote).
    if (!writeMetadata()) {
        setError(QStringLiteral("Failed to write final Zarr array metadata"));
        return false;
    }

    return !m_hasError;
}

int64_t ZarrV3Array::totalRows() const
{
    return m_totalRows;
}

bool ZarrV3Array::hasError() const
{
    return m_hasError;
}

QString ZarrV3Array::errorMessage() const
{
    return m_errorMessage;
}

void ZarrV3Array::setError(const QString &msg)
{
    if (m_hasError)
        return; // keep the first error - it's usually the root cause
    m_hasError = true;
    m_errorMessage = msg;
}

bool ZarrV3Array::writeChunk(const void *data, int64_t nRows)
{
    if (!m_shardFile.isOpen()) {
        setError(QStringLiteral("writeChunk called with shard file closed"));
        return false;
    }

    const size_t srcSize = static_cast<size_t>(nRows) * m_nCols * m_typeSize;
    const size_t csize = ZSTD_compress2(m_cctx, m_compressedScratch.data(), m_compressedScratch.size(), data, srcSize);
    if (ZSTD_isError(csize)) {
        setError(QStringLiteral("ZSTD compression failed: ") + QString::fromUtf8(ZSTD_getErrorName(csize)));
        return false;
    }

    // Write compressed data first
    const qint64 written = m_shardFile.write(
        reinterpret_cast<const char *>(m_compressedScratch.data()),
        static_cast<qint64>(csize));
    if (written != static_cast<qint64>(csize)) {
        setError(
            QStringLiteral("Short write to shard file (%1 of %2 bytes): ").arg(written).arg(csize)
            + m_shardFile.errorString());
        return false;
    }

    // Commit the (byte_offset, byte_length) index entry as raw little-endian bytes.
    const uint64_t indexOffset = m_shardOffset;
    const uint64_t indexLength = static_cast<uint64_t>(csize);
    const auto *offsetBytes = reinterpret_cast<const std::byte *>(&indexOffset);
    const auto *lengthBytes = reinterpret_cast<const std::byte *>(&indexLength);
    m_indexBuffer.insert(m_indexBuffer.end(), offsetBytes, offsetBytes + sizeof(uint64_t));
    m_indexBuffer.insert(m_indexBuffer.end(), lengthBytes, lengthBytes + sizeof(uint64_t));

    m_shardOffset += csize;
    m_totalRows += nRows;
    m_chunkIdx++;

    writeCheckpoint();
    return !m_hasError;
}

void ZarrV3Array::writeCheckpoint()
{
    // Tier A: write the current shard index immediately after the compressed
    // data so the shard is always self-consistent on disk. We then seek back
    // to m_shardOffset so the cursor invariant holds.
    if (m_shardFile.isOpen()) {
        if (!m_shardFile.seek(static_cast<qint64>(m_shardOffset))) {
            setError(QStringLiteral("Failed to seek shard file for index write: ") + m_shardFile.errorString());
            return;
        }
        const qint64 idxWritten = m_shardFile.write(
            reinterpret_cast<const char *>(m_indexBuffer.data()),
            static_cast<qint64>(m_indexBuffer.size()));
        if (idxWritten != static_cast<qint64>(m_indexBuffer.size())) {
            setError(QStringLiteral("Invalid amount of bytes written to shard index: ") + m_shardFile.errorString());
            return;
        }
        m_shardFile.flush();
        if (!m_shardFile.seek(static_cast<qint64>(m_shardOffset))) {
            setError(QStringLiteral("Failed to restore shard cursor after checkpoint: ") + m_shardFile.errorString());
            return;
        }
    }

    // Tier B: rewrite zarr.json every kMetadataCheckpointMinChunks chunks or
    // kMetadataCheckpointMinSecs seconds, whichever comes first.
    ++m_chunksSinceMeta;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsedSecs = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastMetaCheckpoint).count();
    if (m_chunksSinceMeta >= kMetadataCheckpointMinChunks || elapsedSecs >= kMetadataCheckpointMinSecs) {
        if (!writeMetadata()) {
            setError(QStringLiteral("Failed to write Zarr array metadata"));
            return;
        }
        m_chunksSinceMeta = 0;
        m_lastMetaCheckpoint = now;
    }
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
    case DType::Int32:
        dtypeStr = QStringLiteral("int32");
        break;
    case DType::UInt16:
        dtypeStr = QStringLiteral("uint16");
        break;
    case DType::UInt64:
        dtypeStr = QStringLiteral("uint64");
        break;
    case DType::Float32:
        dtypeStr = QStringLiteral("float32");
        break;
    case DType::Float64:
        dtypeStr = QStringLiteral("float64");
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
    if (m_dtype == DType::Float64 || m_dtype == DType::Float32)
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

bool zarrWriteRootGroupMetadata(const fs::path &storePath)
{
    std::error_code ec;
    fs::create_directory(storePath, ec);
    if (ec)
        return false;

    QJsonObject meta;
    meta["zarr_format"] = 3;
    meta["node_type"] = QStringLiteral("group");
    meta["attributes"] = QJsonObject();

    QFile f(QString::fromStdString(storePath.string() + "/zarr.json"));
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(meta).toJson());
    f.close();
    return true;
}
