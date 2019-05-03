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

#ifndef IMAGESOURCEMODULE_H
#define IMAGESOURCEMODULE_H

#include <chrono>
#include <opencv2/core.hpp>

#include "abstractmodule.h"

class ImageSourceModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit ImageSourceModule(QObject *parent = nullptr);

//public slots:
//    virtual void receiveFrame(const cv::Mat& frame, const std::chrono::milliseconds& timestamp);
};

#endif // IMAGESOURCEMODULE_H
