/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define QT_NO_KEYWORDS
#pragma once

#include <memory>
#include <pybind11/pybind11.h>

#include <datactl/syclock.h>

namespace py = pybind11;
using namespace Syntalos;

#pragma GCC visibility push(default)

struct SyntalosPyError : std::runtime_error {
    explicit SyntalosPyError(const char *what_arg);
    explicit SyntalosPyError(const std::string &what_arg);
};

void pythonRegisterSyioModule();

#pragma GCC visibility pop
