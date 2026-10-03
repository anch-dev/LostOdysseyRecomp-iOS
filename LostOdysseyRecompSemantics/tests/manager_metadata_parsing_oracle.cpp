#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/allocation_failure.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace parsing = lo::semantic::gpu::manager_metadata_parsing;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800060000ull;
constexpr std::uint64_t Environment = 0xaaaabbbb00050000ull;
constexpr std::uint64_t ChangedEnvironment = 0xccccdddd00053000ull;
constexpr std::uint64_t ThreadRecord = 0xeeeeffff00052000ull;
constexpr GuestAddress Source = 0x10000u;
constexpr GuestAddress Destination = 0x20000u;
constexpr GuestAddress Output = 0x24000u;
constexpr GuestAddress EndPointer = 0x25000u;
constexpr GuestAddress ClassTable = 0x40000u;

enum class Case { AsciiDigit, ArabicDigit, NonDigit, SpaceClass, Decimal,
    Negative, Hex, NoDigits, Unsigned, Overflow, Invalid, Suffix, NoSuffix };
struct Spec
{
    Case scenario;
    GuestAddress address;
    const char* text;
    std::uint64_t r3, r4, r5, r6, r7;
};
constexpr Spec Cases[] = {
    {Case::AsciiDigit, 0x82376fa8u, "", '7', 0, 0, 0, 0},
    {Case::ArabicDigit, 0x82376fa8u, "", 1635, 0, 0, 0, 0},
    {Case::NonDigit, 0x82376fa8u, "", 0x1234, 0, 0, 0, 0},
    {Case::SpaceClass, 0x822974b0u, "", ' ', 8, 0, 0, 0},
    {Case::Decimal, 0x82376f98u, " \t+123", Source, 0, 0, 0, 0},
    {Case::Negative, 0x82b7d688u, "-2147483648", Source, 0, 10, 0, 0},
    {Case::Hex, 0x82b7d3e0u, "0x2A!", 0xffffffff83215300ull,
        Source, EndPointer, 0, 0},
    {Case::NoDigits, 0x82b7d3e0u, "xyz", 0xffffffff83215300ull,
        Source, EndPointer, 10, 0},
    {Case::Unsigned, 0x82b7d3e0u, "4294967295", 0xffffffff83215300ull,
        Source, 0, 10, 1},
    {Case::Overflow, 0x82b7d3e0u, "2147483648", 0xffffffff83215300ull,
        Source, EndPointer, 10, 0},
    {Case::Invalid, 0x82b7d3e0u, "1", 0xffffffff83215300ull,
        Source, EndPointer, 1, 0},
    {Case::Suffix, 0x82296e80u, "Map_42", Source, Destination, 0, Output, 0},
    {Case::NoSuffix, 0x82296e80u, "Map42", Source, Destination, 0, Output, 0},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000u, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832d3000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83378000u, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve/commit guest memory");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};
struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    Case scenario;
    std::vector<Event> events;
    explicit Services(std::uint8_t* bytes, Case selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), scenario(selected) {}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    {
        events.push_back({'T', {index, 0, 0, 0}});
        return 0;
    }
    void SetTlsValue(std::uint32_t index, std::uint64_t value) override
    { events.push_back({'S', {index, value, 0, 0}}); }
    std::uint64_t CallThreadDataGetter(GuestAddress function,
        std::uint64_t context) override
    {
        events.push_back({'G', {function, context, 0, 0}});
        return ThreadRecord;
    }
    std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& call) override
    {
        events.push_back({'G', {function, context, call.thread_environment, 0}});
        if (scenario == Case::Invalid)
            call.thread_environment = ChangedEnvironment;
        return ThreadRecord;
    }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected CRT thread allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected CRT thread bind"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected CRT thread free"); }
    void CallHandler(GuestMemory& callback_memory, GuestAddress function,
        InvalidParameterCall& call) override
    {
        events.push_back({'H', {function, call.thread_environment,
            call.arguments[0], call.arguments[7]}});
        if (call.arguments[5] != 0x1111222233334444ull ||
            call.arguments[6] != 0x5555666677778888ull ||
            call.arguments[7] != 22u)
            throw std::runtime_error("invalid handler live r8/r9/r10 changed");
        callback_memory.WriteU32(static_cast<GuestAddress>(Stack) - 8u,
            0x87654321u);
        callback_memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u,
            0x12345678u);
        callback_memory.WriteU32(static_cast<GuestAddress>(Stack) - 12u,
            0x9abcdef0u);
        call.thread_environment = Environment;
    }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid parameter trap"); }
};
Services* active = nullptr;

