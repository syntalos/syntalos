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

#include <QTime>
#include <QRandomGenerator>
#include <cmath>
#include "randomnumber.h"

// Generate random numbers from both uniform and Gaussian distrubtions.
RandomNumber::RandomNumber()
{
    // Seed random number generator.
    // unsigned int seed = QTime::currentTime().msec();
    unsigned int seed = 4;

    generator = new QRandomGenerator(seed);

    // Initialize parameters for Gaussian distribution approximator.
    setGaussianAccuracy(6);
}

RandomNumber::~RandomNumber()
{
    delete generator;
}

// Return a random number from a uniform distribution between 0.0 and 1.0.
double RandomNumber::randomUniform()
{
    return generator->generateDouble();
}

// Return a random number from a uniform distribution between min and max.
double RandomNumber::randomUniform(double min, double max)
{
    return(generator->generateDouble() * (max - min) + min);
}

// Return a random number from a uniform logarithmic-scale distribution between min and max.
// (Both min and max should be positive non-zero numbers.)
double RandomNumber::randomLogUniform(double min, double max)
{
    double logMin = log(min);
    double logMax = log(max);
    double logResult = generator->generateDouble() * (logMax - logMin) + logMin;
    return exp(logResult);
}

// Return a random number from a Gaussian distribution with variance = 1.0.
// This function relies on the central limit theorem to approximate a normal
// distribution.  Increasing gaussianN will improve accuracy at the expense of
// speed.  A value of 6 is adequate for most applications.
double RandomNumber::randomGaussian()
{
    double r = 0.0;
    for (int i = 0; i < gaussianN; ++i) {
        r += randomUniform(-1.0, 1.0);
    }
    r *= gaussianScaleFactor;
    return r;
}

// Making n larger increases accuracy of Gaussian approximation at the expense of speed.
void RandomNumber::setGaussianAccuracy(int n)
{
    gaussianN = n;
    gaussianScaleFactor = sqrt(3.0) / sqrt(gaussianN);  // precalculate this for speed
}

