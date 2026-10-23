#include "ScriptInternal.h"
#include <algorithm>
#include <climits>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace gpr::detail {
const std::vector<std::string> &BuiltinNames() {
    static const std::vector<std::string> names{
        "get_val",       "get_ival",       "get_lval",   "set_val",
        "event_press",   "event_release",  "get_ptime",  "get_rtime",
        "block",         "swap",           "abs",        "min",
        "max",           "clamp",          "deadzone",   "sensitivity",
        "rand",          "combo_run",      "combo_stop", "combo_restart",
        "combo_running", "combo_stop_all", "remap",      "invert",
        "is_available",  "get_controller", "get_device", "get_runtime",
        "vm_tctrl",      "random",         "get_brtime", "get_pvar",
        "set_pvar",      "get_bit",        "set_bit",    "pow",
        "isqrt",         "hypot",          "add_val",    "sin",
        "cos",           "atan2",          "lerp",       "wave_sin",
        "wave_cos",      "tick",           "get_phase",  "double_tap",
        "inv","stickize","set_polar","get_polar","get_ipolar",
        "dbyte","dchar","dword","duint8","dint8","duint16","dint16","duint32","dint32",
        "clear_bit","test_bit","get_bits","set_bits",
        "combo_suspend","combo_suspended","combo_resume","combo_suspend_all","combo_resume_all","combo_current_step","combo_step_time_left",
        "get_console","get_battery","block_all_inputs",
        "set_led","get_led","reset_leds","set_ledx","get_ledx","set_rumble","reset_rumble","block_rumble","set_rgb","set_hsb"};
    return names;
}
int FindBuiltin(const std::string &name) {
    const auto &n = BuiltinNames();
    const auto i = std::find(n.begin(), n.end(), name);
    return i == n.end() ? -1 : static_cast<int>(i - n.begin());
}
bool AcceptsBuiltin(int id, int count) noexcept {
    if(id==14)return count==4;
    if(id==15)return count==3;
    if(id==34)return count==2;
    if(id>=48){
        if(id==76||id==79||id==81)return count==2;
        if(id==77||id==80)return count==1;
        if(id==78||id==82||id==83)return count==0;
        if(id==84||id==85)return count==3;
        if(id==49)return count==3;
        if(id==50)return count==3;
        if(id==51||id==52)return count==2;
        if(id==62||id==63)return count==2;
        if(id==64)return count==3;
        if(id==65)return count==4;
        if(id==69||id==70||id==73||id==74||id==75)return count==0;
        return count==1;
    }
    switch (id) {
    case 7:
    case 21:
    case 25:
    case 26:
    case 27:
        return count == 0;
    case 3:
    case 8:
    case 9:
    case 11:
    case 12:
    case 16:
    case 22:
    case 29:
    case 32:
    case 33:
    case 35:
    case 37:
    case 38:
    case 41:
    case 47:
        return count == 2;
    case 13:
    case 34:
    case 42:
        return count == 3;
    case 31:
        return count == 4;
    case 14:
    case 15:
    case 43:
    case 44:
        return count == 2 || count == 3;
    case 39:
    case 40:
        return count == 1 || count == 2;
    default:
        return count == 1;
    }
}
bool UsesTime(int id) noexcept {
    return id == 6 || id == 7 || id == 27 || id == 30 || id == 43 || id == 44 || id == 46;
}
[[noreturn]] static void Fail(const Token &at, const std::string &message) {
    throw ScriptException(message, at.Line, at.Column);
}
int Arithmetic(Op op, int a, int b, const Token &at) {
    switch (op) {
    case Op::Add:
        return Add(a, b);
    case Op::Sub:
        return Sub(a, b);
    case Op::Mul:
        return Mul(a, b);
    case Op::Div:
        if (!b)
            Fail(at, "Division by zero.");
        return a == INT_MIN && b == -1 ? INT_MIN : a / b;
    case Op::Mod:
        if (!b)
            Fail(at, "Remainder by zero.");
        return a == INT_MIN && b == -1 ? 0 : a % b;
    case Op::BitAnd:
        return a & b;
    case Op::BitOr:
        return a | b;
    case Op::BitXor:
        return a ^ b;
    case Op::Shl:
        return Wrapped(static_cast<std::uint32_t>(a) << (b & 31));
    case Op::Shr: {
        const unsigned shift = static_cast<unsigned>(b) & 31;
        const auto bits = static_cast<std::uint32_t>(a);
        return Wrapped(!shift ? bits
                              : (bits >> shift) | (a < 0 ? ~std::uint32_t{0} << (32 - shift) : 0));
    }
    case Op::Eq:
        return a == b;
    case Op::Ne:
        return a != b;
    case Op::Lt:
        return a < b;
    case Op::Le:
        return a <= b;
    case Op::Gt:
        return a > b;
    case Op::Ge:
        return a >= b;
    default:
        Fail(at, "Unsupported arithmetic operator.");
    }
}
namespace {
template <class T> int N(const T &value) { return static_cast<int>(value.size()); }
std::uint32_t Decode(const std::string &text, std::size_t position, std::size_t &length) {
    if (position >= text.size()) {
        length = 0;
        return 0;
    }
    const auto first = static_cast<unsigned char>(text[position]);
    if (first < 128) {
        length = 1;
        return first;
    }
    length = first >= 0xc2 && first <= 0xdf   ? 2
             : first >= 0xe0 && first <= 0xef ? 3
             : first >= 0xf0 && first <= 0xf4 ? 4
                                              : 0;
    if (!length || position + length > text.size())
        throw ScriptException("Source must be valid UTF-8.");
    std::uint32_t cp = first & ((1u << (7 - length)) - 1u);
    for (std::size_t i = 1; i < length; ++i) {
        const auto c = static_cast<unsigned char>(text[position + i]);
        if ((c & 0xc0) != 0x80)
            throw ScriptException("Source must be valid UTF-8.");
        cp = (cp << 6) | (c & 0x3f);
    }
    if ((length == 2 && cp < 0x80) || (length == 3 && cp < 0x800) || (length == 4 && cp < 0x10000) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        throw ScriptException("Source must be valid UTF-8.");
    return cp;
}
bool Space(std::uint32_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
           c == 0x3000;
}
bool Letter(std::uint32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z'))
        return true;
#ifdef _WIN32
    if (cp > 127 && cp < 0xd800 || (cp > 0xdfff && cp <= 0xffff)) {
        WORD type = 0;
        const wchar_t c = static_cast<wchar_t>(cp);
        return GetStringTypeW(CT_CTYPE1, &c, 1, &type) && (type & C1_ALPHA) != 0;
    }
#else
    if (cp > 127 && cp <= 0xffff)
        return true;
#endif
    return false;
}
bool Digit(std::uint32_t c) { return c >= '0' && c <= '9'; }
class Lexer {
    const std::string &source;
    std::size_t position = 0;
    int line = 1, column = 1;
    char Current() const { return position < source.size() ? source[position] : '\0'; }
    std::uint32_t Point() const {
        std::size_t length = 0;
        return Decode(source, position, length);
    }
    char Peek() const { return position + 1 < source.size() ? source[position + 1] : '\0'; }
    void Advance() {
        std::size_t length = 0;
        const auto cp = Decode(source, position, length);
        if (cp == '\n') {
            ++line;
            column = 1;
        } else
            column += cp > 0xffff ? 2 : 1;
        position += length;
    }

  public:
    explicit Lexer(const std::string &text) : source(text) {}
    std::vector<Token> Scan() {
        std::vector<Token> result;
        while (position < source.size()) {
            if (Space(Point())) {
                Advance();
                continue;
            }
            if (Current() == '/' && Peek() == '/') {
                while (Current() != '\n' && position < source.size())
                    Advance();
                continue;
            }
            if (Current() == '/' && Peek() == '*') {
                const auto beginLine = line, beginColumn = column;
                Advance();
                Advance();
                while (position < source.size() && !(Current() == '*' && Peek() == '/'))
                    Advance();
                if (position >= source.size())
                    throw ScriptException("Unterminated block comment.", beginLine, beginColumn);
                Advance();
                Advance();
                continue;
            }
            const auto start = position;
            const int sl = line, sc = column;
            if (Current() == '"') {
                Advance();
                std::string value;
                while (Current() != '"') {
                    if (!Current() || Current() == '\n' || Current() == '\r')
                        throw ScriptException("Unterminated UI string.", sl, sc);
                    if (Current() == '\\') {
                        Advance();
                        const char c = Current();
                        if (c != '"' && c != '\\' && c != 'n' && c != 't' && c != 'r')
                            throw ScriptException(
                                "UI strings support only \\\\, \\\" , \\n, \\r and \\t escapes.", line,
                                column);
                        value += c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
                        Advance();
                    } else {
                        const auto p = position;
                        Advance();
                        value.append(source, p, position - p);
                    }
                    if (value.size() > 1024)
                        throw ScriptException("A UI string may contain at most 1,024 UTF-8 bytes.", sl,
                                              sc);
                }
                Advance();
                Token token(value, sl, sc);
                token.IsString = true;
                result.push_back(std::move(token));
            } else if (Letter(Point()) || Current() == '_') {
                do {
                    Advance();
                } while (Letter(Point()) || Digit(Point()) || Current() == '_');
                if (column - sc > 128)
                    throw ScriptException("An identifier may contain at most 128 characters.", sl, sc);
                result.emplace_back(source.substr(start, position - start), sl, sc, 0, false, true);
            } else if (Digit(Point())) {
                const bool hex = Current() == '0' && (Peek() == 'x' || Peek() == 'X');
                if (hex) {
                    Advance();
                    Advance();
                }
                const auto digits = position;
                std::uint64_t number = 0;
                bool overflow = false;
                while (true) {
                    const char c = Current();
                    const int d = c >= '0' && c <= '9'   ? c - '0'
                                  : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                         : -1;
                    if (d < 0 || d >= (hex ? 16 : 10))
                        break;
                    number = number * (hex ? 16u : 10u) + static_cast<unsigned>(d);
                    overflow |= number > UINT32_MAX;
                    if (overflow)
                        number = 0;
                    Advance();
                }
                if (position == digits || overflow)
                    throw ScriptException("Invalid 32-bit integer literal.", sl, sc);
                result.emplace_back(source.substr(start, position - start), sl, sc,
                                    Wrapped(static_cast<std::uint32_t>(number)), true);
            } else {
                const auto two = source.substr(position, 2), three = source.substr(position, 3);
                if (three == "<<=" || three == ">>=") {
                    Advance();
                    Advance();
                    Advance();
                    result.emplace_back(three, sl, sc);
                } else if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" ||
                           two == "||" || two == "^^" || two == "++" || two == "--" || two == "+=" || two == "-=" ||
                           two == "*=" || two == "/=" || two == "%=" || two == "&=" || two == "|=" ||
                           two == "^=" || two == "<<" || two == ">>") {
                    Advance();
                    Advance();
                    result.emplace_back(two, sl, sc);
                } else if (std::string("{}()[];,:?=+-*/%!~&|^<>").find(Current()) != std::string::npos) {
                    const std::string one(1, Current());
                    Advance();
                    result.emplace_back(one, sl, sc);
                } else
                    throw ScriptException(
                        "Unexpected character. Quoted text is allowed only in ui declarations.", sl, sc);
            }
            if (result.size() > 100000)
                throw ScriptException("A script may contain at most 100,000 tokens.", sl, sc);
        }
        result.emplace_back("<end>", line, column);
        return result;
    }
};
struct Expr;
struct Statement;
using E = Expr *;
using S = std::shared_ptr<Statement>;
struct Expr {
    Token At;
    std::string Kind;
    E Left = nullptr, Right = nullptr, Third = nullptr;
    std::vector<E> Args;
    Expr(Token at, std::string kind, E left = {}, E right = {})
        : At(std::move(at)), Kind(std::move(kind)), Left(left), Right(right) {}
};
struct Statement {
    Token At;
    std::string Kind, Name;
    std::vector<S> Items;
    E Expression = nullptr, Condition = nullptr, Increment = nullptr;
    S Body, Else, Initializer;
    int Size = 0, Bits=0,Columns=0;
    bool Unsigned=false;
    bool HasInitialValues = false;
    std::vector<E> InitialValues;
    Statement(Token at, std::string kind) : At(std::move(at)), Kind(std::move(kind)) {}
};
struct Function {
    Token At;
    std::string Name;
    std::vector<std::string> Parameters;
    S Body;
    bool Combo = false;
};
struct Symbol {
    int Offset = 0, Size = 0;
    bool Local = false, Array = false, ReadOnly = false, Parameter = false;
    int Bits=0,Columns=0;bool Unsigned=false;
};
struct Loop {
    std::vector<int> Breaks, Continues;
    bool Switch=false;
};
class Compiler {
    const std::string &source;
    std::vector<Token> tokens;
    int position = 0, depth = 0, globalCount = 0;
    CompileOptions options;
    std::vector<int> dataBytes;
    std::vector<std::pair<int,int>> remaps;
    std::unordered_map<std::string, int> constants, functionsByName, combosByName;
    std::unordered_map<std::string, Symbol> globals, locals;
    std::vector<Function> functions;
    std::vector<S> initializers;
    std::vector<Instruction> code;
    std::vector<Loop> loops;
    ScriptInterface interface_;
    bool hasInterface_ = false;
    // Flat expressions can contain tens of thousands of nodes before the
    // nesting check rejects them. Arena ownership prevents recursive teardown
    // of their left-associated expression chains on the error path.
    std::vector<std::unique_ptr<Expr>> expressions;
    Function *currentFunction = nullptr;
    int localCount = 0, instructionCount = 0;
    E MakeExpr(Token at, std::string kind, E left = nullptr, E right = nullptr) {
        auto e = std::make_unique<Expr>(std::move(at), std::move(kind), left, right);
        const auto p = e.get();
        expressions.push_back(std::move(e));
        return p;
    }
    const Token &Current() const { return tokens.at(position); }
    Token Next() { return tokens.at(position++); }
    bool Take(const std::string &value) {
        if (Current().IsString || Current().Text != value)
            return false;
        ++position;
        return true;
    }
    Token Expect(const std::string &value) {
        if (Current().IsString || Current().Text != value)
            Fail(Current(), "Expected '" + value + "', found '" + Current().Text + "'.");
        return Next();
    }
    Token Identifier() {
        if (!Current().IsIdentifier)
            Fail(Current(), "Expected an identifier.");
        return Next();
    }
    void EndStatement(){if(Take(";"))return;if(Current().Text=="}"||Current().Text=="<end>"||position>0&&Current().Line>tokens[position-1].Line)return;Expect(";");}
    void Enter(const Token &at) {
        if (++depth > 96)
            Fail(at, "Syntax nesting exceeds the safe limit (96).");
    }
    void Leave() { --depth; }
    static int ControlConstant(const std::string &name) {
        for (const char c : name)
            if (c >= 'a' && c <= 'z')
                return -1;
        return Controls::Parse(name);
    }
    bool Reserved(const std::string &name) const {
        static const std::unordered_set<std::string> keywords{
            "define", "const", "int", "main",  "init",     "combo",  "function", "if",
            "else",   "while", "for", "break", "continue", "return", "wait",     "ui","do","switch","case","default","enum"};
        return keywords.count(name) || constants.count(name) || ControlConstant(name) >= 0 ||
               globals.count(name) || functionsByName.count(name) || FindBuiltin(name) >= 0;
    }
    S ParseBlock() {
        const auto at = Expect("{");
        Enter(at);
        auto node = std::make_shared<Statement>(at, "block");
        while (!Take("}")) {
            if (Current().Text == "<end>")
                Fail(Current(), "Unterminated block.");
            node->Items.push_back(ParseStatement());
        }
        Leave();
        return node;
    }
    void AddGlobal(const S &node, bool readOnly) {
        if (Reserved(node->Name))
            Fail(node->At, "Duplicate or reserved name '" + node->Name + "'.");
        globals.emplace(node->Name, Symbol{globalCount, node->Size == 0 ? 1 : node->Size, false,
                                           node->Size > 0, readOnly});
        globals.at(node->Name).Bits=node->Bits;globals.at(node->Name).Unsigned=node->Unsigned;
        globals.at(node->Name).Columns=node->Columns;
        globalCount += node->Size == 0 ? 1 : node->Size;
        if (globalCount > 4096)
            Fail(node->At, "Global storage exceeds 4,096 integers.");
        initializers.push_back(node);
    }
    std::string UiString() {
        if (!Current().IsString)
            Fail(Current(), "Expected a quoted interface label.");
        return Next().Text;
    }
    void ParseInterface(const Token &at) {
        if (hasInterface_)
            Fail(at, "Declare only one ui block per script.");
        hasInterface_ = true;
        Expect("{");
        std::string group;
        while (!Take("}")) {
            const auto kind = Identifier();
            if (kind.Text == "title" || kind.Text == "description" || kind.Text == "group" ||
                kind.Text == "help") {
                Expect("(");
                const auto text = UiString();
                Expect(")");
                Expect(";");
                if (kind.Text == "title")
                    interface_.Title = text;
                else if (kind.Text == "description")
                    interface_.Description = text;
                else if (kind.Text == "group")
                    group = text;
                else {
                    if (interface_.Parameters.empty())
                        Fail(kind, "help must follow a parameter.");
                    interface_.Parameters.back().Help = text;
                }
                continue;
            }
            ScriptParameter p;
            if (kind.Text == "slider")
                p.Kind = ParameterKind::Slider;
            else if (kind.Text == "toggle")
                p.Kind = ParameterKind::Toggle;
            else if (kind.Text == "select")
                p.Kind = ParameterKind::Select;
            else if (kind.Text == "number")
                p.Kind = ParameterKind::Number;
            else
                Fail(kind,
                     "Expected title, description, group, help, slider, toggle, select or number.");
            const auto name = Identifier();
            if (Reserved(name.Text))
                Fail(name, "Duplicate or reserved parameter name '" + name.Text + "'.");
            if (interface_.Parameters.size() >= 64)
                Fail(name, "A script interface supports at most 64 parameters.");
            p.Name = name.Text;
            p.Group = group;
            Expect("(");
            p.Label = UiString();
            Expect(",");
            if (p.Label.empty())
                Fail(name, "Parameter labels cannot be empty.");
            if (p.Kind == ParameterKind::Slider || p.Kind == ParameterKind::Number) {
                p.Minimum = Constant(ParseExpression());
                Expect(",");
                p.Maximum = Constant(ParseExpression());
                Expect(",");
                p.Default = Constant(ParseExpression());
                if (Take(",")) {
                    if (!Current().IsString) {
                        p.Step = Constant(ParseExpression());
                        if (Take(","))
                            p.Unit = UiString();
                    } else
                        p.Unit = UiString();
                }
            } else {
                p.Default = Constant(ParseExpression());
                p.Minimum = 0;
                p.Maximum = 1;
            }
            Expect(")");
            if (p.Kind == ParameterKind::Select) {
                Expect("{");
                while (!Take("}")) {
                    if (p.Choices.size() >= 64)
                        Fail(Current(), "A selection supports at most 64 choices.");
                    const auto value = Constant(ParseExpression());
                    Expect(":");
                    auto label = UiString();
                    Expect(";");
                    if (label.empty() || std::any_of(p.Choices.begin(), p.Choices.end(),
                                                     [&](const auto &c) { return c.Value == value; }))
                        Fail(name, "Selection choices need unique values and nonempty labels.");
                    p.Choices.push_back({value, std::move(label)});
                }
            }
            Expect(";");
            if (p.Minimum > p.Maximum || p.Step <= 0 || !p.Accepts(p.Default))
                Fail(name, "Invalid parameter range, positive step, or default value.");
            globals.emplace(p.Name, Symbol{N(interface_.Parameters), 1, false, false, true, true});
            interface_.Parameters.push_back(std::move(p));
        }
    }
    S ParseDeclaration(const Token &at, bool semicolon) {
        auto group = std::make_shared<Statement>(at, "declarations");
        do {
            const auto name = Identifier();
            auto item = std::make_shared<Statement>(name, "declaration");
            item->Name = name.Text;
            if(at.Text=="int8"||at.Text=="uint8"||at.Text=="int16"||at.Text=="uint16"||at.Text=="int32"||at.Text=="uint32"){item->Bits=at.Text.find('8')!=std::string::npos?8:at.Text.find("16")!=std::string::npos?16:32;item->Unsigned=at.Text.front()=='u';}
            if (Take("[")) {
                if (Current().Text == "]")
                    item->Size = -1;
                else {
                    item->Size = Constant(ParseExpression());
                    if (item->Size < 1 || item->Size > 1024)
                        Fail(name, "Array length must be between 1 and 1,024.");
                }
                Expect("]");
                if(Take("[")){item->Columns=Current().Text=="]"?-1:Constant(ParseExpression());Expect("]");if(item->Columns==0||item->Columns>1024)Fail(name,"Invalid matrix width.");}
            }
            if (Take("=")) {
                if (item->Size != 0) {
                    Expect("{");
                    item->HasInitialValues = true;
                    if(item->Columns){
                        int rows=0,columns=0;
                        do{Expect("{");int width=0;do{item->InitialValues.push_back(ParseExpression());++width;}while(Take(",")&&Current().Text!="}");Expect("}");if(columns&&columns!=width)Fail(name,"Matrix rows must have equal width.");columns=width;++rows;}while(Take(",")&&Current().Text!="}");Expect("}");
                        if(item->Columns!=-1&&item->Columns!=columns||item->Size!=-1&&item->Size!=rows)Fail(name,"Matrix initializer dimensions differ.");item->Columns=columns;item->Size=columns*rows;
                    }else{
                    if (!Take("}")) {
                        do {
                            item->InitialValues.push_back(ParseExpression());
                        } while (Take(",") && Current().Text != "}");
                        Expect("}");
                    }
                    }
                    if (item->Size == -1)
                        item->Size = N(item->InitialValues);
                    if (item->Size == 0 || item->Size > 1024 || N(item->InitialValues) > item->Size)
                        Fail(name, "Invalid array initializer length (maximum 1,024 elements).");
                } else
                    item->Expression = ParseExpression();
            }
            if (item->Size == -1)
                Fail(name, "An unsized array needs an initializer.");
            if(item->Columns==-1)Fail(name,"An unsized matrix requires an initializer.");
            if(item->Columns>0&&!item->HasInitialValues){item->Size*=item->Columns;if(item->Size>1024)Fail(name,"Matrix exceeds 1,024 elements.");}
            group->Items.push_back(item);
        } while (Take(","));
        if (semicolon)
            EndStatement();
        return group;
    }
    S ParseStatement() {
        Enter(Current());
        S result;
        if (Current().Text == "{")
            result = ParseBlock();
        else {
            const auto at = Current();
            if (Take(";"))
                result = std::make_shared<Statement>(at, "empty");
            else if (Take("int"))
                result = ParseDeclaration(at, true);
            else if (Take("if")) {
                result = std::make_shared<Statement>(at, "if");
                Expect("(");
                result->Condition = ParseExpression();
                Expect(")");
                result->Body = ParseStatement();
                if (Take("else"))
                    result->Else = ParseStatement();
            } else if (Take("while")) {
                result = std::make_shared<Statement>(at, "while");
                Expect("(");
                result->Condition = ParseExpression();
                Expect(")");
                result->Body = ParseStatement();
            }else if(Take("do")){
                result=std::make_shared<Statement>(at,"do");result->Body=ParseStatement();Expect("while");Expect("(");result->Condition=ParseExpression();Expect(")");EndStatement();
            }else if(Take("switch")){
                result=std::make_shared<Statement>(at,"switch");Expect("(");result->Expression=ParseExpression();Expect(")");Expect("{");bool hadDefault=false;std::unordered_set<int> cases;
                while(!Take("}")){auto token=Next();auto branch=std::make_shared<Statement>(token,"case");if(token.Text=="case"){branch->Size=Constant(ParseExpression());if(!cases.insert(branch->Size).second)Fail(token,"Duplicate case.");}else if(token.Text=="default"){if(hadDefault)Fail(token,"Duplicate default.");hadDefault=true;branch->Name="default";}else Fail(token,"Expected case or default.");Take(":");branch->Body=ParseBlock();result->Items.push_back(branch);}
            } else if (Take("for")) {
                result = std::make_shared<Statement>(at, "for");
                Expect("(");
                if (Take("int"))
                    result->Initializer = ParseDeclaration(at, false);
                else if (Current().Text != ";") {
                    result->Initializer = std::make_shared<Statement>(Current(), "expression");
                    result->Initializer->Expression = ParseExpression();
                }
                Expect(";");
                if (Current().Text != ";")
                    result->Condition = ParseExpression();
                Expect(";");
                if (Current().Text != ")")
                    result->Increment = ParseExpression();
                Expect(")");
                result->Body = ParseStatement();
            } else if (Take("break") || Take("continue")) {
                result = std::make_shared<Statement>(at, at.Text);
                EndStatement();
            } else if (Take("return")) {
                result = std::make_shared<Statement>(at, "return");
                if (Current().Text != ";"&&Current().Text!="}")
                    result->Expression = ParseExpression();
                EndStatement();
            } else {
                result = std::make_shared<Statement>(at, "expression");
                result->Expression = ParseExpression();
                EndStatement();
            }
        }
        Leave();
        return result;
    }
    static int Precedence(const std::string &op) {
        if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" ||
            op == "&=" || op == "|=" || op == "^=" || op == "<<=" || op == ">>=")
            return 1;
        if (op == "?")
            return 2;
        if (op == "||")
            return 3;
        if (op == "&&")
            return 4;
        if(op=="^^")return 4;
        if (op == "|")
            return 5;
        if (op == "^")
            return 6;
        if (op == "&")
            return 7;
        if (op == "==" || op == "!=")
            return 8;
        if (op == "<" || op == "<=" || op == ">" || op == ">=")
            return 9;
        if (op == "<<" || op == ">>")
            return 10;
        if (op == "+" || op == "-")
            return 11;
        if (op == "*" || op == "/" || op == "%")
            return 12;
        return 0;
    }
    E ParseExpression(int minimum = 1) {
        Enter(Current());
        const auto at = Next();
        if (at.IsString)
            Fail(at, "Quoted text is allowed only in ui declarations.");
        E left = nullptr;
        if (at.IsNumber)
            left = MakeExpr(at, "number");
        else if (at.Text == "(") {
            left = ParseExpression();
            Expect(")");
        } else if (at.Text == "+" || at.Text == "-" || at.Text == "!" || at.Text == "~" ||
                   at.Text == "++" || at.Text == "--")
            left = MakeExpr(at, "unary", ParseExpression(13));
        else if (at.IsIdentifier)
            left = MakeExpr(at, "name");
        else
            Fail(at, "Expected an expression.");
        while (true) {
            if (Current().Text == "(" && left->Kind == "name") {
                Next();
                auto call = MakeExpr(left->At, "call");
                if (!Take(")")) {
                    do {
                        call->Args.push_back(ParseExpression());
                        if (call->Args.size() > 16)
                            Fail(call->At, "At most 16 arguments are supported.");
                    } while (Take(","));
                    Expect(")");
                }
                left = call;
                continue;
            }
            if (Take("[")) {
                if (left->Kind != "name"&&left->Kind!="index")
                    Fail(left->At, "Only named arrays may be indexed.");
                if(left->Kind=="index"){if(left->Third)Fail(left->At,"At most two array dimensions are supported.");left->Third=ParseExpression();}
                else left = MakeExpr(left->At, "index", ParseExpression());
                Expect("]");
                continue;
            }
            if (Current().Text == "++" || Current().Text == "--") {
                left = MakeExpr(Next(), "postfix", left);
                continue;
            }
            const int p = Precedence(Current().Text);
            if (p < minimum || !p)
                break;
            const auto op = Next();
            if (op.Text == "?") {
                auto ternary = MakeExpr(op, "conditional", left, ParseExpression());
                Expect(":");
                ternary->Third = ParseExpression(2);
                left = ternary;
            } else
                left = MakeExpr(op, p == 1 ? "assign" : "binary", left,
                                ParseExpression(p == 1 ? p : p + 1));
        }
        Leave();
        return left;
    }
    static Op BinaryOp(const Token &at) {
        const auto &op = at.Text;
        if (op == "+" || op == "+=")
            return Op::Add;
        if (op == "-" || op == "-=")
            return Op::Sub;
        if (op == "*" || op == "*=")
            return Op::Mul;
        if (op == "/" || op == "/=")
            return Op::Div;
        if (op == "%" || op == "%=")
            return Op::Mod;
        if (op == "&" || op == "&=")
            return Op::BitAnd;
        if (op == "|" || op == "|=")
            return Op::BitOr;
        if (op == "^" || op == "^=")
            return Op::BitXor;
        if (op == "<<" || op == "<<=")
            return Op::Shl;
        if (op == ">>" || op == ">>=")
            return Op::Shr;
        if (op == "==")
            return Op::Eq;
        if (op == "!=")
            return Op::Ne;
        if (op == "<")
            return Op::Lt;
        if (op == "<=")
            return Op::Le;
        if (op == ">")
            return Op::Gt;
        if (op == ">=")
            return Op::Ge;
        Fail(at, "Unsupported binary operator.");
    }
    int Constant(const E &e) {
        Enter(e->At);
        const int result = ConstantCore(e);
        Leave();
        return result;
    }
    int ConstantCore(const E &e) {
        const auto &kind = e->Kind;
        const auto &op = e->At.Text;
        if (kind == "number")
            return e->At.Number;
        if (kind == "name") {
            const auto i = constants.find(op);
            if (i != constants.end())
                return i->second;
            const auto c = ControlConstant(op);
            if (c >= 0)
                return c;
        }
        if (kind == "unary") {
            const int a = Constant(e->Left);
            if (op == "+")
                return a;
            if (op == "-")
                return Sub(0, a);
            if (op == "!")
                return !a;
            if (op == "~")
                return ~a;
        }
        if (kind == "binary") {
            if(op=="^^")return bool(Constant(e->Left))!=bool(Constant(e->Right));
            const int a = Constant(e->Left);
            if (op == "&&" && !a)
                return 0;
            if (op == "||" && a)
                return 1;
            const int b = Constant(e->Right);
            if (op == "&&" || op == "||")
                return b != 0;
            return Arithmetic(BinaryOp(e->At), a, b, e->At);
        }
        if (kind == "conditional")
            return Constant(e->Left) ? Constant(e->Right) : Constant(e->Third);
        Fail(e->At, "A constant integer expression is required here.");
    }
    int Emit(Op op, int a, int b, const Token &at) {
        if (++instructionCount > 32768)
            Fail(at, "Compiled program exceeds 32,768 instructions.");
        code.push_back({op, a, b, at.Line, at.Column});
        return N(code) - 1;
    }
    void Patch(int index, int target) { code[index].A = target; }
    Symbol Resolve(const E &e) {
        auto i = locals.find(e->At.Text);
        Symbol s;
        if (i != locals.end())
            s = i->second;
        else {
            const auto g = globals.find(e->At.Text);
            if (g == globals.end())
                Fail(e->At, "Unknown variable '" + e->At.Text + "'.");
            s = g->second;
        }
        if ((e->Kind == "index") != s.Array)
            Fail(e->At, s.Array ? "An array needs an index." : "This variable is not an array.");
        if(s.Array&&bool(e->Third)!=bool(s.Columns))Fail(e->At,"Array index dimensions differ.");
        return s;
    }
    void CompileDeclaration(const S &node, bool local) {
        Symbol symbol;
        if (local) {
            if (Reserved(node->Name) || locals.count(node->Name))
                Fail(node->At,
                     "Duplicate or reserved variable '" + node->Name + "'. Locals are function-scoped.");
            symbol = {localCount, node->Size == 0 ? 1 : node->Size, true, node->Size > 0, false};
            symbol.Bits=node->Bits;symbol.Unsigned=node->Unsigned;symbol.Columns=node->Columns;
            locals.emplace(node->Name, symbol);
            localCount += symbol.Size;
            if (localCount > 256)
                Fail(node->At,
                     "A function may use at most 256 local integers, including arguments and arrays.");
        } else
            symbol = globals.at(node->Name);
        if (!symbol.Array) {
            if (node->Expression)
                CompileExpression(node->Expression);
            else
                Emit(Op::Push, 0, 0, node->At);
            if(symbol.Bits)Emit(Op::Narrow,symbol.Bits,symbol.Unsigned?1:0,node->At);
            Emit(symbol.Local ? Op::StoreLocal : Op::Store, symbol.Offset, 0, node->At);
            Emit(Op::Pop, 0, 0, node->At);
        } else if (node->HasInitialValues)
            for (int i = 0; i < N(node->InitialValues); ++i) {
                Emit(Op::Push, i, 0, node->At);
                CompileExpression(node->InitialValues[i]);
                if(symbol.Bits)Emit(Op::Narrow,symbol.Bits,symbol.Unsigned?1:0,node->At);
                Emit(Op::StoreArray, symbol.Offset, symbol.Size | (symbol.Local ? 65536 : 0), node->At);
                Emit(Op::Pop, 0, 0, node->At);
            }
    }
    void CompileStatement(const S &n) {
        const auto &kind = n->Kind;
        if(kind=="do"){
            loops.push_back({});int start=N(code);CompileStatement(n->Body);int test=N(code);CompileExpression(n->Condition);int end=Emit(Op::JumpZero,0,0,n->At);Emit(Op::Jump,start,0,n->At);Patch(end,N(code));for(int p:loops.back().Breaks)Patch(p,N(code));for(int p:loops.back().Continues)Patch(p,test);loops.pop_back();return;
        }
        if(kind=="switch"){
            const int slot=localCount++;if(localCount>256)Fail(n->At,"Local storage exhausted by switch.");CompileExpression(n->Expression);Emit(Op::StoreLocal,slot,0,n->At);Emit(Op::Pop,0,0,n->At);std::vector<int> jumps;int otherwise=-1;
            for(size_t i=0;i<n->Items.size();++i){const auto& branch=n->Items[i];if(branch->Name=="default"){otherwise=static_cast<int>(i);jumps.push_back(-1);continue;}Emit(Op::LoadLocal,slot,0,n->At);Emit(Op::Push,branch->Size,0,n->At);Emit(Op::Eq,0,0,n->At);const int skip=Emit(Op::JumpZero,0,0,n->At);jumps.push_back(Emit(Op::Jump,0,0,n->At));Patch(skip,N(code));}
            const int fall=Emit(Op::Jump,0,0,n->At);loops.push_back({});loops.back().Switch=true;
            for(size_t i=0;i<n->Items.size();++i){if(jumps[i]>=0)Patch(jumps[i],N(code));if(static_cast<int>(i)==otherwise)Patch(fall,N(code));CompileStatement(n->Items[i]->Body);}
            if(otherwise<0)Patch(fall,N(code));for(int p:loops.back().Breaks)Patch(p,N(code));const auto continues=loops.back().Continues;loops.pop_back();if(!continues.empty()){if(loops.empty())Fail(n->At,"continue requires a loop.");loops.back().Continues.insert(loops.back().Continues.end(),continues.begin(),continues.end());}return;
        }
        if (kind == "empty")
            return;
        if (kind == "block" || kind == "declarations") {
            for (const auto &item : n->Items)
                CompileStatement(item);
            return;
        }
        if (kind == "declaration") {
            CompileDeclaration(n, true);
            return;
        }
        if (kind == "expression") {
            if(n->Expression->Kind=="call"&&n->Expression->At.Text=="call"){
                const auto e=n->Expression;
                if(!currentFunction->Combo||e->Args.size()!=1||e->Args[0]->Kind!="name"||!combosByName.count(e->Args[0]->At.Text))Fail(e->At,"call requires a combo name inside a combo.");
                Emit(Op::ComboCall,functionsByName.at(e->Args[0]->At.Text),0,e->At);Emit(Op::Pop,0,0,e->At);return;
            }
            if (n->Expression->Kind == "call" && n->Expression->At.Text == "wait") {
                if (!currentFunction->Combo || n->Expression->Args.size() != 1)
                    Fail(n->Expression->At, "wait(milliseconds) is only valid as a standalone statement "
                                            "directly inside a combo.");
                CompileExpression(n->Expression->Args[0]);
                Emit(Op::Wait, 0, 0, n->Expression->At);
                return;
            }
            CompileExpression(n->Expression);
            Emit(Op::Pop, 0, 0, n->At);
            return;
        }
        if (kind == "if") {
            CompileExpression(n->Condition);
            const int no = Emit(Op::JumpZero, 0, 0, n->At);
            CompileStatement(n->Body);
            if (!n->Else)
                Patch(no, N(code));
            else {
                const int end = Emit(Op::Jump, 0, 0, n->At);
                Patch(no, N(code));
                CompileStatement(n->Else);
                Patch(end, N(code));
            }
            return;
        }
        if (kind == "while" || kind == "for") {
            if (n->Initializer && n->Initializer->Kind == "expression" &&
                n->Initializer->Expression->Kind == "call" &&
                n->Initializer->Expression->At.Text == "wait")
                Fail(n->Initializer->Expression->At,
                     "wait(milliseconds) must be a standalone statement directly inside a combo.");
            if (n->Initializer)
                CompileStatement(n->Initializer);
            const int start = N(code);
            int exit = -1;
            if (n->Condition) {
                CompileExpression(n->Condition);
                exit = Emit(Op::JumpZero, 0, 0, n->At);
            }
            loops.emplace_back();
            CompileStatement(n->Body);
            const int increment = N(code);
            if (n->Increment) {
                CompileExpression(n->Increment);
                Emit(Op::Pop, 0, 0, n->At);
            }
            Emit(Op::Jump, start, 0, n->At);
            if (exit >= 0)
                Patch(exit, N(code));
            for (const auto jump : loops.back().Breaks)
                Patch(jump, N(code));
            for (const auto jump : loops.back().Continues)
                Patch(jump, increment);
            loops.pop_back();
            return;
        }
        if (kind == "break" || kind == "continue") {
            if (loops.empty())
                Fail(n->At, kind + " requires a surrounding loop.");
            const auto branch = Emit(Op::Jump, 0, 0, n->At);
            (kind == "break" ? loops.back().Breaks : loops.back().Continues).push_back(branch);
            return;
        }
        if (kind == "return") {
            if (n->Expression)
                CompileExpression(n->Expression);
            else
                Emit(Op::Push, 0, 0, n->At);
            Emit(Op::Return, 0, 0, n->At);
            return;
        }
        Fail(n->At, "Unsupported statement.");
    }
    void Load(const E &e, const Symbol &s) {
        if (s.Array) {
            CompileIndex(e,s);
            Emit(Op::LoadArray, s.Offset, s.Size | (s.Local ? 65536 : 0), e->At);
        } else
            Emit(s.Parameter ? Op::LoadParameter
                 : s.Local   ? Op::LoadLocal
                             : Op::Load,
                 s.Offset, 0, e->At);
    }
    void CompileIndex(const E& e,const Symbol& s){CompileExpression(e->Left);if(e->Third){CompileExpression(e->Third);Emit(Op::Index2,s.Size/s.Columns,s.Columns,e->At);}}
    void Store(const E &e, const Symbol &s) {
        if (s.ReadOnly)
            Fail(e->At, "A constant or UI parameter is read-only in script code.");
        if(s.Bits)Emit(Op::Narrow,s.Bits,s.Unsigned?1:0,e->At);
        Emit(s.Array   ? Op::StoreArray
             : s.Local ? Op::StoreLocal
                       : Op::Store,
             s.Offset, s.Array ? s.Size | (s.Local ? 65536 : 0) : 0, e->At);
    }
    void CompileExpression(const E &e) {
        Enter(e->At);
        CompileExpressionCore(e);
        Leave();
    }
    void CompileExpressionCore(const E &e) {
        const auto at = e->At;
        const auto &kind = e->Kind;
        if (kind == "number") {
            Emit(Op::Push, at.Number, 0, at);
            return;
        }
        if (kind == "name") {
            const auto c = constants.find(at.Text);
            const auto combo = combosByName.find(at.Text);
            const int control = ControlConstant(at.Text);
            if (c != constants.end() || control >= 0 || combo != combosByName.end()) {
                Emit(Op::Push,
                     c != constants.end() ? c->second
                     : control >= 0       ? control
                                          : combo->second,
                     0, at);
                return;
            }
            Load(e, Resolve(e));
            return;
        }
        if (kind == "index") {
            Load(e, Resolve(e));
            return;
        }
        if (kind == "conditional") {
            CompileExpression(e->Left);
            const auto no = Emit(Op::JumpZero, 0, 0, at);
            CompileExpression(e->Right);
            const auto end = Emit(Op::Jump, 0, 0, at);
            Patch(no, N(code));
            CompileExpression(e->Third);
            Patch(end, N(code));
            return;
        }
        if (kind == "binary") {
            if(at.Text=="^^"){CompileExpression(e->Left);Emit(Op::Bool,0,0,at);CompileExpression(e->Right);Emit(Op::Bool,0,0,at);Emit(Op::BitXor,0,0,at);return;}
            if (at.Text == "&&" || at.Text == "||") {
                CompileExpression(e->Left);
                const auto skip = Emit(Op::JumpZero, 0, 0, at);
                if (at.Text == "&&") {
                    CompileExpression(e->Right);
                    Emit(Op::Bool, 0, 0, at);
                    const auto done = Emit(Op::Jump, 0, 0, at);
                    Patch(skip, N(code));
                    Emit(Op::Push, 0, 0, at);
                    Patch(done, N(code));
                } else {
                    Emit(Op::Push, 1, 0, at);
                    const auto done = Emit(Op::Jump, 0, 0, at);
                    Patch(skip, N(code));
                    CompileExpression(e->Right);
                    Emit(Op::Bool, 0, 0, at);
                    Patch(done, N(code));
                }
                return;
            }
            CompileExpression(e->Left);
            CompileExpression(e->Right);
            Emit(BinaryOp(at), 0, 0, at);
            return;
        }
        if (kind == "assign") {
            const auto target = e->Left;
            if (target->Kind != "name" && target->Kind != "index")
                Fail(at, "Assignment needs a variable or an indexed array.");
            const auto s = Resolve(target);
            if (s.Array) {
                CompileIndex(target,s);
                if (at.Text != "=") {
                    Emit(Op::Dup, 0, 0, at);
                    Emit(Op::LoadArray, s.Offset, s.Size | (s.Local ? 65536 : 0), at);
                }
            } else if (at.Text != "=")
                Load(target, s);
            CompileExpression(e->Right);
            if (at.Text != "=")
                Emit(BinaryOp(at), 0, 0, at);
            Store(target, s);
            return;
        }
        if (kind == "postfix" || kind == "unary") {
            if (at.Text == "++" || at.Text == "--") {
                const auto ref = e->Left;
                if (ref->Kind != "name" && ref->Kind != "index")
                    Fail(at, "Increment needs a variable or an indexed array.");
                const auto s = Resolve(ref);
                if (s.Array) {
                    CompileIndex(ref,s);
                    Emit(Op::Dup, 0, 0, at);
                    Emit(Op::LoadArray, s.Offset, s.Size | (s.Local ? 65536 : 0), at);
                } else
                    Load(ref, s);
                Emit(Op::Push, 1, 0, at);
                Emit(at.Text == "++" ? Op::Add : Op::Sub, 0, 0, at);
                Store(ref, s);
                if (kind == "postfix") {
                    Emit(Op::Push, 1, 0, at);
                    Emit(at.Text == "++" ? Op::Sub : Op::Add, 0, 0, at);
                }
                return;
            }
            CompileExpression(e->Left);
            if (at.Text != "+")
                Emit(at.Text == "-" ? Op::Neg : at.Text == "!" ? Op::Not : Op::BitNot, 0, 0, at);
            return;
        }
        if (kind == "call") {
            if (at.Text == "wait")
                Fail(at, "wait(milliseconds) must be a standalone statement directly inside a combo.");
            const auto builtin = FindBuiltin(at.Text);
            if (builtin >= 0) {
                if(builtin==74)Fail(at,"get_battery needs battery telemetry not provided by this driver SDK.");
                if(at.Text=="set_bit"||at.Text=="clear_bit"||at.Text=="set_bits"){
                    const int needed=at.Text=="set_bits"?4:2;
                    if(N(e->Args)!=needed)Fail(at,"Invalid bit function argument count.");
                    const auto target=e->Args[0];if(target->Kind!="name"&&target->Kind!="index")Fail(at,"Bit mutation requires a variable as first argument.");
                    const auto symbol=Resolve(target);if(symbol.Array)CompileIndex(target,symbol);
                    for(const auto& arg:e->Args)CompileExpression(arg);
                    if(at.Text=="set_bit"){Emit(Op::Push,1,0,at);Emit(Op::Builtin,34,3,at);}else Emit(Op::Builtin,builtin,N(e->Args),at);
                    Store(target,symbol);return;
                }
                if (!AcceptsBuiltin(builtin, N(e->Args)))
                    Fail(at, "Invalid argument count for " + at.Text + ".");
                for (const auto &a : e->Args)
                    CompileExpression(a);
                Emit(Op::Builtin, builtin, N(e->Args), at);
                return;
            }
            const auto fn = functionsByName.find(at.Text);
            if (fn == functionsByName.end() || functions[fn->second].Combo || at.Text == "main" ||
                at.Text == "init")
                Fail(at, "Unsupported or unknown GPC function '" + at.Text + "'. Physical feedback, console authentication, USB ports and proprietary GamePacks require Cronus hardware; see Compatibility.");
            if (e->Args.size() != functions[fn->second].Parameters.size())
                Fail(at, "Invalid argument count for " + at.Text + ".");
            for (const auto &a : e->Args)
                CompileExpression(a);
            Emit(Op::Call, fn->second, N(e->Args), at);
            return;
        }
        Fail(at, "Unsupported expression.");
    }

