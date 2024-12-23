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
#include <iostream>
#include <map>
#include <optional>
#include <memory>

#include "denigma.h"
#include "export.h"

static const auto registeredCommands = []()
    {
        std::map <std::string_view, std::shared_ptr <denigma::ICommand>> retval;
        auto exportCmd = std::make_shared<denigma::ExportCommand>();
        retval.emplace(exportCmd->commandName(), exportCmd);
        return retval;
    }();

static int showHelpPage(const std::string_view& programName)
{
    std::cerr << "Usage: " << programName << " <command> <input-pattern> [--options]" << std::endl;
    std::cerr << std::endl;

    // General options
    std::cerr << "General options:" << std::endl;
    std::cerr << "  --help                      Show this help message and exit" << std::endl;
    std::cerr << "  --force                     Overwrite existing file(s)" << std::endl;
    std::cerr << "  --part [optional-part-name] Process for named part or first part if name is omitted" << std::endl;
    std::cerr << "  --all-parts                 Process for all parts and score" << std::endl;
    std::cerr << "  --version                   Show program version and exit" << std::endl;

    for (const auto& command : registeredCommands) {
        std::string commandStr = "Command " + std::string(command.first);
        std::string sepStr(commandStr.size(), '=');
        std::cerr << std::endl;
        std::cerr << sepStr << std::endl;
        std::cerr << commandStr << std::endl;
        std::cerr << sepStr << std::endl;
        std::cerr << std::endl;
        command.second->showHelpPage(programName, "    ");
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    std::string programName = std::filesystem::path(argv[0]).stem().string();
    
    if (argc < 2) {
        return showHelpPage(programName);
    }

    try {
        denigma::DenigmaOptions options;
        bool showVersion = false;
        bool showHelp = false;
        std::vector<const char*> args;
        for (int x = 1; x < argc; x++) {
            const std::string_view next(argv[x]);
            if (next == "--version") {
                showVersion = true;
            } else if (next == "--help") {
                showHelp = true;
            } else if (next == "--force") {
                options.overwriteExisting = true;
            } else if (next == "--all-parts") {
                options.allPartsAndScore = true;
            } else if (next == "--part") {
                options.partName = "";
                std::string_view arg(argv[x + 1]);
                if (x < (argc - 1) && arg.rfind("--", 0) != 0) {
                    options.partName = arg;
                    x++;
                }
            } else {
                args.push_back(argv[x]);
            }
        }

        if (showVersion) {
            std::cout << programName << " " << DENIGMA_VERSION << std::endl;
            return 0;
        }
        if (showHelp) {
            showHelpPage(programName);
            return 0;
        }
        if (args.empty()) {
            return showHelpPage(programName);
        }

        auto it = registeredCommands.find(args[0]);
        if (it != registeredCommands.end()) {
            args.erase(args.begin());
            it->second->doCommand(args, options);
        } else {
            std::cerr << "Unknown command: " << args[0] << std::endl;
            return showHelpPage(programName);
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return showHelpPage(programName);
    }

    return 0;
}
