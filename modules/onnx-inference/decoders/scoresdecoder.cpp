/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "scoresdecoder.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace OnnxInf
{

const std::vector<ParamSpec> &ScoresDecoder::paramSpecs()
{
    static const std::vector<ParamSpec> specs = {
        {
         .key = "tensor",
         .type = ParamType::String,
         .title = "Output tensor",
         .help = "Name of the model output holding the scores. Empty selects the first output.",
         .defaultValue = std::string(),
         },
        {
         .key = "activation",
         .type = ParamType::Choice,
         .title = "Activation",
         .help = "Function applied to the raw scores, for models that output logits.",
         .defaultValue = std::string("none"),
         .choices = {{"none", "None"}, {"sigmoid", "Sigmoid"}, {"softmax", "Softmax"}},
         },
        {
         .key = "labels",
         .type = ParamType::StringList,
         .title = "Labels",
         .help = "One name per class. Without labels, classes are numbered.",
         .defaultValue = std::vector<std::string>{},
         },
    };
    return specs;
}

ScoresDecoder::ScoresDecoder(const ParamSet &params)
    : m_tensorParam(params.getString("tensor")),
      m_labels(params.getStringList("labels"))
{
    const auto &activation = params.getString("activation");
    if (activation == "sigmoid")
        m_activation = Activation::Sigmoid;
    else if (activation == "softmax")
        m_activation = Activation::Softmax;
    else
        m_activation = Activation::None;
}

auto ScoresDecoder::configure(const std::vector<TensorView> &outputs, const std::string &indexColumn)
    -> std::expected<void, std::string>
{
    const auto tensor = selectTensor(outputs, m_tensorParam);
    if (!tensor)
        return std::unexpected(tensor.error());
    const auto t = *tensor;

    if (t->dtype == DType::Unknown)
        return std::unexpected(std::format("Model output '{}' has a type the scores decoder can not read.", t->name));
    if (t->shape.empty() || t->shape.back() <= 0)
        return std::unexpected(
            std::format(
                "Model output '{}' has the shape {}, the scores decoder needs classes in the last dimension.",
                t->name,
                shapeToString(t->shape)));

    m_classCount = static_cast<size_t>(t->shape.back());
    if (!m_labels.empty() && m_labels.size() != m_classCount)
        return std::unexpected(
            std::format(
                "The recipe has {} labels, but model output '{}' has {} classes.",
                m_labels.size(),
                t->name,
                m_classCount));

    m_tensorName = t->name;
    m_header = {"time_usec", indexColumn, "step", "argmax", "label"};
    for (size_t c = 0; c < m_classCount; ++c)
        m_header.push_back("score_" + (m_labels.empty() ? std::to_string(c) : m_labels[c]));
    m_scores.resize(m_classCount);

    return {};
}

std::vector<std::string> ScoresDecoder::header() const
{
    return m_header;
}

auto ScoresDecoder::decode(const TensorSet &outputs, const DecodeInput &input, std::vector<Row> &rows)
    -> std::expected<void, std::string>
{
    const auto t = outputs.find(m_tensorName);
    if (t == nullptr || t->data == nullptr)
        return std::unexpected(std::format("Model output '{}' is missing.", m_tensorName));
    if (t->shape.empty() || t->shape.back() != static_cast<int64_t>(m_classCount))
        return std::unexpected(
            std::format("Model output '{}' changed its shape to {}.", m_tensorName, shapeToString(t->shape)));

    const size_t steps = t->elementCount() / m_classCount;
    for (size_t step = 0; step < steps; ++step) {
        for (size_t c = 0; c < m_classCount; ++c)
            m_scores[c] = t->valueAt(step * m_classCount + c);

        if (m_activation == Activation::Sigmoid) {
            for (auto &s : m_scores)
                s = 1.0 / (1.0 + std::exp(-s));
        } else if (m_activation == Activation::Softmax) {
            const double maxScore = *std::ranges::max_element(m_scores);
            double sum = 0.0;
            for (auto &s : m_scores) {
                s = std::exp(s - maxScore);
                sum += s;
            }
            for (auto &s : m_scores)
                s /= sum;
        }

        const auto best = static_cast<size_t>(std::ranges::max_element(m_scores) - m_scores.begin());

        Row row;
        row.reserve(5 + m_classCount);
        row.push_back(std::to_string(input.timeUsec));
        row.push_back(std::to_string(input.index));
        row.push_back(std::to_string(step));
        row.push_back(std::to_string(best));
        row.push_back(m_labels.empty() ? std::to_string(best) : m_labels[best]);
        for (const auto s : m_scores)
            row.push_back(formatValue(s));
        rows.push_back(std::move(row));
    }

    return {};
}

} // namespace OnnxInf
