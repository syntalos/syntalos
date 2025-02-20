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

#ifndef MINMAX_H
#define MINMAX_H

#include <limits>

template <class Type> class MinMax
{
public:
    MinMax() : minVal((std::numeric_limits<Type>::max)()), maxVal((std::numeric_limits<Type>::lowest)()) {}
    MinMax(const MinMax<Type> &init) : minVal(init.minVal), maxVal(init.maxVal) {}
    MinMax& operator=(const MinMax &init) { minVal = init.minVal; maxVal = init.maxVal; return *this; }
    MinMax(Type init) : minVal(init), maxVal(init) {}

    Type minVal;
    Type maxVal;

    inline void update(Type value)
    {
        if (value < minVal) minVal = value;
        if (value > maxVal) maxVal = value;
    }
    inline void set(Type value)
    {
        minVal = value;
        maxVal = value;
    }
    inline void reset()
    {
        minVal = (std::numeric_limits<Type>::max)();
        maxVal = (std::numeric_limits<Type>::lowest)();
    }
    inline void swap()
    {
        Type temp = minVal;
        minVal = maxVal;
        maxVal = temp;
    }
};

#endif // MINMAX_H
