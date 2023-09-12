Tsync binary file structure
###########################

.. note::
    This document describes the technical details of the tsync binary format.
    It is not required to know this information if you just want to read `tsync` files,
    for that, just use `edlio <https://edl.rtfd.io>`_ which implements the necessary
    code to read the data.


Intro
=====

The tsync binary file format was created to safely and efficiently store time synchronization
information of scientific experiments.

A tsync file can exist in two *modes*: *Continuous*, where the file contains a continuous list of
timestamps, and *SyncPoints* where it only contains synchronization information if the devices got
out of sync and need to be manually resynchronized in post-processing.


Constants
=========

The following constants are in use in tsync files and will be referenced in the descriptions below:

.. code-block:: cpp

    // TSYNC file magic number (saved as LE): 8A T S Y N C ⏲
    #define TSYNC_FILE_MAGIC 0xF223434E5953548A

    #define TSYNC_FILE_VERSION_MAJOR 1
    #define TSYNC_FILE_VERSION_MINOR 2

    #define TSYNC_FILE_BLOCK_TERM 0x1126000000000000

.. code-block:: cpp

    /**
     * Timepoint storage of a TSync file
     */
    enum class TSyncFileMode {
        CONTINUOUS = 0, /// Continous time-point mapping with no gaps
        SYNCPOINTS = 1  /// Only synchronization points are saved
    };

    /**
     * Unit types for time representation in a TSync file
     */
    enum class TSyncFileTimeUnit {
        INDEX = 0, /// monotonically increasing counter without dimension
        NANOSECONDS = 1,
        MICROSECONDS = 2,
        MILLISECONDS = 3,
        SECONDS = 4
    };

    /**
     * Data types use for storing time values in the data file.
     */
    enum class TSyncFileDataType {
        INVALID = 0,
        INT16 = 2,
        INT32 = 3,
        INT64 = 4,

        UINT16 = 6,
        UINT32 = 7,
        UINT64 = 8
    };

Data blocks are hashed using the `XXH3 <http://fastcompression.blogspot.com/2019/03/presenting-xxh3.html>`_
hashing algorithm to ensure data has not been accidentally corrupted.


Header
======

All data in a tsync file are stored in as little-endian.
All values written to the header block are hashed using the XXH3 hashing algorithm.

A tsync file always begins with magic number ``TSYNC_FILE_MAGIC`` as a ``uint64`` number, followed
by ``TSYNC_FILE_VERSION_MAJOR`` and ``TSYNC_FILE_VERSION_MINOR`` as ``uint64`` as well.
These two alues are followed by the creation date of the tsync file as UNIX timestamp in ``int64`` format.

These values are followed by the module name that created the tsync file as null-terminated UTF-8 encoded string.
The module name is followed by the experiment collection ID (EDL collection ID) as null-terminated UTF-8 encoded
string as well.

Following these values, optional JSON metadata can be inserted as null-terminated string. If there is no JSON data,
the previous value is just followed by another null byte.

Following is the enum value for the file's mode, ``TSyncFileMode`` (``0`` for continuous, ``1`` for syncpoints) as
``uint16``.

This value is followed by the tsync file's block size (``block_size``) as ``int32`` value. A block is a grouping of values that
are checksummed together. The block sizes denotes the count of values each block contains. This value is usually
128 or 256, but could be any value depending on the desired speed/robustness/size tradeoffs.

The block size is followed by the name of the first time as UTF-8, zero-terminated string. This could be a value
like ``"master clock"``. After this value follows the time unit ID ``TSyncFileTimeUnit`` as ``uint16`` value to
denote the unit of measurement, followed by the byte size of the values this data unit is stored in as denoted
by the ``TSyncFileDataType`` enum, as ``uint16``.

This block of definitions for the first clock to synchronize with is followed by a set of the same values, for
the secondary clock.

The header is then padded with zero-bytes to be 8-byte aligned. The padding data is hashed as well.

After padding, the header block is finalized by writing the block terminator magic number ``TSYNC_FILE_BLOCK_TERM``
as ``uint64`` value, followed by the XXH3 digest of the just created header as ``uint64``.
The XXH3 hash sum is then reset for the following blocks.

The header is followed by data blocks containing the actual sync data.


Data Blocks
===========

A block contains the time value of the first clock, written in the previously denoted integer size ``TSyncFileDataType``,
followed by the value of the second clock. Both values are hashed, and the checksum is kept and updated using the following values.

If values of the previously defined ``block_size`` amount have been written, the block is finalized by writing a
``TSYNC_FILE_BLOCK_TERM`` terminator as ``uint64`` at the given position, followed by the XXH3 checksum as ``ùint64``.
After writing the block, the rolling checksum is reset, and the next block is written.
(This allows to pinpoint and ignore a damaged block, if the file gets corrupted, without loosing all data).

If the file is to be finished, but the last block did not end with a terminator yet, a ``TSYNC_FILE_BLOCK_TERM``
terminator value and the respective checksum is written enayway, ignoring the block size, so that a complete tsync file
always ends with a terminator+checksum combination.