  public:
    explicit Compiler(const std::string &text,CompileOptions settings) : source(text), tokens(Lexer(text).Scan()),options(settings) {
        if(options.IntegerBits!=16&&options.IntegerBits!=32)throw ScriptException("Select 16-bit or 32-bit GPC integers.");
        constants["TRUE"] = constants["true"] = 1;
        constants["FALSE"] = constants["false"] = 0;
        constants["NOT_USE"]=-1;
    }
    std::shared_ptr<const ProgramData> Compile() {
        while (Current().Text != "<end>" || Current().IsString) {
            const auto top = Next();
            if (!top.IsIdentifier)
                Fail(top, "Expected a declaration, entry block or ui block.");
            if(top.Text=="enum"){
                Expect("{");int value=0;while(!Take("}")){const auto name=Identifier();if(Reserved(name.Text))Fail(name,"Duplicate enum name.");if(Take("="))value=Constant(ParseExpression());constants.emplace(name.Text,value);value=Add(value,1);if(!Take(",")){Expect("}");break;}}Take(";");
            }else if(top.Text=="remap"||top.Text=="unmap"){
                const auto from=Identifier();const int a=Constant(MakeExpr(from,"name"));int b=-1;
                if(top.Text=="remap"){Expect("-");Expect(">");b=Constant(ParseExpression());}
                Expect(";");if(a<0||a>47||b>47||b< -1)Fail(top,"Remap identifiers must be in 0..47.");
                if(std::find_if(remaps.begin(),remaps.end(),[&](const auto& p){return p.first==a;})!=remaps.end())Fail(top,"Duplicate remap source.");remaps.emplace_back(a,b);
            }else if(top.Text=="data"){
                if(!dataBytes.empty())Fail(top,"Only one data section is allowed.");
                Expect("(");if(!Take(")")){do{const int v=Constant(ParseExpression());if(v< -128||v>255)Fail(top,"Data byte outside -128..255.");dataBytes.push_back(v&255);if(dataBytes.size()>65536)Fail(top,"Data section exceeds 64 KiB.");}while(Take(","));Expect(")");}Expect(";");
            }else if (top.Text == "ui") {
                Fail(top,"The .gpc format does not contain .igc ui declarations.");
            } else if (top.Text == "define" || top.Text == "const") {
                Token type=top;
                if (top.Text == "const")
                    if(Current().Text=="int"||Current().Text=="int8"||Current().Text=="uint8"||Current().Text=="int16"||Current().Text=="uint16"||Current().Text=="int32"||Current().Text=="uint32")type=Next();
                const auto name = Identifier();
                if (top.Text == "const" && Current().Text == "[") {
                    --position;
                    const auto declaration = ParseDeclaration(type, true);
                    for (const auto &entry : declaration->Items) {
                        if (!entry->Size || !entry->HasInitialValues)
                            Fail(entry->At, "A constant array needs an initializer.");
                        for (const auto &value : entry->InitialValues)
                            (void)Constant(value);
                        AddGlobal(entry, true);
                    }
                    continue;
                }
                if (Reserved(name.Text))
                    Fail(name, "Duplicate or reserved name '" + name.Text + "'.");
                Expect("=");
                const int value = Constant(ParseExpression());
                constants.emplace(name.Text, value);
                while(Take(",")){const auto next=Identifier();if(Reserved(next.Text))Fail(next,"Duplicate definition.");Expect("=");const int v=Constant(ParseExpression());constants.emplace(next.Text,v);}EndStatement();
            } else if (top.Text == "int"||top.Text=="int8"||top.Text=="uint8"||top.Text=="int16"||top.Text=="uint16"||top.Text=="int32"||top.Text=="uint32") {
                const auto declaration = ParseDeclaration(top, true);
                for (const auto &entry : declaration->Items)
                    AddGlobal(entry, false);
            } else if (top.Text == "main" || top.Text == "init" || top.Text == "combo" ||
                       top.Text == "function") {
                Function fn;
                fn.At = top;
                fn.Name = top.Text;
                fn.Combo = top.Text == "combo";
                if (top.Text == "combo" || top.Text == "function") {
                    fn.Name = Identifier().Text;
                    if (Reserved(fn.Name))
                        Fail(top, "Duplicate or reserved function name '" + fn.Name + "'.");
                } else if (functionsByName.count(fn.Name))
                    Fail(top, "Duplicate or reserved function name '" + fn.Name + "'.");
                if (functions.size() >= 96)
                    Fail(top, "At most 96 functions, including main, init and combos, are supported.");
                functionsByName.emplace(fn.Name, N(functions));
                if (fn.Combo) {
                    if (combosByName.size() >= 32)
                        Fail(top, "At most 32 combos are supported.");
                    combosByName.emplace(fn.Name, N(combosByName));
                }
                if (top.Text == "function") {
                    Expect("(");
                    if (!Take(")")) {
                        do {
                            Take("int");
                            const auto parameter = Identifier();
                            if (std::find(fn.Parameters.begin(), fn.Parameters.end(), parameter.Text) !=
                                    fn.Parameters.end() ||
                                constants.count(parameter.Text)||ControlConstant(parameter.Text)>=0||FindBuiltin(parameter.Text)>=0)
                                Fail(parameter, "Duplicate or reserved parameter name.");
                            fn.Parameters.push_back(parameter.Text);
                            if (fn.Parameters.size() > 16)
                                Fail(parameter, "At most 16 function arguments are supported.");
                        } while (Take(","));
                        Expect(")");
                    }
                }
                fn.Body = ParseBlock();
                functions.push_back(std::move(fn));
            } else
                Fail(top, "Expected define, int, init, main, function or combo.");
        }
        if (!functionsByName.count("main"))
            throw ScriptException("A script needs a main { ... } block.");
        if (!functionsByName.count("init")) {
            if (functions.size() >= 96)
                Fail(Current(), "At most 96 functions, including main, init and combos, are supported.");
            functionsByName.emplace("init", N(functions));
            Function fn;
            fn.Name = "init";
            fn.At = Token("init");
            fn.Body = std::make_shared<Statement>(Token("{"), "block");
            functions.push_back(std::move(fn));
        }
        auto program = std::make_shared<ProgramData>();
        program->IntegerBits=options.IntegerBits;
        if(options.Persist){program->PrivateMemory=PersistentMemory::Open("script:"+(options.PersistentKey.empty()?source:options.PersistentKey));program->GlobalMemory=PersistentMemory::Open("global-pvar");}
        program->Data=std::move(dataBytes);program->Remaps=std::move(remaps);
        program->Source = source;
        program->Interface = std::move(interface_);
        program->GlobalCount = globalCount;
        program->Init = functionsByName.at("init");
        program->Main = functionsByName.at("main");
        for (auto &fn : functions) {
            currentFunction = &fn;
            code.clear();
            locals.clear();
            localCount = 0;
            for (const auto &arg : fn.Parameters) {
                locals.emplace(arg, Symbol{localCount++, 1, true, false, false});
            }
            if (fn.Name == "init")
                for (const auto &declaration : initializers)
                    CompileDeclaration(declaration, false);
            CompileStatement(fn.Body);
            Emit(Op::Push, 0, 0, fn.At);
            Emit(Op::Return, 0, 0, fn.At);
            program->Blocks.push_back(
                {fn.Name, std::move(code), N(fn.Parameters), localCount, fn.Combo});
        }
        program->ComboBlocks.resize(combosByName.size());
        bool virtualIo=false;
        for(const auto& block:program->Blocks)for(const auto& op:block.Code)if(op.Code==Op::Builtin&&op.A>=76)virtualIo=true;
        if(virtualIo)program->Report.Notices.push_back("Virtual Cronus LED/rumble commands: shown in the app monitor; not sent to physical controller motors or LEDs.");
        bool identity=false;for(const auto& block:program->Blocks)for(const auto& op:block.Code)if(op.Code==Op::Builtin&&(op.A==25||op.A==73))identity=true;
        if(identity)program->Report.Notices.push_back("Platform identity is a Windows compatibility profile (XUSB=Xbox 360, GIP=Xbox One, HID=PS4 default), not a physical Cronus console port.");
        for (const auto &item : combosByName)
            program->ComboBlocks[item.second] = functionsByName.at(item.first);
        return program;
    }
};
} // namespace
std::shared_ptr<const ProgramData> CompileProgram(const std::string &text,CompileOptions options) {
    std::size_t units = 0;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t length = 0;
        const auto cp = Decode(text, i, length);
        i += length;
        units += cp > 0xffff ? 2 : 1;
        if (units > 524288)
            throw ScriptException("A script may contain at most 512 KiB of text.");
    }
    return Compiler(text,options).Compile();
}
} // namespace gpr::detail
namespace gpr {
ScriptProgram::ScriptProgram(std::shared_ptr<const detail::ProgramData> data) : data_(std::move(data)) {}
const std::string &ScriptProgram::Source() const noexcept { return data_->Source; }
const ScriptInterface &ScriptProgram::Interface() const noexcept { return data_->Interface; }
const Compatibility &ScriptProgram::Report() const noexcept { return data_->Report; }
bool ScriptParameter::Accepts(int value) const noexcept {
    if (Kind == ParameterKind::Select)
        return std::any_of(Choices.begin(), Choices.end(),
                           [value](const auto &c) { return c.Value == value; });
    return value >= Minimum && value <= Maximum && Step > 0 &&
           (static_cast<std::int64_t>(value) - Minimum) % Step == 0;
}
ParameterValues ScriptInterface::Defaults() const noexcept {
    ParameterValues result{};
    for (std::size_t i = 0; i < Parameters.size(); ++i)
        result[i] = Parameters[i].Default;
    return result;
}
void ScriptInterface::Validate(const ParameterValues &values) const {
    for (std::size_t i = 0; i < Parameters.size(); ++i)
        if (!Parameters[i].Accepts(values[i]))
            throw std::invalid_argument("Invalid value for script parameter '" + Parameters[i].Name +
                                        "'.");
}
std::shared_ptr<const ScriptProgram> ScriptCompiler::Compile(const std::string &source,CompileOptions options) {
    return std::shared_ptr<const ScriptProgram>(new ScriptProgram(detail::CompileProgram(source,options)));
}
} // namespace gpr
