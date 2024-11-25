/*
 * Copyright (C) 2024, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef SCOREFILECRYPTER_H
#define SCOREFILECRYPTER_H

#include <string>
#include <cstdint>

// Shout out to Deguerre https://github.com/Deguerre

/** @brief Static class that encapsulates the crypter for `score.dat`.  */
class ScoreFileCrypter
{
    // These two values were determined empirically and must not be changed.
    constexpr static uint32_t INITIAL_STATE = 0x28006D45; // arbitrary initial value for algorithm
    constexpr static uint32_t RESET_LIMIT = 0x20000; // reset value corresponding (probably) to an internal Finale buffer size

public:
    /** @brief Encodes or decodes a `score.dat` file extracted
     * from a `.musx` archive. This is a symmetric algorithm that
     * encodes a decoded buffer or decodes an encoded buffer.
     * 
     * @param [in,out] buffer a buffer that is re-coded in place,
     * @param [in] buffSize the number of characters in the buffer.
     */
    template<typename CT>
    static void cryptBuffer(CT* buffer, size_t buffSize)
    {
        static_assert(std::is_same<CT, uint8_t>::value ||
                      std::is_same<CT, char>::value,
                      "cryptBuffer can only be called with buffers of uint8_t or char.");
        uint32_t state = INITIAL_STATE;
        int i = 0;
        for (size_t i = 0; i < buffSize; i++) {
            if (i % RESET_LIMIT == 0) {
                state = INITIAL_STATE;
            }
            state = state * 0x41c64e6d + 0x3039; // BSD rand()!
            uint16_t upper = state >> 16;
            uint8_t c = upper + upper / 255;
            buffer[i] ^= c;
        }
    }

    /** @brief version of cryptBuffer for containers */
    template <typename T>
    static void cryptBuffer(T& buffer)
    {
        // Ensure that the value type is either uint8_t or char
        static_assert(std::is_same<typename T::value_type, uint8_t>::value ||
                      std::is_same<typename T::value_type, char>::value,
                      "cryptBuffer can only be called with containers of uint8_t or char.");
        return cryptBuffer(buffer.data(), buffer.size());
    }

    /** @brief version of cryptBuffer for C-style arrays */
    template <typename CT, size_t N>
    static void cryptBuffer(CT (&buffer)[N])
    {
        return cryptBuffer(buffer, N);
    }
};

#endif //SCOREFILECRYPTER_H