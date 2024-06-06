/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2014 Jure Varlec <jure.varlec@ad-vega.si>

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

#ifndef BAYER_H
#define BAYER_H

#include "bayer/decoder.h"

// Some formats appeared only after aravis-0.2.0, so
// we check for their presence. The 12_PACKED formats
// were added individually. The required ifdefs are in
// the respective headers.

#include "bayer/BayerBG10.h"
#include "bayer/BayerBG12.h"
#include "bayer/BayerBG12_PACKED.h"
#include "bayer/BayerBG16.h"
#include "bayer/BayerBG8.h"
#include "bayer/BayerGB10.h"
#include "bayer/BayerGB12.h"
#include "bayer/BayerGB12_PACKED.h"
#include "bayer/BayerGB16.h"
#include "bayer/BayerGB8.h"
#include "bayer/BayerGR10.h"
#include "bayer/BayerGR12.h"
#include "bayer/BayerGR12_PACKED.h"
#include "bayer/BayerGR16.h"
#include "bayer/BayerGR8.h"
#include "bayer/BayerRG10.h"
#include "bayer/BayerRG12.h"
#include "bayer/BayerRG12_PACKED.h"
#include "bayer/BayerRG16.h"
#include "bayer/BayerRG8.h"

#endif
