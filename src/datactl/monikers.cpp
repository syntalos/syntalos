/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "monikers.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <xxhash.h>

namespace Syntalos
{

namespace
{

// Word lists are embedded verbatim from data/words/*.txt as raw byte blobs.
// We avoid building a compile-time table of string_views (which would bake
// ~16 bytes per entry into the binary) and instead build a compact uint32_t
// offset table lazily on first use.
#ifdef __has_embed
constexpr unsigned char adjectives_data[] = {
#embed "words/adjectives.txt"
};
constexpr unsigned char animals_data[] = {
#embed "words/animals.txt"
};
constexpr unsigned char intermediate_data[] = {
#embed "words/intermediate.txt"
};
constexpr unsigned char nouns_data[] = {
#embed "words/nouns.txt"
};
#else
#include "words_embed.h"
#endif

class WordList
{
public:
    WordList(const unsigned char *data, std::size_t size) noexcept
        : m_blob(reinterpret_cast<const char *>(data), size)
    {
    }

    std::size_t size() const
    {
        ensureBuilt();
        return m_starts.size();
    }

    std::string_view at(std::size_t idx) const
    {
        ensureBuilt();
        const uint32_t start = m_starts[idx];
        // For the last entry, use size() as sentinel when the file ends with '\n'
        // so that end = size()-1 (the '\n' position) and substr correctly excludes it.
        const bool lastEntry = (idx + 1 >= m_starts.size());
        const uint32_t next = lastEntry ? static_cast<uint32_t>(
                                              m_blob.size() + (!m_blob.empty() && m_blob.back() == '\n' ? 0 : 1))
                                        : m_starts[idx + 1];
        // next points one past the line separator; strip it (and a CR if present).
        std::size_t end = next - 1;
        if (end > start && m_blob[end - 1] == '\r')
            --end;
        return m_blob.substr(start, end - start);
    }

private:
    void ensureBuilt() const
    {
        std::call_once(m_built, [this] {
            std::size_t lines = 0;
            for (char c : m_blob)
                if (c == '\n')
                    ++lines;
            const bool trailing = !m_blob.empty() && m_blob.back() != '\n';
            if (trailing)
                ++lines;

            m_starts.reserve(lines);
            if (!m_blob.empty())
                m_starts.push_back(0);
            for (std::size_t i = 0; i + 1 < m_blob.size(); ++i) {
                if (m_blob[i] == '\n')
                    m_starts.push_back(static_cast<uint32_t>(i + 1));
            }
        });
    }

    std::string_view m_blob;
    mutable std::vector<uint32_t> m_starts;
    mutable std::once_flag m_built;
};

WordList &adjectives()
{
    static WordList list(adjectives_data, sizeof(adjectives_data));
    return list;
}

WordList &animals()
{
    static WordList list(animals_data, sizeof(animals_data));
    return list;
}

WordList &intermediates()
{
    static WordList list(intermediate_data, sizeof(intermediate_data));
    return list;
}

WordList &nouns()
{
    static WordList list(nouns_data, sizeof(nouns_data));
    return list;
}

std::mt19937_64 &threadRng()
{
    static thread_local std::mt19937_64 rng{
        std::random_device{}() ^ static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
    return rng;
}

} // namespace

static std::string makeAnimalMonikerFromRng(std::mt19937_64 &rng)
{
    auto &adj = adjectives();
    auto &ani = animals();

    std::uniform_int_distribution<std::size_t> aDist(0, adj.size() - 1);
    std::uniform_int_distribution<std::size_t> nDist(0, ani.size() - 1);

    const auto a = adj.at(aDist(rng));
    const auto n = ani.at(nDist(rng));

    std::string out;
    out.reserve(a.size() + 1 + n.size());
    out.append(a).append("-").append(n);

    // Word entries may contain spaces (e.g. "guinea pig"); flatten them.
    std::replace(out.begin(), out.end(), ' ', '-');
    return out;
}

static std::string makeMonikerFromSeed(uint64_t seed)
{
    std::mt19937_64 rng(seed);

    auto &adj = adjectives();
    auto &mid = intermediates();
    auto &nn = nouns();

    std::uniform_int_distribution<std::size_t> aDist(0, adj.size() - 1);
    std::uniform_int_distribution<std::size_t> mDist(0, mid.size() - 1);
    std::uniform_int_distribution<std::size_t> nDist(0, nn.size() - 1);
    // ~25% of monikers get an intermediate word inserted
    std::uniform_int_distribution<int> useMid(0, 3);

    const auto a = adj.at(aDist(rng));
    const bool withMid = useMid(rng) == 0;
    const auto m = withMid ? mid.at(mDist(rng)) : std::string_view{};
    const auto n = nn.at(nDist(rng));

    std::string out;
    out.reserve(a.size() + n.size() + (withMid ? m.size() + 2 : 1));
    out.append(a).append("-");
    if (withMid)
        out.append(m).append("-");
    out.append(n);

    std::replace(out.begin(), out.end(), ' ', '-');
    return out;
}

std::string makeAnimalMoniker()
{
    return makeAnimalMonikerFromRng(threadRng());
}

std::string makeAnimalMonikerForString(const std::string &source)
{
    std::mt19937_64 rng(XXH3_64bits(source.data(), source.size()));
    return makeAnimalMonikerFromRng(rng);
}

std::string makeMonikerForUuid(const Uuid &uuid)
{
    return makeMonikerFromSeed(XXH3_64bits(uuid.bytes.data(), uuid.bytes.size()));
}

std::string makeMonikerForString(const std::string &source)
{
    return makeMonikerFromSeed(XXH3_64bits(source.data(), source.size()));
}

} // namespace Syntalos
