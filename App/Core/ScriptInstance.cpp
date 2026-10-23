#include "ScriptInternal.h"
#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <limits>

namespace gpr {
namespace {
using detail::CodeBlock;
using detail::Instruction;
using detail::Op;
using I64 = std::int64_t;
struct ReturnPoint {
    int Block = 0, Pc = 0, LocalBase = 0, LocalTop = 0, Stack = 0;
};
struct VmState {
    std::array<int, 1024> Stack{};
    std::vector<int> Locals;
    std::array<ReturnPoint, 32> Calls{};
    int Block = 0, Pc = 0, Sp = 0, LocalBase = 0, LocalTop = 0, Depth = 0;
    explicit VmState(int capacity) : Locals(static_cast<std::size_t>(capacity)) {}
    void Start(int block, const CodeBlock &definition) {
        Block = block;
        Pc = Sp = LocalBase = Depth = 0;
        LocalTop = definition.Locals;
        if (LocalTop)
            std::fill_n(Locals.begin(), LocalTop, 0);
    }
};
struct ComboState {
    VmState Vm;
    std::array<I64, 256> Offset{};
    std::array<int, 256> Lower{}, Upper{};
    std::array<bool, 256> HasHeld{};
    bool Running = false,Suspended=false;
    I64 PauseLeft=0;
    int Step=0;
    I64 Deadline = 0;
    std::uint32_t Generation = 0;
    explicit ComboState(int capacity) : Vm(capacity) {}
    void Clear() {
        Running = false;
        Suspended=false;Step=0;PauseLeft=0;
        Deadline = 0;
        ++Generation;
        HasHeld.fill(false);
    }
};
enum class Outcome { Completed, Waiting, Replaced, Cancelled };
int Neutral(int) noexcept { return 0; }
bool Active(int index, int value, bool valid) noexcept {
    (void)index;return valid && value != 0;
}
int SafeAbs(int value) noexcept { return value == INT_MIN ? INT_MAX : std::abs(value); }
int Rounded(double value) noexcept {
    return value <= INT_MIN ? INT_MIN : value >= INT_MAX ? INT_MAX : static_cast<int>(std::round(value));
}
int Trig(double degrees, int amplitude, bool cosine) noexcept {
    double reduced = std::fmod(degrees, 360);
    if (reduced < 0)
        reduced += 360;
    if (cosine)
        reduced = std::fmod(reduced + 90, 360);
    if (reduced == 0 || reduced == 180)
        return 0;
    if (reduced == 90)
        return amplitude;
    if (reduced == 270)
        return amplitude == INT_MIN ? INT_MAX : -amplitude;
    if (reduced == 30 || reduced == 150)
        return Rounded(amplitude * 0.5);
    if (reduced == 210 || reduced == 330)
        return Rounded(amplitude * -0.5);
    return Rounded(std::sin(reduced * (3.14159265358979323846 / 180)) * amplitude);
}
int Milliseconds(I64 microseconds) noexcept {
    return static_cast<int>(std::min<I64>(INT_MAX, microseconds / 1000));
}
void Bounds(int id, int &minimum, int &maximum) noexcept {
    minimum=(id>=9&&id<=12)||(id>=21&&id<=26)?-100:id>=44&&id<=47?-32768:id>=38&&id<=43?INT_MIN:0;
    maximum=id>=44&&id<=47?32767:id>=38&&id<=43?INT_MAX:100;
}
int Bounded(I64 value, int minimum, int maximum) noexcept {
    return static_cast<int>(std::clamp<I64>(value, minimum, maximum));
}
} // namespace
struct ScriptInstance::Impl {
    std::shared_ptr<const detail::ProgramData> program;
    std::vector<int> globals;
    ParameterValues parameters{};
    std::array<int, 256> previous{}, savedReleaseTime{}, comboBase{};
    std::array<I64, 256> changedAt{}, blockedUntil{}, lastReleasedAt{}, lastPressedAt{},
        betweenPresses{};
    std::array<bool, 256> hasPressed{}, priorPress{}, pressed{}, released{}, previousAvailable{};
    std::array<int, 80> pvars{};
    std::array<bool, 80> hasPvar{};
    std::array<int,4> leds{},rumble{};
    std::array<int,3> rgb{};
    std::unique_ptr<VmState> main;
    std::vector<std::unique_ptr<ComboState>> combos;
    ControlFrame *frame = nullptr;
    ComboState *currentCombo = nullptr;
    Instruction currentInstruction{};
    int currentComboIndex = -1;
    I64 now = 0, startTime = 0, previousTime = 0;
    int elapsed = 0, remaining = 0, tickMilliseconds = 10, activeTickMilliseconds = 0, nextWake = 0;
    std::atomic<int> budget{10000};
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    std::uint32_t randomState = 0xA341316Cu;
    bool initialized = false, timeAware = false, forcedTicks = false;
    I64 nextCycle=0;
    std::array<int,256> lastOutput{};
    explicit Impl(std::shared_ptr<const detail::ProgramData> p)
        : program(std::move(p)), globals(static_cast<std::size_t>(program->GlobalCount)) {
        parameters = program->Interface.Defaults();
        int maxLocals = 0;
        for (const auto &block : program->Blocks)
            maxLocals = std::max(maxLocals, block.Locals);
        const int capacity = std::min(8192, maxLocals * 33);
        main = std::make_unique<VmState>(capacity);
        combos.reserve(program->ComboBlocks.size());
        for (std::size_t i = 0; i < program->ComboBlocks.size(); ++i)
            combos.push_back(std::make_unique<ComboState>(capacity));
    }
    [[noreturn]] void Error(const std::string &message) const {
        throw ScriptException(message, currentInstruction.Line, currentInstruction.Column);
    }
    int Control(int id) const {
        if (static_cast<unsigned>(id) >= 256)
            Error("Control identifier must be between 0 and 255.");
        return id;
    }
    int Combo(int id) const {
        if (static_cast<unsigned>(id) >= combos.size())
            Error("Unknown combo identifier.");
        return id;
    }
    I64 Deadline(int ms) const noexcept {
        const I64 duration = static_cast<I64>(ms) * 1000;
        return now > INT64_MAX - duration ? INT64_MAX : now + duration;
    }
    void Schedule(I64 deadline) noexcept {
        const I64 delta = deadline > now ? deadline - now : 0;
        const int ms = static_cast<int>(
            std::min<I64>(INT_MAX, std::max<I64>(1, delta / 1000 + (delta % 1000 != 0))));
        if (!nextWake || ms < nextWake)
            nextWake = ms;
    }
    void Set(int id, I64 value) {
        id = Control(id);
        if(id>=21&&id<=37&&!frame->Available[id])Error("This sensor/touch/paddle control is not exposed by the current Interceptor mapping; no fabricated value was used.");
        int low, high;
        Bounds(id, low, high);
        int v = Bounded(value, low, high);
        if(id>=41&&id<=43)v=static_cast<std::int16_t>(v);
        frame->Values[id] = v;
        if (currentCombo) {
            currentCombo->Offset[id] = 0;
            currentCombo->Lower[id] = currentCombo->Upper[id] = v;
            currentCombo->HasHeld[id] = true;
        }
    }
    int Add(int id, int delta) {
        id = Control(id);
        int low, high;
        Bounds(id, low, high);
        const int value = Bounded(static_cast<I64>(frame->Values[id]) + delta, low, high);
        frame->Values[id] = value;
        if (currentCombo) {
            auto &c = *currentCombo;
            if (!c.HasHeld[id]) {
                c.Offset[id] = delta;
                c.Lower[id] = low;
                c.Upper[id] = high;
                c.HasHeld[id] = true;
            } else {
                c.Offset[id] += delta;
                c.Lower[id] = Bounded(static_cast<I64>(c.Lower[id]) + delta, low, high);
                c.Upper[id] = Bounded(static_cast<I64>(c.Upper[id]) + delta, low, high);
            }
        }
        return value;
    }
    void Push(VmState &state, int value) {
        if (state.Sp >= 1024)
            Error("Expression stack exceeds 1,024 values.");
        if(program->IntegerBits==16)value=static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
        state.Stack[state.Sp++] = value;
    }
    int Pop(VmState &state) {
        if (state.Sp <= 0)
            Error("Invalid compiled expression stack.");
        return state.Stack[--state.Sp];
    }
    int Invoke(int id, const int *args, int count) {
        if (!currentCombo && detail::UsesTime(id))
            timeAware = true;
        int a = count > 0 ? args[0] : 0, b = count > 1 ? args[1] : 0, c = count > 2 ? args[2] : 0;
        switch (id) {
        case 0:
            a = Control(a);
            if(a>=21&&a<=37&&!frame->Available[a])Error("Requested sensor/touch/paddle input is unavailable from this controller mapping.");
            return frame->Available[a] ? frame->Values[a] : Neutral(a);
        case 1:
            a = Control(a);
            if(a>=21&&a<=37&&!frame->Available[a])Error("Requested physical sensor/touch/paddle input is unavailable from this controller mapping.");
            return frame->Available[a] ? frame->Physical[a] : Neutral(a);
        case 2:
            a = Control(a);
            return initialized && previousAvailable[a] ? previous[a] : Neutral(a);
        case 3:
            Set(a, b);
            return frame->Values[a];
        case 4:
            return pressed[Control(a)];
        case 5:
            return released[Control(a)];
        case 6:
            a = Control(a);
            return frame->Available[a] ? std::min(32767,Milliseconds(now - changedAt[a])) : 0;
        case 7:
            return elapsed;
        case 8:
            a = Control(a);
            if (b < 0 || b > 86400000)
                Error("block duration must be between 0 and 86,400,000 ms.");
            blockedUntil[a] = b ? changedAt[a]+static_cast<I64>(b)*1000 : now;
            if (blockedUntil[a]>now)
                Set(a, Neutral(a));
            return 0;
        case 9:
            a = Control(a);
            b = Control(b);
            if (frame->Available[a] && frame->Available[b]) {
                const int old = frame->Values[a];
                Set(a, frame->Values[b]);
                Set(b, old);
            }
            return 0;
        case 10:
            return SafeAbs(a);
        case 11:
            return std::min(a, b);
        case 12:
            return std::max(a, b);
        case 13:
            if (b > c)
                Error("clamp minimum must not exceed maximum.");
            return std::clamp(a, b, c);
        case 14: {
            a=Control(a);b=Control(b);const int d=args[3];
            if(c==101){const double x=frame->Values[a],y=frame->Values[b],radius=std::hypot(x,y);if(radius>0){Set(a,static_cast<int>(x+x*d/radius));Set(b,static_cast<int>(y+y*d/radius));}}
            else {const auto bump=[](int x,int delta){return x==0?0:x<0?x-delta:x+delta;};Set(a,bump(frame->Values[a],c));Set(b,bump(frame->Values[b],d));}
            return 0;
        }
        case 15: {
            a = Control(a);
            const int percentage = c;
            if (percentage < 0 || percentage > 1000)
                Error("sensitivity percentage must be in 0..1000.");
            const int midpoint=b== -1?50:b;
            if(midpoint<1||midpoint>99)Error("sensitivity midpoint must be 1..99 or NOT_USE.");
            const int x=frame->Values[a],m=SafeAbs(x);
            const I64 shaped=m<=midpoint?static_cast<I64>(m)*50/midpoint:50+static_cast<I64>(m-midpoint)*50/(100-midpoint);
            Set(a,(x<0?-1:1)*shaped*percentage/100);
            return 0;
        }
        case 16:
        case 29: {
            if (a > b)
                Error("rand minimum must not exceed maximum.");
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            const auto width = static_cast<std::uint64_t>(static_cast<I64>(b) - a) + 1;
            return static_cast<int>(a + static_cast<I64>(randomState % width));
        }
        case 17:
        case 19: {
            const int index = Combo(a);
            auto &combo = *combos[index];
            if (id == 19 || !combo.Running) {
                combo.Clear();
                combo.Running = true;
                combo.Vm.Start(program->ComboBlocks[index],
                               program->Blocks[program->ComboBlocks[index]]);
                combo.Deadline = now;
            }
            return 0;
        }
        case 18:
            a = Combo(a);
            if (currentCombo && a < currentComboIndex && combos[a]->Running)
                Schedule(Deadline(1));
            combos[a]->Clear();
            return 0;
        case 20:
            return combos[Combo(a)]->Running;
        case 21:
            for (std::size_t i = 0; i < combos.size(); ++i) {
                if (currentCombo && static_cast<int>(i) < currentComboIndex && combos[i]->Running)
                    Schedule(Deadline(1));
                combos[i]->Clear();
            }
            return 0;
        case 22:
            a = Control(a);
            b = Control(b);
            if (a != b && frame->Available[a] && frame->Available[b]) {
                if (Active(a, frame->Physical[a], true)) {
                    const bool unsignedTarget = (b >= 4 && b < 96) || (b >= 160 && b < 192);
                    Set(b, unsignedTarget ? std::max(frame->Values[b], frame->Physical[a])
                                          : frame->Physical[a]);
                }
                Set(a, Neutral(a));
            }
            return 0;
        case 23:
            a = Control(a);
            Set(a, -static_cast<I64>(frame->Values[a]));
            return 0;
        case 24:
            return frame->Available[Control(a)];
        case 25:
            return frame->Family==2?2:frame->Family==8?5:4;
        case 26:
            return detail::Wrapped(frame->DeviceKey);
        case 27:
            return Milliseconds(now - startTime);
        case 28:
            if (a < -9 || a > 30)
                Error("vm_tctrl offset must be in -9..30 ms.");
            tickMilliseconds = 10+a;
            forcedTicks = true;
            return 0;
        case 30:
            a = Control(a);
            return !frame->Available[a]                  ? 0
                   : Active(a, frame->Physical[a], true) ? savedReleaseTime[a]
                                                         : Milliseconds(now - lastReleasedAt[a]);
        case 31: {
            if (static_cast<unsigned>(a) >= 80)
                Error("Persistent variable slot must be in 0..79.");
            if (b > c)
                Error("get_pvar minimum must not exceed maximum.");
            const int fallback = args[3];
            if (fallback < b || fallback > c)
                Error("get_pvar default must be within its bounds.");
            {const auto& memory=a>=64?program->GlobalMemory:program->PrivateMemory;const int value=memory?memory->Read(a>=64?a-64:a,fallback):hasPvar[a]?pvars[a]:fallback;return value>=b&&value<=c?value:fallback;}
        }
        case 32:
            if (static_cast<unsigned>(a) >= 80)
                Error("Persistent variable slot must be in 0..79.");
            pvars[a] = b;
            hasPvar[a] = true;
            {const auto& memory=a>=64?program->GlobalMemory:program->PrivateMemory;if(memory)memory->Write(a>=64?a-64:a,b);}
            return b;
        case 33:
            if (static_cast<unsigned>(b) >= 32)
                Error("Bit index must be in 0..31.");
            return static_cast<int>((static_cast<std::uint32_t>(a) >> b) & 1);
        case 34: {
            if (static_cast<unsigned>(b) >= 32 || c < 0 || c > 1)
                Error("set_bit needs a bit index in 0..31 and value 0 or 1.");
            const auto bit = std::uint32_t{1} << b;
            return detail::Wrapped(c ? (static_cast<std::uint32_t>(a) | bit)
                                     : (static_cast<std::uint32_t>(a) & ~bit));
        }
        case 35: {
            if (b < 0 || b > 31)
                Error("pow exponent must be in 0..31.");
            int power = 1;
            while (b-- > 0)
                power = detail::Mul(power, a);
            return power;
        }
        case 36:
            if (a < 0)
                Error("isqrt argument must be nonnegative.");
            return static_cast<int>(std::sqrt(a));
        case 37:
            return static_cast<int>(std::min<double>(
                INT_MAX, std::sqrt(static_cast<double>(a) * a + static_cast<double>(b) * b)));
        case 38:
            return Add(a, b);
        case 39:
        case 40:
            return Trig(a, count == 2 ? b : 100, id == 40);
        case 41:
            return Rounded(std::atan2(a, b) * (180 / 3.14159265358979323846));
        case 42:
            if (c < 0 || c > 100)
                Error("lerp percentage must be in 0..100.");
            return static_cast<int>(a + (static_cast<I64>(b) - a) * c / 100);
        case 43:
        case 44: {
            if (a < 1 || a > 86400000)
                Error("Wave period must be in 1..86,400,000 ms.");
            const I64 period = static_cast<I64>(a) * 1000;
            return Trig(static_cast<double>((now - startTime) % period) *
                                (360.0 / static_cast<double>(period)) +
                            (count == 3 ? c % 360 : 0),
                        b, id == 44);
        }
        case 45:
            if (a < 1 || a > 20)
                Error("tick interval must be in 1..20 ms.");
            if (!activeTickMilliseconds || a < activeTickMilliseconds)
                activeTickMilliseconds = a;
            return 0;
        case 46:
            if (a < 1 || a > 86400000)
                Error("Phase period must be in 1..86,400,000 ms.");
            return static_cast<int>((now - startTime) / 1000 % a);
        case 47:
            a = Control(a);
            if (b < 1 || b > 86400000)
                Error("Double-tap window must be in 1..86,400,000 ms.");
            return frame->Available[a] && pressed[a] && priorPress[a] &&
                   betweenPresses[a] <= static_cast<I64>(b) * 1000;
        case 48:return detail::Sub(0,a);
        case 49:{a=Control(a);b=Control(b);if(c<1||c>200)Error("stickize radius must be 1..200.");const double length=std::hypot(frame->Values[a],frame->Values[b]);if(length>c){Set(a,static_cast<int>(frame->Values[a]*c/length));Set(b,static_cast<int>(frame->Values[b]*c/length));}return 0;}
        case 50:{if(a!=2&&a!=4)Error("set_polar requires POLAR_RS or POLAR_LS.");const int x=a==2?9:11,y=x+1;const double radians=(b%360)*3.14159265358979323846/180;Set(x,std::lround(std::cos(radians)*c*100/32767));Set(y,std::lround(-std::sin(radians)*c*100/32767));return 0;}
        case 51:case 52:{if(a!=2&&a!=4||b<0||b>1)Error("Invalid polar identifier.");const auto& v=id==52?frame->Physical:frame->Values;const int x=a==2?9:11;return b?static_cast<int>((std::lround(std::atan2(-v[x+1],v[x])*180/3.14159265358979323846)+360)%360):std::min(32767,static_cast<int>(std::hypot(v[x],v[x+1])*32767/100));}
        case 53:case 54:case 55:case 56:case 57:case 58:case 59:case 60:case 61:{
            const unsigned bytes=id==55||id==58||id==59?2:id==60||id==61?4:1;
            if(a<0||static_cast<size_t>(a)>program->Data.size()||program->Data.size()-static_cast<size_t>(a)<bytes)Error("Data read outside the data section.");
            std::uint32_t value=0;for(unsigned i=0;i<bytes;++i)value=(value<<8)|program->Data[static_cast<size_t>(a)+i];
            if(id==54||id==57)return static_cast<std::int8_t>(value);if(id==59)return static_cast<std::int16_t>(value);return detail::Wrapped(value);
        }
        case 62:if(b<0||b>31)Error("Bit index must be 0..31.");return detail::Wrapped(static_cast<std::uint32_t>(a)&~(1u<<b));
        case 63:if(b<0||b>31)Error("Bit index must be 0..31.");return (static_cast<std::uint32_t>(a)>>b)&1;
        case 64:case 65:{const int shift=id==65?c:b;const auto mask=static_cast<std::uint32_t>(id==65?args[3]:c);if(shift<0||shift>=program->IntegerBits)Error("Bit index exceeds selected integer width.");if(id==65)return detail::Wrapped((static_cast<std::uint32_t>(a)&~(mask<<shift))|((static_cast<std::uint32_t>(b)&mask)<<shift));return detail::Wrapped((static_cast<std::uint32_t>(a)>>shift)&mask);}
        case 66:case 68:{auto& combo=*combos[Combo(a)];if(id==66&&combo.Running&&!combo.Suspended){combo.PauseLeft=std::max<I64>(0,combo.Deadline-now);combo.Suspended=true;}else if(id==68&&combo.Suspended){combo.Deadline=now+combo.PauseLeft;combo.Suspended=false;}return 0;}
        case 67:return combos[Combo(a)]->Suspended;
        case 69:case 70:for(size_t i=0;i<combos.size();++i){int p=static_cast<int>(i);Invoke(id==69?66:68,&p,1);}return 0;
        case 71:return combos[Combo(a)]->Step;
        case 72:{const auto& combo=*combos[Combo(a)];return Milliseconds(combo.Suspended?combo.PauseLeft:std::max<I64>(0,combo.Deadline-now));}
        case 73:return frame->Family==2?2:frame->Family==8?5:4;
        case 74:Error("Physical battery telemetry is unavailable in the Interceptor input API.");
        case 75:for(int i=0;i<30;++i)Set(i,0);return 0;
        case 76:case 79:if(a<0||a>3)Error("LED must be 0..3.");leds[a]=b;return 0;
        case 77:case 80:if(a<0||a>3)Error("LED must be 0..3.");return leds[a];
        case 78:leds.fill(0);return 0;
        case 81:if(a<0||a>3)Error("Rumble must be 0..3.");rumble[a]=std::clamp(b,0,100);return 0;
        case 82:case 83:rumble.fill(0);return 0;
        case 84:rgb={std::clamp(a,0,255),std::clamp(b,0,255),std::clamp(c,0,255)};return 0;
        case 85:{const double hue=((a%360+360)%360)/60.0,s=std::clamp(b,0,100)/100.0,v=std::clamp(c,0,100)/100.0,chroma=v*s,x=chroma*(1-std::abs(std::fmod(hue,2)-1)),m=v-chroma;double r=0,g=0,blue=0;switch(static_cast<int>(hue)){case 0:r=chroma;g=x;break;case 1:r=x;g=chroma;break;case 2:g=chroma;blue=x;break;case 3:g=x;blue=chroma;break;case 4:r=x;blue=chroma;break;default:r=chroma;blue=x;break;}rgb={Rounded((r+m)*255),Rounded((g+m)*255),Rounded((blue+m)*255)};return 0;}
        default:
            Error("Unknown compiled builtin.");
        }
    }
    Outcome Run(VmState &state) {
        const auto generation = currentCombo ? currentCombo->Generation : 0;
        while (true) {
            if (--remaining < 0)
                Error("Script instruction budget exceeded. Check loops and recursion; output was "
                      "restored to "
                      "physical input.");
            const auto &block = program->Blocks[state.Block];
            if (static_cast<unsigned>(state.Pc) >= block.Code.size())
                Error("Invalid compiled instruction address.");
            currentInstruction = block.Code[state.Pc++];
            const auto in = currentInstruction;
            switch (in.Code) {
            case Op::Push:
                Push(state, in.A);
                break;
            case Op::Narrow:{const auto value=static_cast<std::uint32_t>(Pop(state));int narrowed=detail::Wrapped(value);if(in.A==8)narrowed=in.B?static_cast<std::uint8_t>(value):static_cast<std::int8_t>(value);else if(in.A==16)narrowed=in.B?static_cast<std::uint16_t>(value):static_cast<std::int16_t>(value);Push(state,narrowed);break;}
            case Op::Index2:{int column=Pop(state),row=Pop(state);if(row<0||row>=in.A||column<0||column>=in.B)Error("Matrix index out of bounds.");Push(state,row*in.B+column);break;}
            case Op::Load:
                Push(state, globals[in.A]);
                break;
            case Op::LoadParameter:
                Push(state, parameters[in.A]);
                break;
            case Op::Store:
                globals[in.A] = state.Stack[state.Sp - 1];
                break;
            case Op::LoadLocal:
                Push(state, state.Locals[state.LocalBase + in.A]);
                break;
            case Op::StoreLocal:
                state.Locals[state.LocalBase + in.A] = state.Stack[state.Sp - 1];
                break;
            case Op::LoadArray: {
                const int index = Pop(state), length = in.B & 65535;
                if (static_cast<unsigned>(index) >= static_cast<unsigned>(length))
                    Error("Array index " + std::to_string(index) + " is outside 0.." +
                          std::to_string(length - 1) + ".");
                Push(state, (in.B & 65536) ? state.Locals[state.LocalBase + in.A + index]
                                           : globals[in.A + index]);
                break;
            }
            case Op::StoreArray: {
                const int value = Pop(state), index = Pop(state), length = in.B & 65535;
                if (static_cast<unsigned>(index) >= static_cast<unsigned>(length))
                    Error("Array index " + std::to_string(index) + " is outside 0.." +
                          std::to_string(length - 1) + ".");
                if (in.B & 65536)
                    state.Locals[state.LocalBase + in.A + index] = value;
                else
                    globals[in.A + index] = value;
                Push(state, value);
                break;
            }
            case Op::Dup: {
                const int value = state.Stack[state.Sp - 1];
                Push(state, value);
                break;
            }
            case Op::Dup2: {
                const int second = state.Stack[state.Sp - 2], first = state.Stack[state.Sp - 1];
                Push(state, second);
                Push(state, first);
                break;
            }
            case Op::Pop:
                (void)Pop(state);
                break;
            case Op::Neg:
                state.Stack[state.Sp - 1] = detail::Sub(0, state.Stack[state.Sp - 1]);
                break;
            case Op::Not:
                state.Stack[state.Sp - 1] = !state.Stack[state.Sp - 1];
                break;
            case Op::Bool:
                state.Stack[state.Sp - 1] = state.Stack[state.Sp - 1] != 0;
                break;
            case Op::BitNot:
                state.Stack[state.Sp - 1] = ~state.Stack[state.Sp - 1];
                break;
            case Op::Jump:
                state.Pc = in.A;
                break;
            case Op::JumpZero:
                if (!Pop(state))
                    state.Pc = in.A;
                break;
            case Op::Builtin: {
                const int start = state.Sp - in.B;
                if (start < 0)
                    Error("Invalid compiled argument stack.");
                const int result = Invoke(in.A, state.Stack.data() + start, in.B);
                if (currentCombo && currentCombo->Generation != generation)
                    return currentCombo->Running ? Outcome::Replaced : Outcome::Cancelled;
                state.Sp = start;
                Push(state, result);
                break;
            }
            case Op::ComboCall:
            case Op::Call: {
                if(in.Code==Op::ComboCall&&!currentCombo)Error("call requires an executing combo.");
                if(in.Code==Op::ComboCall)currentCombo->HasHeld.fill(false);
                if (state.Depth >= 32)
                    Error("Function call depth exceeds 32.");
                const auto &called = program->Blocks[in.A];
                const int start = state.Sp - in.B;
                if (state.LocalTop + called.Locals > static_cast<int>(state.Locals.size()))
                    Error("Function local storage exceeds the safe limit.");
                state.Calls[state.Depth++] = {state.Block, state.Pc, state.LocalBase, state.LocalTop,
                                              start};
                state.LocalBase = state.LocalTop;
                state.LocalTop += called.Locals;
                if (called.Locals)
                    std::fill_n(state.Locals.begin() + state.LocalBase, called.Locals, 0);
                if (in.B)
                    std::copy_n(state.Stack.begin() + start, in.B,
                                state.Locals.begin() + state.LocalBase);
                state.Sp = start;
                state.Block = in.A;
                state.Pc = 0;
                break;
            }
            case Op::Return: {
                const int result = Pop(state);
                if (!state.Depth)
                    return Outcome::Completed;
                const auto back = state.Calls[--state.Depth];
                state.Block = back.Block;
                state.Pc = back.Pc;
                state.LocalBase = back.LocalBase;
                state.LocalTop = back.LocalTop;
                state.Sp = back.Stack;
                Push(state, result);
                break;
            }
            case Op::Wait: {
                const int duration = Pop(state);
                if (duration < 0 || duration > 86400000)
                    Error("wait duration must be between 0 and 86,400,000 ms.");
                if (!currentCombo)
                    Error("wait is only valid directly inside a combo.");
                currentCombo->Deadline = Deadline(std::max(1, duration));
                ++currentCombo->Step;
                return Outcome::Waiting;
            }
            default: {
                const int b = Pop(state), a = Pop(state);
                Push(state, detail::Arithmetic(in.Code, a, b, detail::Token("", in.Line, in.Column)));
                break;
            }
            }
        }
    }
    void Reset() {
        if (busy.test_and_set(std::memory_order_acquire))
            throw std::logic_error("A script instance cannot be reset while executing.");
        std::fill(globals.begin(), globals.end(), 0);
        previous.fill(0);
        changedAt.fill(0);
        blockedUntil.fill(0);
        previousAvailable.fill(false);
        lastReleasedAt.fill(0);
        savedReleaseTime.fill(0);
        pvars.fill(0);
        hasPvar.fill(false);
        lastPressedAt.fill(0);
        betweenPresses.fill(0);
        hasPressed.fill(false);
        priorPress.fill(false);
        for (auto &combo : combos)
            combo->Clear();
        initialized = forcedTicks = timeAware = false;
        nextWake = activeTickMilliseconds = 0;
        previousTime = startTime = 0;
        nextCycle=0;lastOutput.fill(0);
        tickMilliseconds = 10;
        randomState = 0xA341316Cu;
        busy.clear(std::memory_order_release);
    }
    void Execute(ControlFrame &input, I64 timestamp, int interval) {
        if (timestamp < 0)
            throw std::invalid_argument("nowMicroseconds must be nonnegative.");
        if (busy.test_and_set(std::memory_order_acquire))
            throw std::logic_error("A script instance belongs to a single device worker.");
        if(initialized&&timestamp>=previousTime&&timestamp<nextCycle){input.Values=lastOutput;nextWake=static_cast<int>((nextCycle-timestamp+999)/1000);busy.clear(std::memory_order_release);return;}
        frame = &input;
        frame->CopyPhysicalToOutput();
        bool acceptedTime = false;
        const auto finish = [&] {
            if (acceptedTime) {
                previous = frame->Physical;
                previousAvailable = frame->Available;
                previousTime = timestamp;
            }
            currentCombo = nullptr;
            currentComboIndex = -1;
            frame = nullptr;
            busy.clear(std::memory_order_release);
        };
        try {
            if (initialized && timestamp < previousTime)
                throw ScriptException("Monotonic time moved backwards.");
            acceptedTime = true;
            now = timestamp;
            (void)interval;elapsed=initialized?Milliseconds(timestamp-previousTime):0;
            tickMilliseconds=10;
            remaining = budget.load(std::memory_order_relaxed);
            nextWake = activeTickMilliseconds = 0;
            timeAware = false;
            if (!initialized) {
                startTime = now;
                changedAt.fill(now);
                lastReleasedAt.fill(now);
            }
            for (int i = 0; i < 256; ++i) {
                const bool was = Active(i, previous[i], initialized && previousAvailable[i]),
                           active = Active(i, frame->Physical[i], frame->Available[i]);
                pressed[i] = !was && active;
                released[i] = was && !active;
                if (!frame->Available[i]) {
                    hasPressed[i] = priorPress[i] = false;
                    betweenPresses[i] = 0;
                }
                if (pressed[i]) {
                    priorPress[i] = hasPressed[i];
                    betweenPresses[i] = hasPressed[i] ? now - lastPressedAt[i] : 0;
                    lastPressedAt[i] = now;
                    hasPressed[i] = true;
                    savedReleaseTime[i] = Milliseconds(now - lastReleasedAt[i]);
                }
                if (released[i])
                    lastReleasedAt[i] = now;
                if (was != active)
                    changedAt[i] = now;
            }
            if (!initialized) {
                main->Start(program->Init, program->Blocks[program->Init]);
                currentCombo = nullptr;
                (void)Run(*main);
                initialized = true;
            }
            main->Start(program->Main, program->Blocks[program->Main]);
            currentCombo = nullptr;
            (void)Run(*main);
            for (int i = 0; i < 256; ++i)
                if (blockedUntil[i] > now)
                    frame->Values[i] = Neutral(i);
            for (std::size_t index = 0; index < combos.size(); ++index) {
                auto &combo = *combos[index];
                if (!combo.Running||combo.Suspended)
                    continue;
                bool ran = false;
                if (combo.Deadline <= now) {
                    comboBase = frame->Values;
                    Outcome outcome;
                    do {
                        combo.HasHeld.fill(false);
                        currentCombo = &combo;
                        currentComboIndex = static_cast<int>(index);
                        outcome = Run(combo.Vm);
                        currentCombo = nullptr;
                        currentComboIndex = -1;
                        if (outcome == Outcome::Cancelled || outcome == Outcome::Replaced)
                            frame->Values = comboBase;
                    } while (outcome == Outcome::Replaced);
                    if (outcome == Outcome::Completed)
                        combo.Clear();
                    ran = true;
                }
                if (combo.Running) {
                    if (!ran)
                        for (int i = 0; i < 256; ++i)
                            if (combo.HasHeld[i])
                                frame->Values[i] = Bounded(frame->Values[i] + combo.Offset[i],
                                                           combo.Lower[i], combo.Upper[i]);
                    Schedule(combo.Deadline);
                }
            }
            for (const auto &combo : combos)
                if (combo->Running&&!combo->Suspended)
                    Schedule(combo->Deadline);
            for (int i = 0; i < 256; ++i)
                if (blockedUntil[i] > now)
                    Schedule(blockedUntil[i]);
            const int cadence = activeTickMilliseconds ? activeTickMilliseconds : tickMilliseconds;
            nextWake=cadence;nextCycle=Deadline(cadence);lastOutput=frame->Values;
        } catch (...) {
            frame->CopyPhysicalToOutput();
            for (auto &combo : combos)
                combo->Clear();
            nextWake = 0;
            finish();
            throw;
        }
        finish();
    }
};
ScriptInstance::ScriptInstance(std::shared_ptr<const detail::ProgramData> program)
    : impl_(std::make_unique<Impl>(std::move(program))) {}
ScriptInstance::~ScriptInstance() = default;
std::unique_ptr<ScriptInstance> ScriptProgram::CreateInstance() const {
    return std::unique_ptr<ScriptInstance>(new ScriptInstance(data_));
}
void ScriptInstance::Execute(ControlFrame &frame, std::int64_t nowMicroseconds,
                             int elapsedMilliseconds) {
    ControlFrame gpc;gpc.DeviceKey=frame.DeviceKey;gpc.Family=frame.Family;
    const auto& slots=GpcSlots();
    for(size_t i=0;i<slots.size();++i)if(slots[i]>=0&&!(i>=38&&i<=43)){gpc.Available[i]=frame.Available[slots[i]];gpc.Physical[i]=frame.Physical[slots[i]];if(i>=44)gpc.Physical[i]=static_cast<int>(static_cast<long long>(gpc.Physical[i])*32767/100);}
    for(int i=38;i<=43;++i)gpc.Available[i]=true;
    impl_->Execute(gpc,nowMicroseconds,elapsedMilliseconds);
    const auto unremapped=gpc.Values;
    for(const auto& mapping:impl_->program->Remaps)gpc.Values[mapping.first]=0;
    for(const auto& mapping:impl_->program->Remaps)if(mapping.second>=0)gpc.Values[mapping.second]=unremapped[mapping.first];
    frame.CopyPhysicalToOutput();
    for(size_t i=0;i<slots.size();++i)if(slots[i]>=0&&gpc.Available[i]){
        if(i>=38&&i<=43)continue; // Traces are debug state, never physical HID VALUE fields.
        if(i>=44){if(gpc.Values[i]!=gpc.Physical[i])frame.Values[slots[i]]=static_cast<int>(static_cast<long long>(gpc.Values[i])*100/32767);}
        else frame.Values[slots[i]]=gpc.Values[i];
    }
}
void ScriptInstance::Reset() { impl_->Reset(); }
void ScriptInstance::SetParameters(const ParameterValues &values) {
    impl_->program->Interface.Validate(values);
    if (impl_->busy.test_and_set(std::memory_order_acquire))
        throw std::logic_error("Parameters must be updated between script executions.");
    impl_->parameters = values;
    impl_->busy.clear(std::memory_order_release);
}
int ScriptInstance::NextWakeMilliseconds() const noexcept { return impl_->nextWake; }
int ScriptInstance::InstructionBudget() const noexcept {
    return impl_->budget.load(std::memory_order_relaxed);
}
ScriptDebug ScriptInstance::Debug()const noexcept{ScriptDebug d;for(int i=0;i<6;++i)d.Traces[i]=impl_->lastOutput[38+i];d.Leds=impl_->leds;d.Rumble=impl_->rumble;d.Rgb=impl_->rgb;for(const auto& m:{impl_->program->PrivateMemory,impl_->program->GlobalMemory})if(m&&m->Error())d.PersistenceError=m->Error();return d;}
void ScriptInstance::SetInstructionBudget(int value) {
    if (value < 100 || value > 100000)
        throw std::out_of_range("Instruction budget must be in 100..100000.");
    impl_->budget.store(value, std::memory_order_relaxed);
}
} // namespace gpr
