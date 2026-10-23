#pragma once
#include <array>
#include <string>
#include <vector>
namespace gpr {
// Script-visible identifiers use Cronus numbering, never the capture codec's slots.
int GpcConstant(const std::string& name);
const std::array<int,48>& GpcSlots();
struct CompileOptions { int IntegerBits=32;bool Persist=false;std::string PersistentKey; };
struct Compatibility { std::vector<std::string> Notices; };
}
