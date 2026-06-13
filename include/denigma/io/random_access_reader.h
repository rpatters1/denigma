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
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace denigma {

/// Random-access byte reader used for container formats such as MUSX.
class IRandomAccessReader
{
public:
    virtual ~IRandomAccessReader() = default;

    /// Returns the total readable byte count.
    [[nodiscard]] virtual std::uint64_t size() const = 0;

    /// Reads up to output.size() bytes from offset and returns the number of bytes read.
    virtual std::size_t readAt(std::uint64_t offset, std::span<std::byte> output) const = 0;
};

/// Random-access reader backed by a filesystem file.
class FileRandomAccessReader final : public IRandomAccessReader
{
public:
    /// Opens path for random-access reads.
    explicit FileRandomAccessReader(const std::filesystem::path& path);

    [[nodiscard]] std::uint64_t size() const override;
    std::size_t readAt(std::uint64_t offset, std::span<std::byte> output) const override;

private:
    mutable std::ifstream m_file;
    std::uint64_t m_size{};
};

/// Random-access reader backed by caller-owned memory.
class BufferRandomAccessReader final : public IRandomAccessReader
{
public:
    /// Uses caller-owned memory as a random-access byte source.
    explicit BufferRandomAccessReader(std::span<const std::byte> data);

    [[nodiscard]] std::uint64_t size() const override;
    std::size_t readAt(std::uint64_t offset, std::span<std::byte> output) const override;

private:
    std::span<const std::byte> m_data;
};

} // namespace denigma
