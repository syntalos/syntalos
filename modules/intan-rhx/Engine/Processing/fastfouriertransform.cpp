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
#include <limits>
#include "rhxglobals.h"
#include "fastfouriertransform.h"

using namespace std;

FastFourierTransform::FastFourierTransform(float sampleRate_, unsigned int length_, WindowFunction function_) :
    sampleRate(sampleRate_),
    length(length_),
    function(function_),
    window(nullptr),
    logPsd(nullptr),
    frequency(nullptr)
{
    setLength(length);
}

FastFourierTransform::~FastFourierTransform()
{
    if (window) delete [] window;
    if (logPsd) delete [] logPsd;
    if (frequency) delete [] frequency;
}

void FastFourierTransform::setLength(int length_)
{
    length = length_;
    createWindow();
    createPsdVector();
    createFrequencyVector();
}

void FastFourierTransform::createWindow()
{
    if (window) delete [] window;
    window = new float [length];

    unsigned int i, j;
    float nDiv2 = (float)(length >> 1);
    float nMinus1DivTwoPi = (float)(length - 1) / TwoPiF;
    float value;

    switch (function) {
    case WindowRectangular:
        for (i = 0; i < length; ++i) {
            window[i] = 1.0F;
        }
        break;
    case WindowTriangular:
        for (i = 0, j = length - 1; i < (length >> 1); ) {
            value = (float)(i + 1) / nDiv2;
            window[i++] = value;
            window[j--] = value;
        }
        break;
    case WindowHann:
        for (i = 0; i < length; ++i) {
            window[i] = 0.5F * (1.0F - cos((float)i / nMinus1DivTwoPi));
        }
        break;
    case WindowHamming:
        for (i = 0; i < length; ++i) {
            window[i] = 0.54F - 0.46F * cos((float)i / nMinus1DivTwoPi);
        }
        break;
    }
}

void FastFourierTransform::createPsdVector()
{
    if (logPsd) delete [] logPsd;
    logPsd = new float [(length >> 1) + 1];
}

void FastFourierTransform::createFrequencyVector()
{
    if (frequency) delete [] frequency;
    unsigned int n = (length >> 1) + 1;
    frequency = new float [n];

    float deltaF = sampleRate / (float)length;
    float f = 0.0F;
    for (unsigned int i = 0; i < n; ++i) {
        frequency[i] = f;
        f += deltaF;
    }
}

