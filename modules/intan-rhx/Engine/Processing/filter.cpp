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

#include <cmath>
#include <iostream>
#include "rhxglobals.h"
#include "filter.h"

// Filter an array of data.
void Filter::filter(const float* in, float* out, unsigned int length)
{
    float* outEnd = out + length;
    for (; out != outEnd; ++out) {
        *out = filterOne(*in);
        ++in;
    }
}

// Filter an array of data and return difference as well.  If the filter is a lowpass filter with
// cutoff frequency fc, then inMinusOut will act as a highpass filter with cutoff frequency fc.
void Filter::filter(const float* in, float* out, float* inMinusOut, unsigned int length)
{
    float* outEnd = out + length;
    for (; out != outEnd; ++out) {
        *out = filterOne(*in);
        *inMinusOut = *in - *out;
        ++inMinusOut;
        ++in;
    }
}

// Filter an array of data and overwrite input.
void Filter::filter(float* inout, unsigned int length)
{
    float* inoutEnd = inout + length;
    for (; inout != inoutEnd; ++inout) {
        *inout = filterOne(*inout);
    }
}


BiquadFilter::BiquadFilter()
{
    reset();
}

BiquadFilter::~BiquadFilter()
{

}

float BiquadFilter::filterOne(float in)
{
    if (firstTime) {
        prevIn = in;
        prevPrevIn = in;
        if (isDcGainZero) {
            prevOut = 0.0;
            prevPrevOut = 0.0;
        } else {
            prevOut = in;
            prevPrevOut = in;
        }
        firstTime = false;
    }
    // Direct Form 1 implementation
    float out = b0 * in + b1 * prevIn + b2 * prevPrevIn - a1 * prevOut - a2 * prevPrevOut;
    // TODO: Compare to Direct Form 2 implementation (see Wikipedia page on "Digital biquad filter").

    prevPrevIn = prevIn;
    prevIn = in;
    prevPrevOut = prevOut;
    prevOut = out;

    return out;
}

void BiquadFilter::reset()
{
    firstTime = true;
}

float BiquadFilter::getA1() const
{
    return a1;
}

float BiquadFilter::getA2() const
{
    return a2;
}

float BiquadFilter::getB0() const
{
    return b0;
}

float BiquadFilter::getB1() const
{
    return b1;
}

float BiquadFilter::getB2() const
{
    return b2;
}


FirstOrderLowpassFilter::FirstOrderLowpassFilter(double fc, double sampleRate)
{
    isDcGainZero = false;
    float k = (float) exp(-TwoPi * fc / sampleRate);
    b0 = 1.0F - k;
    b1 = 0.0F;
    b2 = 0.0F;
    a1 = -k;
    a2 = 0.0F;
}


FirstOrderHighpassFilter::FirstOrderHighpassFilter(double fc, double sampleRate)
{
    isDcGainZero = true;
    float k = (float) exp(-TwoPi * fc / sampleRate);
    b0 = 1.0F;
    b1 = -1.0F;
    b2 = 0.0F;
    a1 = -k;
    a2 = 0.0F;
}


SecondOrderLowpassFilter::SecondOrderLowpassFilter(double fc, double q, double sampleRate)
{
    isDcGainZero = false;
    double k = tan(Pi * fc / sampleRate);
    double norm = 1.0 / (1.0 + k / q + k * k);
    b0 = (float)(k * k * norm);
    b1 = (float)(2.0 * k * k * norm);
    b2 = b0;
    a1 = (float)(2.0 * (k * k - 1.0) * norm);
    a2 = (float)((1.0 - k / q + k * k) * norm);
}


SecondOrderHighpassFilter::SecondOrderHighpassFilter(double fc, double q, double sampleRate)
{
    isDcGainZero = true;
    double k = tan(Pi * fc / sampleRate);
    double norm = 1.0 / (1.0 + k / q + k * k);
    b0 = (float)norm;
    b1 = (float)(-2.0 * norm);
    b2 = b0;
    a1 = (float)(2.0 * (k * k - 1.0) * norm);
    a2 = (float)((1.0 - k / q + k * k) * norm);
}


SecondOrderNotchFilter::SecondOrderNotchFilter(double fNotch, double bandwidth, double sampleRate)
{
    isDcGainZero = false;
    double d = exp(-1.0 * Pi * bandwidth / sampleRate);
    double b = (1.0 + d * d) * cos(TwoPi * fNotch / sampleRate);
    b0 = (float)((1.0 + d * d) / 2.0);
    b1 = (float)(-b);
    b2 = b0;
    a1 = b1;
    a2 = (float)(d * d);
}


HighOrderFilter::~HighOrderFilter()
{
}

float HighOrderFilter::filterOne(float in)
{
    float out = in;
    for (Filter& f : filters) {
        out = f.filterOne(out);
    }
    return out;
}