class ErrorAdapter final : public AllocationFailureServices
{
public:
    ErrorAdapter(Services& services, PPCContext& context)
        : services_(services), context_(context) {}
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::runtime_error("unexpected error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::runtime_error("unexpected bugcheck"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected new handler"); }
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{context_.r13.u64};
        const auto record = GetCrtThreadData(services_.memory, services_, call);
        context_.r13.u64 = call.thread_environment;
        return record;
    }
private:
    Services& services_;
    PPCContext& context_;
};

void Seed(Window& window, const Spec& spec)
{
    std::memset(window.bytes, 0xbd, 0x70000);
    std::memset(window.bytes + ClassTable, 0, 0x200);
    std::memset(window.bytes + 0x83214000u, 0, 0x2000);
    std::memset(window.bytes + 0x832d3000u, 0, 0x1000);
    std::memset(window.bytes + 0x83378000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x83215b40u, ClassTable);
    for (std::uint16_t unit : {' ', '\t', '\n', '\r'})
        memory.WriteU16(ClassTable + 2u * unit, 8u);
    memory.WriteU32(0x83214d74u, 0x1234u);
    memory.WriteU32(0x83214d78u, 5u);
    memory.WriteU32(0x832d3adcu, 0x82345680u);
    memory.WriteU32(0x83378e80u, 0x82345690u);
    memory.WriteU32(static_cast<GuestAddress>(Environment) + 336u, 1u);
    memory.WriteU32(static_cast<GuestAddress>(ChangedEnvironment) + 336u, 1u);
    memory.WriteU32(EndPointer, 0xccccccccu);
    memory.WriteU32(Output, 0xccccccccu);
    for (std::size_t i = 0; spec.text[i] != '\0'; ++i)
        memory.WriteU16(Source + 2u * static_cast<GuestAddress>(i),
            static_cast<std::uint8_t>(spec.text[i]));
    memory.WriteU16(Source + 2u * static_cast<GuestAddress>(std::strlen(spec.text)), 0);
}

