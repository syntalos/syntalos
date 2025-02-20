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
#ifndef FASTFOURIERTRANSFORM_H
#define FASTFOURIERTRANSFORM_H

class FastFourierTransform
{
public:
    enum WindowFunction {
        WindowRectangular,
        WindowTriangular,
        WindowHann,
        WindowHamming
    };

    FastFourierTransform(float sampleRate_, unsigned int length_ = 1024, WindowFunction function_ = WindowHamming);
    ~FastFourierTransform();

    void setLength(int length_);
    static void complexInputFft(float *data, unsigned int n);
    static void realInputFft(float *data, unsigned int n);
    float* logSqrtPowerSpectralDensity(float *data);
    float getFrequency(int index) const;

private:
    float sampleRate;
    unsigned int length;
    WindowFunction function;

    float *window;
    float *logPsd;
    float *frequency;

    void createWindow();
    void createPsdVector();
    void createFrequencyVector();
};

#endif // FASTFOURIERTRANSFORM_H