void HighOrderFilter::reset()
{
    for (Filter& f : filters) {
        f.reset();
    }
}


vector<BiquadFilter> HighOrderFilter::getFilters() const
{
    return filters;
}


NotchFilter::NotchFilter(double fNotch, double bandwidth, double sampleRate)
{
    filters = {
        SecondOrderNotchFilter(fNotch, bandwidth, sampleRate)
    };
}


BesselLowpassFilter::BesselLowpassFilter(unsigned int order, double fc, double sampleRate)
{
    switch (order) {
    case 1:
        filters = {
            FirstOrderLowpassFilter(fc, sampleRate)
        };
        break;
    case 2:
        filters = {
            SecondOrderLowpassFilter(fc * 1.2736, 0.5773, sampleRate)
        };
        break;
    case 3:
        filters = {
            FirstOrderLowpassFilter(fc * 1.3270, sampleRate),
            SecondOrderLowpassFilter(fc * 1.4524, 0.6910, sampleRate)
        };
        break;
    case 4:
        filters = {
            SecondOrderLowpassFilter(fc * 1.4192, 0.5219, sampleRate),
            SecondOrderLowpassFilter(fc * 1.5912, 0.8055, sampleRate)
        };
        break;
    case 5:
        filters = {
            FirstOrderLowpassFilter(fc * 1.5069, sampleRate),
            SecondOrderLowpassFilter(fc * 1.5611, 0.5635, sampleRate),
            SecondOrderLowpassFilter(fc * 1.7607, 0.9165, sampleRate)
        };
        break;
    case 6:
        filters = {
            SecondOrderLowpassFilter(fc * 1.6060, 0.5103, sampleRate),
            SecondOrderLowpassFilter(fc * 1.6913, 0.6112, sampleRate),
            SecondOrderLowpassFilter(fc * 1.9071, 1.0234, sampleRate)
        };
        break;
    case 7:
        filters = {
            FirstOrderLowpassFilter(fc * 1.6853, sampleRate),
            SecondOrderLowpassFilter(fc * 1.7174, 0.5324, sampleRate),
            SecondOrderLowpassFilter(fc * 1.8235, 0.6608, sampleRate),
            SecondOrderLowpassFilter(fc * 2.0507, 1.1262, sampleRate)
        };
        break;
    case 8:
        filters = {
            SecondOrderLowpassFilter(fc * 1.7837, 0.5060, sampleRate),
            SecondOrderLowpassFilter(fc * 1.8376, 0.5596, sampleRate),
            SecondOrderLowpassFilter(fc * 1.9591, 0.7109, sampleRate),
            SecondOrderLowpassFilter(fc * 2.1953, 1.2258, sampleRate)
        };
        break;
    default:
        cerr << "BesselLowpassFilter: order is not valid." << '\n';
    }
}


BesselHighpassFilter::BesselHighpassFilter(unsigned int order, double fc, double sampleRate)
{
    switch (order) {
    case 1:
        filters = {
            FirstOrderHighpassFilter(fc, sampleRate)
        };
        break;
    case 2:
        filters = {
            SecondOrderHighpassFilter(fc / 1.2736, 0.5773, sampleRate)
        };
        break;
    case 3:
        filters = {
            FirstOrderHighpassFilter(fc / 1.3270, sampleRate),
            SecondOrderHighpassFilter(fc / 1.4524, 0.6910, sampleRate)
        };
        break;
    case 4:
        filters = {
            SecondOrderHighpassFilter(fc / 1.4192, 0.5219, sampleRate),
            SecondOrderHighpassFilter(fc / 1.5912, 0.8055, sampleRate)
        };
        break;
    case 5:
        filters = {
            FirstOrderHighpassFilter(fc / 1.5069, sampleRate),
            SecondOrderHighpassFilter(fc / 1.5611, 0.5635, sampleRate),
            SecondOrderHighpassFilter(fc / 1.7607, 0.9165, sampleRate)
        };
        break;
    case 6:
        filters = {
            SecondOrderHighpassFilter(fc / 1.6060, 0.5103, sampleRate),
            SecondOrderHighpassFilter(fc / 1.6913, 0.6112, sampleRate),
            SecondOrderHighpassFilter(fc / 1.9071, 1.0234, sampleRate)
        };
        break;
    case 7:
        filters = {
            FirstOrderHighpassFilter(fc / 1.6853, sampleRate),
            SecondOrderHighpassFilter(fc / 1.7174, 0.5324, sampleRate),
            SecondOrderHighpassFilter(fc / 1.8235, 0.6608, sampleRate),
            SecondOrderHighpassFilter(fc / 2.0507, 1.1262, sampleRate)
        };
        break;
    case 8:
        filters = {
            SecondOrderHighpassFilter(fc / 1.7837, 0.5060, sampleRate),
            SecondOrderHighpassFilter(fc / 1.8376, 0.5596, sampleRate),
            SecondOrderHighpassFilter(fc / 1.9591, 0.7109, sampleRate),
            SecondOrderHighpassFilter(fc / 2.1953, 1.2258, sampleRate)
        };
        break;
    default:
        cerr << "BesselHighpassFilter: order is not valid." << '\n';
    }
}


