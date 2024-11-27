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
#ifndef SCORE_FILE_ENCODER_H
#define SCORE_FILE_ENCODER_H

#include <string>
#include <cstdint>

// Shout out to Deguerre https://github.com/Deguerre

namespace musx
{

/** @brief Static class that encapsulates the crypter for a `score.dat` file taken
 * from a `.musx` file. A `.musx` file is a standard zip archive that contains
 * a directory structure containing all the data Finale uses to render a document.
 * The primary EnigmaXml document is a file called `score.dat`. This is a Gzip archive that
 * has been encoded using the algorithm provided in this class.
 * 
 * The steps to extract EnigmaXml from a `.musx` document are:
 * - Unzip the `.musx` file.
 * - Read the `score.dat` file into a buffer.
 * - Decode the the buffer using `ScoreFileEncoder::cryptBuffer`.
 * - Gunzip the decoded buffer into the EnigmaXml.
 */
class ScoreFileEncoder
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

    /** @brief version of cryptBuffer for containers.
     * 
     * @param [in,out] buffer a container that is re-coded in place,
     */
    template <typename T>
    static void cryptBuffer(T& buffer)
    {
        // Ensure that the value type is either uint8_t or char
        static_assert(std::is_same<typename T::value_type, uint8_t>::value ||
                      std::is_same<typename T::value_type, char>::value,
                      "cryptBuffer can only be called with containers of uint8_t or char.");
        return cryptBuffer(buffer.data(), buffer.size());
    }
};

} // end namespace

#endif //SCORE_FILE_ENCODER_H