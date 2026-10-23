#include "Script.h"
#include <unordered_map>

namespace gpr {
namespace {
[[maybe_unused]] std::string Upper(std::string s) {
    for (auto &c : s)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return s;
}
} // namespace
const std::array<std::string, 256> &Controls::Names() {
    static const auto names = [] {
        std::array<std::string, 256> n{};
        const char *canonical[]{"LX",   "LY",    "RX",    "RY",   "LT",    "RT",    "UP", "DOWN",
                                "LEFT", "RIGHT", "SOUTH", "EAST", "WEST",  "NORTH", "LB", "RB",
                                "VIEW", "MENU",  "LS",    "RS",   "GUIDE", "TOUCH"};
        for (int i = 0; i < 22; ++i)
            n[i] = canonical[i];
        for (int i = 22; i < 32; ++i)
            n[i] = "RESERVED_" + std::to_string(i);
        for (int i = 0; i < 64; ++i) {
            n[32 + i] = "BUTTON_" + std::to_string(i + 1);
            n[96 + i] = "AXIS_" + std::to_string(i + 1);
        }
        for (int i = 0; i < 32; ++i)
            n[160 + i] = "TRIGGER_" + std::to_string(i + 1);
        for (int i = 0; i < 16; ++i)
            n[192 + i] = "HAT_" + std::to_string(i + 1);
        for (int i = 0; i < 48; ++i)
            n[208 + i] = "VALUE_" + std::to_string(i + 1);
        return n;
    }();
    return names;
}
int Controls::Parse(const std::string &name) { const int c=GpcConstant(name);return c==-2147483647?-1:c; }
ScriptException::ScriptException(const std::string &message, int line, int column)
    : std::runtime_error(message), Line(line), Column(column) {}
std::string ScriptException::Diagnostic() const {
    return "Line " + std::to_string(Line) + ", column " + std::to_string(Column) + ": " + what();
}
} // namespace gpr