ButterworthLowpassFilter::ButterworthLowpassFilter(unsigned int order, double fc, double sampleRate)
{
    switch (order) {
    case 1:
        filters = {
            FirstOrderLowpassFilter(fc, sampleRate)
        };
        break;
    case 2:
        filters = {
            SecondOrderLowpassFilter(fc, 0.7071, sampleRate)
        };
        break;
    case 3:
        filters = {
            FirstOrderLowpassFilter(fc, sampleRate),
            SecondOrderLowpassFilter(fc, 1.0000, sampleRate)
        };
        break;
    case 4:
        filters = {
            SecondOrderLowpassFilter(fc, 1.3065, sampleRate),
            SecondOrderLowpassFilter(fc, 0.5412, sampleRate)
        };
        break;
    case 5:
        filters = {
            FirstOrderLowpassFilter(fc, sampleRate),
            SecondOrderLowpassFilter(fc, 0.6180, sampleRate),
            SecondOrderLowpassFilter(fc, 1.6181, sampleRate)
        };
        break;
    case 6:
        filters = {
            SecondOrderLowpassFilter(fc, 0.5177, sampleRate),
            SecondOrderLowpassFilter(fc, 0.7071, sampleRate),
            SecondOrderLowpassFilter(fc, 1.9320, sampleRate)
        };
        break;
    case 7:
        filters = {
            FirstOrderLowpassFilter(fc, sampleRate),
            SecondOrderLowpassFilter(fc, 0.5549, sampleRate),
            SecondOrderLowpassFilter(fc, 0.8019, sampleRate),
            SecondOrderLowpassFilter(fc, 2.2472, sampleRate)
        };
        break;
    case 8:
        filters = {
            SecondOrderLowpassFilter(fc, 0.5098, sampleRate),
            SecondOrderLowpassFilter(fc, 0.6013, sampleRate),
            SecondOrderLowpassFilter(fc, 0.8999, sampleRate),
            SecondOrderLowpassFilter(fc, 2.5628, sampleRate)
        };
        break;
    default:
        cerr << "ButterworthLowpassFilter: order is not valid." << '\n';
    }
}


ButterworthHighpassFilter::ButterworthHighpassFilter(unsigned int order, double fc, double sampleRate)
{
    switch (order) {
    case 1:
        filters = {
            FirstOrderHighpassFilter(fc, sampleRate)
        };
        break;
    case 2:
        filters = {
            SecondOrderHighpassFilter(fc, 0.7071, sampleRate)
        };
        break;
    case 3:
        filters = {
            FirstOrderHighpassFilter(fc, sampleRate),
            SecondOrderHighpassFilter(fc, 1.0000, sampleRate)
        };
        break;
    case 4:
        filters = {
            SecondOrderHighpassFilter(fc, 1.3065, sampleRate),
            SecondOrderHighpassFilter(fc, 0.5412, sampleRate)
        };
        break;
    case 5:
        filters = {
            FirstOrderHighpassFilter(fc, sampleRate),
            SecondOrderHighpassFilter(fc, 0.6180, sampleRate),
            SecondOrderHighpassFilter(fc, 1.6181, sampleRate)
        };
        break;
    case 6:
        filters = {
            SecondOrderHighpassFilter(fc, 0.5177, sampleRate),
            SecondOrderHighpassFilter(fc, 0.7071, sampleRate),
            SecondOrderHighpassFilter(fc, 1.9320, sampleRate)
        };
        break;
    case 7:
        filters = {
            FirstOrderHighpassFilter(fc, sampleRate),
            SecondOrderHighpassFilter(fc, 0.5549, sampleRate),
            SecondOrderHighpassFilter(fc, 0.8019, sampleRate),
            SecondOrderHighpassFilter(fc, 2.2472, sampleRate)
        };
        break;
    case 8:
        filters = {
            SecondOrderHighpassFilter(fc, 0.5098, sampleRate),
            SecondOrderHighpassFilter(fc, 0.6013, sampleRate),
            SecondOrderHighpassFilter(fc, 0.8999, sampleRate),
            SecondOrderHighpassFilter(fc, 2.5628, sampleRate)
        };
        break;
    default:
        cerr << "ButterworthHighpassFilter: order is not valid." << '\n';
    }
}
