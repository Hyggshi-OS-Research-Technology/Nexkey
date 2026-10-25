// SPDX-License-Identifier: MIT
// NexKey command line entry point.

#include "Cli.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    using namespace nexkey::cli;

    Args args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    if (args.empty()) {
        print_help(std::cout);
        return 1;
    }

    const std::string command = args.front();
    const Args rest(args.begin() + 1, args.end());

    if (command == "help" || command == "--help" || command == "-h") {
        print_help(std::cout);
        return 0;
    }
    if (command == "version" || command == "--version" || command == "-V") {
        print_version(std::cout);
        return 0;
    }
    if (command == "methods") {
        print_methods(std::cout);
        return 0;
    }
    if (command == "encodings") {
        print_encodings(std::cout);
        return 0;
    }
    if (command == "test") {
        return run_test(rest);
    }
    if (command == "convert") {
        return run_convert(rest);
    }
    if (command == "config") {
        return run_config(rest);
    }
    if (command == "status") {
        return run_status(rest);
    }
    if (command == "toggle") {
        return run_toggle(rest);
    }
    if (command == "macro") {
        return run_macro(rest);
    }
    if (command == "emoji") {
        return run_emoji(rest);
    }

    std::cerr << "nexkey: unknown command " << command << "\n\n";
    print_help(std::cerr);
    return 2;
}
