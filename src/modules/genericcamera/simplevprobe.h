/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIMPLEVPROBE_HPP
#define SIMPLEVPROBE_HPP

#include <QAbstractVideoSurface>
#include <QList>

class QCamera;
class QCameraViewfinder;

class SimpleVProbe : public QAbstractVideoSurface
{
    Q_OBJECT

private:
    QCamera *source;
public:
    explicit SimpleVProbe(QObject *parent = nullptr);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const override;

    // Called from QAbstractVideoSurface whenever a new frame is present
    bool present(const QVideoFrame &frame) Q_DECL_OVERRIDE;

    bool setSource(QCamera *source);

    bool isActive() const;

signals:
    void videoFrameProbed(const QVideoFrame &videoFrame);
    void flush();

};

#endif // SIMPLEVPROBE_HPP
