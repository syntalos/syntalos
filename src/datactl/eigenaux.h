/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <cstdint>
#include <vector>

#include "binarystream.h"

namespace Syntalos
{

typedef Eigen::Matrix<int32_t, Eigen::Dynamic, 1> VectorXsi;
typedef Eigen::Matrix<uint64_t, Eigen::Dynamic, 1> VectorXul;
typedef Eigen::Matrix<int64_t, Eigen::Dynamic, 1> VectorXsl;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorXd;

typedef Eigen::Matrix<int32_t, Eigen::Dynamic, Eigen::Dynamic> MatrixXsi;
typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MatrixXd;

template<typename T>
double vectorMedian(const Eigen::Matrix<T, Eigen::Dynamic, 1> &vec)
{
    const auto size = vec.size();
    if (size == 0)
        return nan(""); // what is the median of an empty vector?

    std::vector<T> vecSorted(vec.size());
    std::partial_sort_copy(vec.data(), vec.data() + vec.size(), std::begin(vecSorted), std::end(vecSorted));

    if (size % 2 == 0)
        return ((long long)vecSorted[size / 2 - 1] + (long long)vecSorted[size / 2]) / 2.0;
    else
        return vecSorted[size / 2];
}

template<typename T>
double vectorMedianInplace(Eigen::Matrix<T, Eigen::Dynamic, 1> &vec)
{
    const auto size = vec.size();
    if (size == 0)
        return nan(""); // what is the median of an empty vector?

    // TODO: This is dangerous code - Eigen 3.4.x brings support for STL iterators,
    // which will make this much nicer when we can use it. Currently, this code
    // would break for vectors with a stride != 1
    std::sort(vec.data(), vec.data() + vec.size());
    if (size % 2 == 0)
        return ((long long)vec[size / 2 - 1] + (long long)vec[size / 2]) / 2.0;
    else
        return vec[size / 2];
}

template<typename T>
double vectorVariance(const Eigen::Matrix<T, Eigen::Dynamic, 1> &vec, const double &mean, bool unbiased = true)
{
    if (unbiased)
        return (vec.template cast<double>().array() - mean).pow(2).sum() / (vec.rows() - 1);
    else
        return (vec.template cast<double>().array() - mean).pow(2).sum() / vec.rows();
}

template<typename T>
double vectorVariance(const Eigen::Matrix<T, Eigen::Dynamic, 1> &vec, bool unbiased = true)
{
    return vectorVariance(vec, vec.mean(), unbiased);
}

template<typename EigenType>
void serializeEigen(BinaryStreamWriter &stream, const EigenType &matrix)
{
    const auto rows = static_cast<uint64_t>(matrix.rows());
    const auto cols = static_cast<uint64_t>(matrix.cols());
    stream.write(rows);
    stream.write(cols);

    for (uint64_t i = 0; i < rows; ++i) {
        for (uint64_t j = 0; j < cols; ++j) {
            typename EigenType::Scalar value = matrix(i, j);
            stream.write(value);
        }
    }
}

template<typename EigenType>
EigenType deserializeEigen(BinaryStreamReader &stream)
{
    EigenType matrix;

    uint64_t rows, cols;
    stream.read(rows);
    stream.read(cols);

    matrix.resize(rows, cols);
    for (uint64_t i = 0; i < rows; ++i) {
        for (uint64_t j = 0; j < cols; ++j) {
            typename EigenType::Scalar value;
            stream.read(value);
            matrix(i, j) = value;
        }
    }

    return matrix;
}

} // namespace Syntalos