// Perform an FFT of an array of n complex numbers, where n must be a power of two.
// The complex numbers are stored in data, an array of length 2n, where
// data[0] = input_real[t]
// data[1] = input_imaginary[t]
// data[2] = input_real[t+1]
// data[3] = input_imaginary[t+1]
// etc...
// The complex FFT is returned in the same format, overwriting data.
void FastFourierTransform::complexInputFft(float *data, unsigned int n)
{
    // Reverse-binary reindexing
    unsigned int m;
    unsigned int nTimes2 = n << 1;
    unsigned int j = 1;
    for (unsigned int i = 1; i < nTimes2; i += 2) {
        if (j > i) {
            swap(data[j-1], data[i-1]);
            swap(data[j], data[i]);
        }
        m = n;
        while (m >= 2 && j > m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    // Danielson-Lanczos section
    double wTemp, wReal, wpReal, wImag, wpImag, theta;
    float tempReal, tempImag;
    unsigned int iStep;
    unsigned int mMax = 2;
    while (mMax < nTimes2) {
        iStep = mMax << 1;
        theta = -TwoPi / (double)mMax;     // we flip sign here to match MATLAB fft()
        wTemp = sin(0.5 * theta);
        wpReal = -2.0 * wTemp * wTemp;
        wpImag = sin(theta);
        wReal = 1.0;
        wImag = 0.0;
        for (m = 1; m < mMax; m += 2) {
            for (unsigned int i = m; i <= nTimes2; i += iStep) {
                j = i + mMax;
                tempReal = (float)(wReal * (double)data[j-1] - wImag * (double)data[j]);
                tempImag = (float)(wReal * (double)data[j] + wImag * (double)data[j-1]);
                data[j-1] = data[i-1] - tempReal;
                data[j] = data[i] - tempImag;
                data[i-1] += tempReal;
                data[i] += tempImag;
            }
            wTemp = wReal;
            wReal += wReal * wpReal - wImag * wpImag;
            wImag += wImag * wpReal + wTemp * wpImag;
        }
        mMax = iStep;
    }
}

// Perform an FFT of an array of n real numbers, where n must be a power of two.
// The real numbers are stored in data, an array of length n, and the complex FFT
// (for positive frequencies only) is overwritten on the same array in the following
// format:
// data[0] = real component of frequency 0
// data[1] = real component of frequency n/2
// data[2] = real component of frequency 1
// data[3] = imaginary component of frequency 1
// data[4] = real component of frequency 2
// data[5] = imaginary component of frequency 2
// etc...
// Note the special use of data[1] to return the real component of the maximum
// frequency n/2.  The imaginary component of frequency 0 and n/2 are always zero
// for real-valued inputs.
void FastFourierTransform::realInputFft(float *data, unsigned int n)
{
    complexInputFft(data, n >> 1);

    double theta = -Pi / (double)(n >> 1);      // we flip sign here to match MATLAB fft()
    double wTemp = sin(0.5 * theta);
    double wpReal = -2.0 * wTemp * wTemp;
    double wpImag = sin(theta);
    double wReal = 1.0 + wpReal;
    double wImag = wpImag;
    unsigned int nPlus1 = n + 1;
    unsigned int i1, i2, i3, i4;
    float h1Real, h1Imag, h2Real, h2Imag;
    for (unsigned int i = 2; i <= (n >> 2); ++i) {
        i1 = (i << 1) - 2;
        i2 = i1 + 1;
        i3 = nPlus1 - i2;
        i4 = i3 + 1;
        h1Real = 0.5F * (data[i1] + data[i3]);
        h1Imag = 0.5F * (data[i2] - data[i4]);
        h2Real = 0.5F * (data[i2] + data[i4]);
        h2Imag = 0.5F * (data[i3] - data[i1]);
        data[i1] = (float)(h1Real + wReal * h2Real - wImag * h2Imag);
        data[i2] = (float)(h1Imag + wReal * h2Imag + wImag * h2Real);
        data[i3] = (float)(h1Real - wReal * h2Real + wImag * h2Imag);
        data[i4] = (float)(-h1Imag + wReal * h2Imag + wImag * h2Real);
        wTemp = wReal;
        wReal += wReal * wpReal - wImag * wpImag;
        wImag += wImag * wpReal + wTemp * wpImag;
    }
    data[(n >> 1) + 1] *= -1.0F;    // we flip this imaginary value sign to match MATLAB fft()

    // Convey first real frequency value in data[0] and last real frequency value in data[1].
    // These have no imaginary components.
    h1Real = data[0];
    data[0] += data[1];
    data[1] = h1Real - data[1];
}

// Calculate the logarithm of the square root of the PSD of data and normalizes values to facilitate calculation
// of signal amplitude from PSD.  The values in data are overwritten with intermediate results.
// Returns a pointer to the results, an array (length/2 + 1) long.
float* FastFourierTransform::logSqrtPowerSpectralDensity(float *data)
{
    // Apply window.
    for (unsigned int i = 0; i < length; ++i) {
        data[i] *= window[i];
    }

    // Calculate FFT.
    realInputFft(data, length);

    float normalizationFactor = log10f(2.0F / (float) length); // add this to facilitate estimate of narrowband signal amplitude
                                                               // from PSD.
    const float windowCorrectionFactor = 0.267789F; // empirical correction factor; only valid for Hamming window!
    normalizationFactor += windowCorrectionFactor;

    float epsilon = numeric_limits<float>::min();   // add tiny number to PSD results before
                                                    // calculating log to avoid log(0) = -inf.
    logPsd[0] = 0.5F * log10f(0.25F * data[0] * data[0] + epsilon) + normalizationFactor;    // no imaginary component here
    unsigned int i = 1;
    unsigned int j = 2;
    for ( ; i < (length >> 1); ++i) {
        // PSD = |FFT|^2 = real^2 + imaginary^2
        // Then take the square root (moved outside the logarithm as a factor of 1/2) to go from uV^2/Hz to uV/sqrt(Hz).
        // Then take logarithm to compress wide dynamic range for viewing.  And add normalization factor to normalize to
        // the number of samples in the FFT and to compensate for weighting of FFT window function.
        logPsd[i] = 0.5F * log10f(data[j] * data[j] + data[j+1] * data[j+1] + epsilon) + normalizationFactor;
        j += 2;
    }
    logPsd[i] = 0.5F * log10f(0.25F * data[1] * data[1] + epsilon) + normalizationFactor;    // no imaginary component here
    return logPsd;
}

// Return frequency for an index ranging from zero to (length/2).
float FastFourierTransform::getFrequency(int index) const
{
    if (index > (int)(length >> 1)) {
        return -1.0F;
    } else {
        return frequency[index];
    }
}
