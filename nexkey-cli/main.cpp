#include "vkcore/VKBuffer.hpp"
#include "vkcore/VKEncoding.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {

vkcore::InputMethod parse_method(const std::string& value)
{
    if (value == "vni") {
        return vkcore::InputMethod::VNI;
    }
    if (value == "viqr") {
        return vkcore::InputMethod::VIQR;
    }
    return vkcore::InputMethod::Telex;
}

vkcore::Encoding parse_encoding(const std::string& value)
{
    if (value == "tcvn3") {
        return vkcore::Encoding::TCVN3;
    }
    if (value == "vni" || value == "vni-encoding") {
        return vkcore::Encoding::VNIEncoding;
    }
    return vkcore::Encoding::Unicode;
}

void usage()
{
    std::cout
        << "Usage:\n"
        << "  nexkey status\n"
        << "  nexkey toggle\n"
        << "  nexkey set --method telex|vni|viqr\n"
        << "  nexkey set --encoding unicode|tcvn3|vni\n"
        << "  nexkey convert --from vni-encoding --to utf8 < input.txt\n"
        << "  nexkey test [--method telex|vni|viqr] \"text\"\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "status") {
        std::cout << "mode=VI method=telex encoding=unicode\n";
        return 0;
    }

    if (command == "toggle") {
        std::cout << "mode toggled\n";
        return 0;
    }

    if (command == "set") {
        if (argc != 4) {
            usage();
            return 1;
        }
        std::cout << argv[2] << "=" << argv[3] << "\n";
        return 0;
    }

    if (command == "convert") {
        std::string from;
        std::string to;
        for (int i = 2; i + 1 < argc; i += 2) {
            if (std::string(argv[i]) == "--from") {
                from = argv[i + 1];
            } else if (std::string(argv[i]) == "--to") {
                to = argv[i + 1];
            }
        }
        std::ostringstream input;
        input << std::cin.rdbuf();
        if ((from == "vni" || from == "vni-encoding") && (to == "utf8" || to == "unicode")) {
            std::cout << vkcore::VKEncoding::to_utf8_from_vni(input.str());
            return 0;
        }
        std::cerr << "unsupported conversion\n";
        return 2;
    }

    if (command == "test") {
        vkcore::InputMethod method = vkcore::InputMethod::Telex;
        std::string text;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--method" && i + 1 < argc) {
                method = parse_method(argv[++i]);
            } else if (arg == "--encoding" && i + 1 < argc) {
                ++i;
            } else {
                if (!text.empty()) {
                    text.push_back(' ');
                }
                text += arg;
            }
        }

        vkcore::VKBuffer buffer(method, parse_encoding("unicode"));
        for (char c : text) {
            if (const auto r = buffer.process_key(static_cast<unsigned char>(c)); !r.committed.empty()) {
                std::cout << r.committed;
            }
        }
        std::cout << buffer.flush() << "\n";
        return 0;
    }

    usage();
    return 1;
}
