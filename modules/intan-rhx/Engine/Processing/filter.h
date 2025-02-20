//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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
#ifndef FILTER_H
#define FILTER_H

#include <vector>

// Base class for all filters.
// We use float instead of double for input/output values to reduce storage space in the theory that
// moving data to and from memory is likely the dominant speed bottleneck.
class Filter
{
public:
    //virtual ~Filter();
    void filter(const float* in, float* out, unsigned int length); // filter an array of data
    void filter(const float* in, float* out, float* inMinusOut, unsigned int length); // filter an array of data and return difference as well
    void filter(float* inout, unsigned int length); // filter an array of data and overwrite
    virtual float filterOne(float in) = 0;  // filter one data point
    virtual void reset() = 0;   // reset filter state
};


// Implement a digital biquad filter with a transfer function
//
//           b0 + b1 z^-1 + b2 z^-2
//   H(z)  = ----------------------
//           a0 + a1 z^-1 + a2 z^-2
//
// Coefficients are normalized so that a[0] = 1.

class BiquadFilter : public Filter
{
public:
    BiquadFilter();
    virtual ~BiquadFilter();
    float filterOne(float in) override;
    void reset() override;
    float getA1() const;
    float getA2() const;
    float getB0() const;
    float getB1() const;
    float getB2() const;

protected:
    // We are not using arrays (e.g., float b[3]) in hopes that using non-arrayed variables improves speed.
    float a1;   // Filter coefficients.  'a0' is assumed to equal 1.
    float a2;
    float b0;
    float b1;
    float b2;
    bool isDcGainZero;  // Set true for highpass and bandpass filters; false for lowpass and notch filters

private:
    float prevIn;       // input at time t-1
    float prevPrevIn;   // input at time t-2
    float prevOut;      // output at time t-1
    float prevPrevOut;  // output at time t-2
    bool firstTime;
};

class FirstOrderLowpassFilter : public BiquadFilter
{
public:
    FirstOrderLowpassFilter(double fc, double sampleRate);
};

class FirstOrderHighpassFilter : public BiquadFilter
{
public:
    FirstOrderHighpassFilter(double fc, double sampleRate);
};

class SecondOrderLowpassFilter : public BiquadFilter
{
public:
    SecondOrderLowpassFilter(double fc, double q, double sampleRate);
};

class SecondOrderHighpassFilter : public BiquadFilter
{
public:
    SecondOrderHighpassFilter(double fc, double q, double sampleRate);
};

class SecondOrderNotchFilter : public BiquadFilter
{
public:
    SecondOrderNotchFilter(double fNotch, double bandwidth, double sampleRate);
};

class HighOrderFilter : public Filter
{
public:
    virtual ~HighOrderFilter();
    float filterOne(float in) override;
    void reset() override;
    std::vector<BiquadFilter> getFilters() const;

protected:
    std::vector<BiquadFilter> filters;
};

class NotchFilter : public HighOrderFilter
{
public:
    NotchFilter(double fNotch, double bandwidth, double sampleRate);
};

class BesselLowpassFilter : public HighOrderFilter
{
public:
    BesselLowpassFilter(unsigned int order, double fc, double sampleRate);
};

class BesselHighpassFilter : public HighOrderFilter
{
public:
    BesselHighpassFilter(unsigned int order, double fc, double sampleRate);
};

class ButterworthLowpassFilter : public HighOrderFilter
{
public:
    ButterworthLowpassFilter(unsigned int order, double fc, double sampleRate);
};

class ButterworthHighpassFilter : public HighOrderFilter
{
public:
    ButterworthHighpassFilter(unsigned int order, double fc, double sampleRate);
};

#endif // FILTER_H
