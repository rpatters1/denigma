/*merged/
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
#pragma once

#include <vector>
#include <string>
#include <functional>

#include "denigma.h"

class ArgList {
public:
    // Constructor to allow initialization with { "arg1", "arg2", ... }
    ArgList(std::initializer_list<denigma::arg_string> init)
        : args_(init) {}

    // Default constructor for manual addition
    ArgList() = default;

    // Add a single argument
    void add(const denigma::arg_string& arg) {
        args_.emplace_back(arg);
    }

    // Add multiple arguments
    void add(const std::vector<denigma::arg_string>& args) {
        args_.insert(args_.end(), args.begin(), args.end());
    }

    // Get argc (number of arguments)
    int argc() const {
        return static_cast<int>(args_.size());
    }

    // Get argv (C-style denigma::arg_char** array)
    denigma::arg_char** argv() {
        argv_.clear();
        for (const auto& arg : args_) {
            argv_.push_back(const_cast<denigma::arg_char*>(arg.c_str()));
        }
        argv_.push_back(nullptr); // Null-terminate
        return argv_.data();
    }

private:
    std::vector<denigma::arg_string> args_; // Store arguments as strings
    std::vector<denigma::arg_char*> argv_;       // Cache for denigma::arg_char** representation
};

void checkStderr(const std::string& expectedMessage, std::function<void()> callback);
void checkStdout(const std::string& expectedMessage, std::function<void()> callback);

constexpr const char DENIGMA_NAME[] = "denigma";