bool Compare(const Spec& spec, Window& original, Window& recovered)
{
    Seed(original, spec);
    Seed(recovered, spec);
    Services expected(original.bytes, spec.scenario),
        actual(recovered.bytes, spec.scenario);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0xaaaa555566667777ull;
    context.r13.u64 = Environment;
    for (unsigned reg = 23; reg <= 31; ++reg)
        (&context.r23)[reg - 23u].u64 =
            0x1234000000000000ull + reg;
    context.r3.u64 = spec.r3;
    context.r4.u64 = spec.r4;
    context.r5.u64 = spec.r5;
    context.r6.u64 = spec.r6;
    context.r7.u64 = spec.r7;
    context.r8.u64 = 0x1111222233334444ull;
    context.r9.u64 = 0x5555666677778888ull;
    active = &expected;
    switch (spec.address)
    {
    case 0x82296e80u: __imp__sub_82296E80(context, original.bytes); break;
    case 0x822974b0u: __imp__sub_822974B0(context, original.bytes); break;
    case 0x82376f98u: __imp__sub_82376F98(context, original.bytes); break;
    case 0x82376fa8u: __imp__sub_82376FA8(context, original.bytes); break;
    case 0x82b7d3e0u: __imp__sub_82B7D3E0(context, original.bytes); break;
    case 0x82b7d688u: __imp__sub_82B7D688(context, original.bytes); break;
    default: throw std::runtime_error("missing raw dispatch");
    }
    active = &actual;
    parsing::FrameRegisters frame{};
    frame.lr = 0xaaaa555566667777ull;
    frame.r13 = Environment;
    frame.r8 = 0x1111222233334444ull;
    frame.r9 = 0x5555666677778888ull;
    for (unsigned reg = 23; reg <= 31; ++reg)
        frame.r23_through_r31[reg - 23u] = 0x1234000000000000ull + reg;
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!parsing::Apply(spec.address, actual.memory, actual, actual,
            spec.r3, spec.r4, spec.r5, spec.r6, spec.r7, Stack, frame, result))
        throw std::runtime_error("missing semantic dispatch");
    std::uint32_t expected_low = 0;
    switch (spec.scenario)
    {
    case Case::AsciiDigit: expected_low = 7; break;
    case Case::ArabicDigit: expected_low = 3; break;
    case Case::NonDigit: expected_low = 0xffffffffu; break;
    case Case::SpaceClass: expected_low = 8; break;
    case Case::Decimal: expected_low = 123; break;
    case Case::Negative: expected_low = 0x80000000u; break;
    case Case::Hex: expected_low = 42; break;
    case Case::NoDigits: expected_low = 0; break;
    case Case::Unsigned: expected_low = 0xffffffffu; break;
    case Case::Overflow: expected_low = 0x7fffffffu; break;
    case Case::Invalid: expected_low = 0; break;
    case Case::Suffix: expected_low = 1; break;
    case Case::NoSuffix: expected_low = 0; break;
    }
    const bool scenario_effects =
        static_cast<GuestAddress>(result) == expected_low &&
        (spec.scenario != Case::Unsigned || result == 0x00000000ffffffffull) &&
        (spec.scenario != Case::Suffix ||
            (actual.memory.ReadU32(Output) == 42u &&
             actual.memory.ReadU16(Destination) == 'M' &&
             actual.memory.ReadU16(Destination + 6u) == 0)) &&
        (spec.scenario != Case::NoSuffix ||
            actual.memory.ReadU32(Output) == 0xccccccccu) &&
        (spec.scenario != Case::Hex ||
            actual.memory.ReadU32(EndPointer) == Source + 8u) &&
        (spec.scenario != Case::NoDigits ||
            actual.memory.ReadU32(EndPointer) == Source) &&
        (spec.scenario != Case::Overflow ||
            (actual.memory.ReadU32(static_cast<GuestAddress>(ThreadRecord) + 8u) == 34u &&
             actual.events.size() == 3u)) &&
        (spec.scenario != Case::Invalid ||
            (actual.memory.ReadU32(static_cast<GuestAddress>(ThreadRecord) + 8u) == 22u &&
             actual.events.size() == 4u &&
             actual.events.back().kind == 'H' &&
             actual.events.back().args[1] == ChangedEnvironment &&
             actual.events.back().args[3] == 22u &&
             frame.lr == 0x87654321u &&
             frame.r23_through_r31.back() == 0x123456789abcdef0ull));
    bool same = scenario_effects && context.r3.u64 == result && context.r1.u64 == Stack &&
        context.lr == frame.lr && context.r13.u64 == frame.r13 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x70000) == 0 &&
        std::memcmp(original.bytes + 0x83214000u,
            recovered.bytes + 0x83214000u, 0x2000) == 0 &&
        std::memcmp(original.bytes + 0x832d3000u,
            recovered.bytes + 0x832d3000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83378000u,
            recovered.bytes + 0x83378000u, 0x1000) == 0;
    for (unsigned reg = 23; reg <= 31; ++reg)
        same &= (&context.r23)[reg - 23u].u64 ==
            frame.r23_through_r31[reg - 23u];
    if (!same)
    {
        std::size_t first = 0;
        while (first < 0x70000 && original.bytes[first] == recovered.bytes[first])
            ++first;
        std::fprintf(stderr,
            "FAIL parsing %08x case %u r3 %llx/%llx r13 %llx/%llx lr %llx/%llx events %zu/%zu out %08x/%08x end %08x/%08x diff %zx:%02x/%02x\n",
            spec.address, static_cast<unsigned>(spec.scenario),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.r13.u64),
            static_cast<unsigned long long>(frame.r13),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size(),
            expected.memory.ReadU32(Output), actual.memory.ReadU32(Output),
            expected.memory.ReadU32(EndPointer), actual.memory.ReadU32(EndPointer),
            first, first < 0x70000 ? original.bytes[first] : 0,
            first < 0x70000 ? recovered.bytes[first] : 0);
    }
    return same;
}
} // namespace

