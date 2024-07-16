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
#include <QDataStream>

namespace Syntalos
{

typedef Eigen::Matrix<qint32, Eigen::Dynamic, 1> VectorXsi;
typedef Eigen::Matrix<quint64, Eigen::Dynamic, 1> VectorXul;
typedef Eigen::Matrix<qint64, Eigen::Dynamic, 1> VectorXsl;
typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorXd;

typedef Eigen::Matrix<qint32, Eigen::Dynamic, Eigen::Dynamic> MatrixXsi;
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
double vectorVariance(const Eigen::Matrix<T, Eigen::Dynamic, 1> &vec, const double &mean)
{
    return (vec.template cast<double>().array() - mean).pow(2).sum() / vec.rows();
}

template<typename T>
double vectorVariance(const Eigen::Matrix<T, Eigen::Dynamic, 1> &vec)
{
    return vectorVariance(vec, vec.mean());
}

template<typename EigenType>
void serializeEigen(QDataStream &stream, const EigenType &matrix)
{
    const auto rows = static_cast<quint64>(matrix.rows());
    const auto cols = static_cast<quint64>(matrix.cols());
    stream << rows << cols;

    for (quint64 i = 0; i < rows; ++i) {
        for (quint64 j = 0; j < cols; ++j) {
            typename EigenType::Scalar value = matrix(i, j);
            stream << value;
        }
    }
}

template<typename EigenType>
EigenType deserializeEigen(QDataStream &stream)
{
    EigenType matrix;

    quint64 rows, cols;
    stream >> rows >> cols;

    matrix.resize(rows, cols);
    for (quint64 i = 0; i < rows; ++i) {
        for (quint64 j = 0; j < cols; ++j) {
            typename EigenType::Scalar value;
            stream >> value;
            matrix(i, j) = value;
        }
    }

    return matrix;
}

} // namespace Syntalos
