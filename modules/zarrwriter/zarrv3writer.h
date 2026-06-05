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

#pragma once

#include <chrono>
#include <cstdint>
#include <expected>

#include <QFile>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <filesystem>

#include <zstd.h>

#include "datactl/binarystream.h"

namespace fs = std::filesystem;

/**
 * @brief Incrementally write a single Zarr v3 array to a filesystem store.
 *
 * Data is appended in row-major byte layout. The shard's trailing index and
 * zarr.json are checkpointed during the run so a crash-truncated store stays
 * readable by stock zarr-python.
 *
 * On I/O failure the array enters a sticky error state (hasError() / errorMessage());
 * subsequent appendBytes() calls become no-ops so the caller can detect the
 * failure and propagate it.
 */
class ZarrV3Array
{
public:
    enum class DType {
        Int32,
        UInt16,
        UInt32,
        UInt64,
        Float32,
        Float64
    };

    ZarrV3Array(
        const QString &storeDir,
        const QString &arrayName,
        DType dtype,
        int64_t chunkSize,
        int nCols,
        QStringList dimNames = {});

    /**
     * Create the on-disk directory layout and open the shard file for writing.
     * Must be called once before appendBytes().
     * Returns an error string on failure.
     */
    [[nodiscard]] std::expected<void, QString> open();

    void setAttributes(const QJsonObject &attrs);

    /**
     * Append @p nRows rows of data supplied as row-major bytes.
     * Flushes complete inner chunks to the shard file immediately.
     */
    void appendBytes(const void *data, int64_t nRows);

    /**
     * Flush remaining buffered data, write the shard index, and write
     * the zarr.json metadata document.
     */
    bool finalize();

    [[nodiscard]] int64_t totalRows() const;

    /**
     * Whether the writer entered a sticky I/O error state. Once set,
     * further appendBytes() / finalize() calls become no-ops apart from
     * cleanup. The caller should propagate errorMessage() to the user.
     */
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] QString errorMessage() const;

private:
    bool writeChunk(const void *data, int64_t nRows);
    bool writeMetadata();
    void writeCheckpoint();
    void setError(const QString &msg);

    // Checkpoints so we don't lose Zarr data in a crash (of course, Syntalos never crashes,
    // but just in case...): Tier-A (index flush): every chunk. Tier-B (zarr.json rewrite):
    // every kJsonCheckpointEveryChunks chunks OR kJsonCheckpointEverySecs seconds.
    static constexpr int kMetadataCheckpointMinChunks = 16;
    static constexpr int kMetadataCheckpointMinSecs = 30;

    QString m_arrayDir;
    DType m_dtype;
    int64_t m_chunkSize;
    int m_nCols;
    QStringList m_dimNames;
    int m_typeSize;

    // Shard file kept open for the entire write lifecycle.
    // 1-D arrays: <arrayDir>/c/0
    // 2-D arrays: <arrayDir>/c/0/0
    QFile m_shardFile;

    // ZSTD compression context
    ZSTD_CCtx *m_cctx;

    // Shard index: one (byte_offset, byte_length) pair per inner chunk
    // stored as raw little-endian uint64_t pairs. Written incrementally after
    // each chunk (Tier A) and finalised on finalize().
    Syntalos::ByteVector m_indexBuffer;
    uint64_t m_shardOffset; // current end of compressed data in the shard file

    // Rolling accumulation buffer for incoming uncompressed data.
    // Flushed chunks are erased immediately so this stays bounded to <= 1 chunk.
    Syntalos::ByteVector m_buffer;
    int64_t m_totalRows;
    int64_t m_chunkIdx;

    // Tier-B checkpoint tracking
    int64_t m_chunksSinceMeta;
    std::chrono::steady_clock::time_point m_lastMetaCheckpoint;

    // Reusable destination buffer for ZSTD output, sized once in open() to
    // ZSTD_compressBound(chunkBytes). Avoids per-chunk malloc/free churn.
    Syntalos::ByteVector m_compressedScratch;

    bool m_hasError;
    QString m_errorMessage;

    QJsonObject m_attributes;
};

/**
 * @brief Write the Zarr v3 root group metadata file (zarr.json) into @p storePath.
 *
 * Creates the store directory if it does not exist.
 */
bool zarrWriteRootGroupMetadata(const fs::path &storePath);