void SaveGprs(PPCContext& context, std::uint8_t* base, unsigned first)
{
    for (unsigned reg = first; reg <= 31; ++reg)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - reg),
            (&context.r23)[reg - 23u].u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void RestoreGprs(PPCContext& context, std::uint8_t* base, unsigned first)
{
    for (unsigned reg = first; reg <= 31; ++reg)
        (&context.r23)[reg - 23u].u64 =
            PPC_LOAD_U64(context.r1.u32 - 8u * (33u - reg));
    context.lr = PPC_LOAD_U32(context.r1.u32 - 8u);
}

void OriginalDirectCall(PPCContext& context, std::uint8_t* base,
    GuestAddress target)
{
    switch (target)
    {
    case 0x82296830u:
        context.r3.u64 = registered_metadata_string::Utf16Length(
            active->memory, context.r3.u64);
        return;
    case 0x8232d318u:
    {
        const auto copy = manager_object_registration::CopyUtf16Padded(
            active->memory, context.r3.u64, context.r4.u64, context.r5.u64);
        context.r3.u64 = copy.r3;
        context.r4.u64 = copy.r4;
        context.r5.u64 = copy.r5;
        return;
    }
    case 0x822974b0u: __imp__sub_822974B0(context, base); return;
    case 0x82376fa8u: __imp__sub_82376FA8(context, base); return;
    case 0x82376f98u: __imp__sub_82376F98(context, base); return;
    case 0x82b7d688u: __imp__sub_82B7D688(context, base); return;
    case 0x82b7d3e0u: __imp__sub_82B7D3E0(context, base); return;
    case 0x82b7fd78u:
    {
        ErrorAdapter adapter(*active, context);
        context.r3.u64 = GetAllocationErrorAddress(adapter);
        return;
    }
    case 0x82b7fec0u:
    {
        InvalidParameterCall call{};
        std::uint64_t* const arguments[] = {&context.r3.u64, &context.r4.u64,
            &context.r5.u64, &context.r6.u64, &context.r7.u64,
            &context.r8.u64, &context.r9.u64, &context.r10.u64};
        for (unsigned i = 0; i < 8; ++i) call.arguments[i] = *arguments[i];
        call.thread_environment = context.r13.u64;
        context.r3.u64 = ReportInvalidParameter(active->memory, *active, call);
        context.r13.u64 = call.thread_environment;
        return;
    }
    default: throw std::runtime_error("unexpected raw direct target");
    }
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned comparisons = 0;
        for (const Spec& spec : Cases)
        {
            if (!Compare(spec, original, recovered)) return 1;
            ++comparisons;
        }
        Services service(recovered.bytes, Case::Decimal);
        parsing::FrameRegisters frame{};
        frame.lr = 9;
        std::uint64_t result = 7;
        if (parsing::Apply(0x12345678u, service.memory, service, service,
                1, 2, 3, 4, 5, Stack, frame, result) || result != 7 ||
            frame.lr != 9 || !service.events.empty()) return 1;
        std::printf("PASS manager-metadata-parsing 6 entries %u PPC comparisons + unknown\n",
            comparisons);
        std::puts("LIMIT reused CRT thread, errno, invalid-parameter, UTF16 length/copy lower models; external TLS/handler and generic volatile ABI excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
