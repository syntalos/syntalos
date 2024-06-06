/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QARVRECORDEDVIDEO_H
#define QARVRECORDEDVIDEO_H

#include "qarvdecoder.h"
#include <QString>
#include <QFile>

#pragma GCC visibility push(default)

//! QArvRecordedVideo provides a means of opening a video description file.
class QArvRecordedVideo {

    class QArvRecordedVideoExtension;

public:
    //! Opens the description file with the given filename.
    QArvRecordedVideo(const QString& filename);

    //! Opens a raw video file directly. Only supports uncompressed files.
    QArvRecordedVideo(const QString& filename, enum AVPixelFormat swsFmt,
                      uint headerBytes, QSize fsize);

    //! Returns true if the file has been opened successfully.
    bool status();

    //! Returns the error status of the underlying QFile.
    /*
     * If this function returns NoError but status() returns false
     * then the description file could not be read.
     */
    QFile::FileError error();

    //! Returns the error string of the underlying QFile.
    QString errorString();

    //! Returns true if we are at the end of the underlying QFile.
    bool atEnd();

    //! Reads a single frame and advances to the next.
    /*!
     * Returns an empty QByteArray on error.
     */
    QByteArray read();

    //! Seeks to the provided frame number, if possible.
    /*!
     * Returns false on error.
     */
    bool seek(quint64 frame);

    //! Returns true if video file is seekable.
    /*!
     * A file might not be seekable e.g. if it is compressed.
     */
    bool isSeekable();

    //! Return the number of frames for a seekable file.
    uint numberOfFrames();

    //! Returns a decoder for decoding read frames.
    QArvDecoder* makeDecoder();

    //! Returns the frame size.
    QSize frameSize();

    //! Returns the byte size of a frame.
    uint frameBytes();

    //! Returns the nominal frame rate.
    /*
     * This is the frame rate that the user chose in the GUI. The actual frame
     * rate is lower than that. If precise timing is required, it is suggested
     * to record frame timestamps as well.
     */
    int framerate();

private:
    QArvRecordedVideoExtension* ext;
    QFile videofile;
    QSize fsize;
    int fps;
    bool uncompressed, isOK;
    ArvPixelFormat arvPixfmt;
    enum AVPixelFormat swscalePixfmt;
    uint frameBytes_;
};

#pragma GCC visibility pop

#endif
