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

#include "gtest/gtest.h"
#include "test_utils.h"

// Optional setup/teardown for test suite
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Code to run before all tests
    }

    void TearDown() override {
        // Code to run after all tests
    }
};

// checks for a specific error string in the output
void checkError(const std::string& expectedMessage, std::function<void()> callback)
{
    // Redirect stderr to capture messages
    std::ostringstream nullStream; // used to suppress std::cout
    std::streambuf* originalCout = std::cout.rdbuf(nullStream.rdbuf()); // Redirect std::cout to null
    std::ostringstream errStream;
    std::streambuf* originalCerr = std::cerr.rdbuf(errStream.rdbuf());

    // do the callback
    callback();

    // Restore stderr and stdout
    std::cout.rdbuf(originalCout);
    std::cerr.rdbuf(originalCerr);

    // check for errStream for error
    std::string capturedErrors = errStream.str();
    if (expectedMessage.empty()) {
        EXPECT_TRUE(capturedErrors.empty()) << "No error message expected but got " << capturedErrors;
        return;
    }
    EXPECT_NE(capturedErrors.find(expectedMessage), std::string::npos)
        << "Expected error message not found. Actual: " << capturedErrors;
};

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}
