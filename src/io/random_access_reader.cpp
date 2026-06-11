/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "denigma/io/random_access_reader.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace denigma {

FileRandomAccessReader::FileRandomAccessReader(const std::filesystem::path& path)
    : m_file(path, std::ios::binary)
{
    if (!m_file) {
        throw std::runtime_error("unable to open random-access input file");
    }

    m_file.seekg(0, std::ios::end);
    const auto end = m_file.tellg();
    if (end < 0) {
        throw std::runtime_error("unable to determine random-access input file size");
    }
    m_size = static_cast<std::uint64_t>(end);
    m_file.seekg(0, std::ios::beg);
}

std::uint64_t FileRandomAccessReader::size() const
{
    return m_size;
}

std::size_t FileRandomAccessReader::readAt(std::uint64_t offset, std::span<std::byte> output) const
{
    if (offset >= m_size || output.empty()) {
        return 0;
    }

    const auto available = static_cast<std::size_t>(std::min<std::uint64_t>(m_size - offset, output.size()));
    m_file.clear();
    m_file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!m_file) {
        throw std::runtime_error("unable to seek random-access input file");
    }
    m_file.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(available));
    if (m_file.bad()) {
        throw std::runtime_error("unable to read random-access input file");
    }
    return static_cast<std::size_t>(m_file.gcount());
}

BufferRandomAccessReader::BufferRandomAccessReader(std::span<const std::byte> data)
    : m_data(data)
{
}

std::uint64_t BufferRandomAccessReader::size() const
{
    return static_cast<std::uint64_t>(m_data.size());
}

std::size_t BufferRandomAccessReader::readAt(std::uint64_t offset, std::span<std::byte> output) const
{
    if (offset >= m_data.size() || output.empty()) {
        return 0;
    }

    const auto available = static_cast<std::size_t>(std::min<std::uint64_t>(m_data.size() - offset, output.size()));
    std::memcpy(output.data(), m_data.data() + offset, available);
    return available;
}

} // namespace denigma
