// Appended after the pinned generated PPC bodies and their actual ABI helpers.
#include "lo_semantics/manager_allocate.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
constexpr std::uint64_t Space = std::uint64_t{1} << 32;
constexpr std::size_t LowSize = 0x80000;
constexpr GuestAddress PrimaryVtablePage = 0x82002000u;
constexpr GuestAddress FallbackVtablePage = 0x8201d000u;
constexpr GuestAddress GlobalPage = 0x83315000u;
constexpr GuestAddress SectionGlobal = 0x83315fd4u;
constexpr GuestAddress ManagerGlobal = 0x83315fd8u;
constexpr GuestAddress FailureCount = 0x83315fdcu;
constexpr GuestAddress StackTop = 0x78000u;
constexpr GuestAddress FallbackFrame = StackTop - 128u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress AlternateManager = 0x12000u;
constexpr GuestAddress PrimaryVtable = 0x82002700u;
constexpr GuestAddress FallbackVtable = 0x8201dd90u;
constexpr GuestAddress AlternateVtable = 0x71200u;
constexpr GuestAddress Section = 0x20000u;
constexpr GuestAddress AlternateSection = 0x20100u;
constexpr GuestAddress PrimaryMethod = ORACLE_PRIMARY_ALLOC_METHOD;
constexpr GuestAddress PrimaryWrapperMethod = ORACLE_PRIMARY_WRAPPER_METHOD;
constexpr GuestAddress FallbackMethod = ORACLE_FALLBACK_WRAPPER_METHOD;
constexpr GuestAddress AlternateMethod = 0x823f2020u;
constexpr std::uint64_t CallbackResult = 0x82abcdef00000000ull;
constexpr std::uint64_t LeaveResult = 0x1234567890abcdefull;
constexpr std::uint64_t SavedR29 = 0x1111222233334444ull;
constexpr std::uint64_t SavedR30 = 0x5555666677778888ull;
constexpr std::uint64_t SavedR31 = 0x9999aaaabbbbccccull;
constexpr std::uint32_t CallerLR = 0x81234567u;

void Require(bool valid, const char* reason)
{
    if (!valid) throw std::runtime_error(reason);
}

struct GuestImage
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    GuestImage()
    {
        Require(bytes != nullptr, "reserve guest address space");
        Require(VirtualAlloc(bytes, LowSize, MEM_COMMIT, PAGE_READWRITE) == bytes &&
                VirtualAlloc(bytes + PrimaryVtablePage, 0x1000, MEM_COMMIT,
                             PAGE_READWRITE) == bytes + PrimaryVtablePage &&
                VirtualAlloc(bytes + FallbackVtablePage, 0x1000, MEM_COMMIT,
                             PAGE_READWRITE) == bytes + FallbackVtablePage &&
                VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT,
                             PAGE_READWRITE) == bytes + GlobalPage,
                "commit ordinary guest windows");
    }
    ~GuestImage() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    GuestImage(const GuestImage&) = delete;
    GuestImage& operator=(const GuestImage&) = delete;
};

enum class Kind { Primary, Fallback };
struct Case
{
    Kind kind;
    std::uint64_t receiver = Object;
    std::uint64_t arg4 = 0xfeedface12345678ull;
    std::uint64_t arg5 = 0x11223344deadbeefull;
    unsigned failures = 0;
    bool change_flags = false;
    bool change_globals = false;
    bool change_holder = false;
    bool zero_low_result = false;
    bool composed = false;
    bool alias_vtable_slot = false;
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    std::array<std::uint32_t, 8> before;
    std::array<std::uint32_t, 8> after;
    bool operator==(const Event&) const = default;
};

struct Run
{
    std::uint8_t* bytes;
    const Case& scenario;
    std::vector<Event> events;
    unsigned attempts = 0;
    unsigned virtual_calls = 0;
    unsigned leaves = 0;
    unsigned composed_calls = 0;

    std::uint32_t Read32(GuestAddress address) const
    {
        return (std::uint32_t(bytes[address]) << 24) |
               (std::uint32_t(bytes[address + 1]) << 16) |
               (std::uint32_t(bytes[address + 2]) << 8) | bytes[address + 3];
    }
    void Write32(GuestAddress address, std::uint32_t value)
    {
        bytes[address] = std::uint8_t(value >> 24);
        bytes[address + 1] = std::uint8_t(value >> 16);
        bytes[address + 2] = std::uint8_t(value >> 8);
        bytes[address + 3] = std::uint8_t(value);
    }
    std::array<std::uint32_t, 8> Snapshot() const
    {
        return {Read32(Object + 0x48de4u), Read32(Object + 0x48de8u),
                Read32(Object), Read32(FallbackFrame + 80u),
                Read32(SectionGlobal), Read32(ManagerGlobal),
                Read32(FailureCount), Read32(0x30000u)};
    }
    void Begin(char kind, std::array<std::uint64_t, 4> args)
    {
        events.push_back({kind, args, Snapshot(), {}});
    }
    void End() { events.back().after = Snapshot(); }

    std::uint64_t Try(std::uint64_t section_register)
    {
        Begin('T', {section_register});
        Require(section_register == std::uint64_t{Section} + 4u,
                "Try uses captured section address across retries");
        const bool fail = attempts++ < scenario.failures;
        if (fail)
        {
            if (scenario.change_globals)
            {
                Write32(SectionGlobal, AlternateSection);
                Write32(ManagerGlobal, AlternateManager);
            }
            if (scenario.change_holder)
                Write32(FallbackFrame + 80u, AlternateSection);
            Write32(0x30000u, Read32(0x30000u) + 1u);
        }
        End();
        return fail ? 0xfeedbeef00000000ull : 0x1122334400000001ull;
    }
    std::uint64_t Allocate(GuestAddress method, std::uint64_t receiver,
                           std::uint64_t arg4, std::uint64_t arg5)
    {
        Begin('V', {method, receiver, arg4, arg5});
        ++virtual_calls;
        const bool primary = scenario.kind == Kind::Primary || scenario.composed;
        const GuestAddress expected_manager = scenario.change_globals &&
            scenario.failures ? AlternateManager : Object;
        Require(method == (primary ? PrimaryMethod :
            expected_manager == AlternateManager ? AlternateMethod :
            FallbackMethod), "virtual target and low tag bits");
        Require(receiver == (scenario.kind == Kind::Primary
                ? scenario.receiver : expected_manager),
                "virtual receiver register");
        Require(arg4 == scenario.arg4 && arg5 == (primary ? 8u : scenario.arg5),
                "virtual argument registers");
        if (primary && scenario.change_flags)
        {
            Write32(Object + 0x48de4u, 0xabcdef01u);
            Write32(Object + 0x48de8u, 0xabcdef02u);
            Write32(Object, AlternateVtable);
        }
        if (!primary && scenario.change_holder)
            Write32(FallbackFrame + 80u, AlternateSection);
        Write32(0x30000u, Read32(0x30000u) ^ 0x12345678u);
        End();
        return scenario.zero_low_result ? CallbackResult :
            CallbackResult | 0x76543210u;
    }
    std::uint64_t Leave(std::uint64_t section_register)
    {
        Begin('L', {section_register});
        ++leaves;
        const GuestAddress expected = scenario.change_holder
            ? AlternateSection : Section;
        Require(section_register == std::uint64_t{expected} + 4u,
                "Leave reloads holder after virtual callback");
        Write32(0x30000u, Read32(0x30000u) + 3u);
        End();
        return LeaveResult;
    }
};

Run* active = nullptr;
struct Services final : ManagerAllocateServices
{
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint64_t TryEnterCriticalSection(std::uint64_t value) override
    { return run.Try(value); }
    std::uint64_t AllocateVirtual(GuestAddress method,
        std::uint64_t receiver, std::uint64_t arg4,
        std::uint64_t arg5) override
    {
        if (run.scenario.composed && method == PrimaryWrapperMethod)
        {
            ++run.composed_calls;
            GuestMemory memory(0, {run.bytes, static_cast<std::size_t>(Space)});
            return AllocateThroughPrimaryManager(memory, *this,
                                                 receiver, arg4, arg5);
        }
        return run.Allocate(method, receiver, arg4, arg5);
    }
    std::uint64_t LeaveCriticalSection(std::uint64_t value) override
    { return run.Leave(value); }
};

void Compare(const Case& scenario)
{
    GuestImage original_image, recovered_image;
    for (std::size_t index = 0; index < LowSize; ++index)
        original_image.bytes[index] = std::uint8_t(index * 37u + 11u);
    for (GuestAddress page : {PrimaryVtablePage, FallbackVtablePage,
                              GlobalPage})
        for (std::size_t index = 0; index < 0x1000; ++index)
            original_image.bytes[page + index] =
                std::uint8_t(index * 23u + 7u);
    Run original{original_image.bytes, scenario};
    original.Write32(Object, PrimaryVtable);
    original.Write32(AlternateManager, AlternateVtable);
    const GuestAddress tag = scenario.composed ? 0u :
        (scenario.kind == Kind::Primary ? scenario.change_flags :
         scenario.change_holder) ? 3u : 0u;
    original.Write32(PrimaryVtable + 4u, PrimaryMethod | tag);
    original.Write32(PrimaryVtable + 16u, PrimaryWrapperMethod | tag);
    original.Write32(FallbackVtable + 16u, FallbackMethod | tag);
    if (scenario.alias_vtable_slot)
    {
        original.Write32(Object, Object + 0x48de0u);
        original.Write32(Object + 0x48de4u, PrimaryMethod | tag);
    }
    original.Write32(AlternateVtable + 16u, AlternateMethod | 3u);
    original.Write32(SectionGlobal, Section);
    original.Write32(ManagerGlobal, Object);
    original.Write32(FailureCount, 0xfffffffeu);
    if (scenario.kind == Kind::Fallback && !scenario.composed)
        original.Write32(Object, FallbackVtable);
    std::memcpy(recovered_image.bytes, original_image.bytes, LowSize);
    for (GuestAddress page : {PrimaryVtablePage, FallbackVtablePage,
                              GlobalPage})
        std::memcpy(recovered_image.bytes + page,
                    original_image.bytes + page, 0x1000);
    Run recovered{recovered_image.bytes, scenario};

    PPCContext context{};
    context.r1.u64 = StackTop;
    context.lr = CallerLR;
    context.r29.u64 = SavedR29;
    context.r30.u64 = SavedR30;
    context.r31.u64 = SavedR31;
    context.r3.u64 = scenario.receiver;
    context.r4.u64 = scenario.arg4;
    context.r5.u64 = scenario.arg5;
    active = &original;
    if (scenario.kind == Kind::Primary)
        oracle_AllocatePrimary(context, original_image.bytes);
    else
        oracle_AllocateFallback(context, original_image.bytes);
    active = nullptr;
    Require(context.r1.u64 == StackTop && context.lr == CallerLR &&
            context.r29.u64 == SavedR29 && context.r30.u64 == SavedR30 &&
            context.r31.u64 == SavedR31,
            "original PPC restores nonvolatile registers and stack");

    GuestMemory memory(0, {recovered_image.bytes,
                            static_cast<std::size_t>(Space)});
    Services services(recovered);
    if (scenario.kind == Kind::Fallback)
        recovered.Write32(FallbackFrame + 80u, Section);
    const std::uint64_t result = scenario.kind == Kind::Primary
        ? AllocateThroughPrimaryManager(memory, services, scenario.receiver,
                                        scenario.arg4, scenario.arg5)
        : AllocateThroughFallbackManager(memory, services, FallbackFrame,
                                         scenario.arg4, scenario.arg5);
    Require(result == context.r3.u64, "full PPC r3 return");
    Require(original.events == recovered.events, "callback order/args/owned memory");
    Require(original.attempts == recovered.attempts &&
            original.virtual_calls == recovered.virtual_calls &&
            original.leaves == recovered.leaves &&
            original.composed_calls == recovered.composed_calls &&
            original.composed_calls == (scenario.composed ? 1u : 0u) &&
            original.virtual_calls == 1 &&
            original.attempts == (scenario.kind == Kind::Fallback
                ? scenario.failures + 1u : 0u) &&
            original.leaves == (scenario.kind == Kind::Fallback ? 1u : 0u),
            "callback counts");
    for (std::size_t index = 0; index < LowSize; ++index)
    {
        if (index >= FallbackFrame - 160u && index < StackTop + 64u)
            continue; // Generic PPC ABI saves and backchain belong to an adapter.
        if (original_image.bytes[index] != recovered_image.bytes[index])
        {
            std::fprintf(stderr, "low byte %zx: %02x/%02x\n", index,
                         original_image.bytes[index], recovered_image.bytes[index]);
            throw std::runtime_error("ordinary guest memory mismatch");
        }
    }
    for (GuestAddress page : {PrimaryVtablePage, FallbackVtablePage,
                              GlobalPage})
        Require(std::memcmp(original_image.bytes + page,
                            recovered_image.bytes + page, 0x1000) == 0,
                "high guest page mismatch");
    if (scenario.kind == Kind::Fallback)
        Require(original.Read32(FallbackFrame + 80u) ==
                    recovered.Read32(FallbackFrame + 80u),
                "live holder word mismatch");
}
} // namespace

PPC_FUNC(sub_822958F8) { oracle_StoreLockAndWaitForEnter(ctx, base); }
PPC_FUNC(__imp__RtlTryEnterCriticalSection)
{ ctx.r3.u64 = active->Try(ctx.r3.u64); }
PPC_FUNC(__imp__RtlLeaveCriticalSection)
{ ctx.r3.u64 = active->Leave(ctx.r3.u64); }
void oracle_Indirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method)
{
    if (active->scenario.composed && method == PrimaryWrapperMethod)
    {
        ++active->composed_calls;
        oracle_AllocatePrimary(ctx, active->bytes);
    }
    else
        ctx.r3.u64 = active->Allocate(method, ctx.r3.u64,
                                      ctx.r4.u64, ctx.r5.u64);
}

int main()
{
    try
    {
        unsigned primary = 0, fallback = 0, composed = 0;
        for (unsigned high = 0; high < 4; ++high)
            for (bool mutate : {false, true})
                for (bool zero_low : {false, true})
                {
                    Case test{Kind::Primary};
                    test.receiver = (std::uint64_t{high} << 32) | Object;
                    test.arg4 ^= std::uint64_t{high} << 32;
                    test.arg5 ^= high * 0x10203u;
                    test.change_flags = mutate;
                    test.zero_low_result = zero_low;
                    Compare(test);
                    ++primary;
                }
        for (unsigned high = 0; high < 2; ++high)
            for (bool mutate : {false, true})
                for (bool zero_low : {false, true})
                {
                    Case test{Kind::Primary};
                    test.receiver = (std::uint64_t{high} << 32) | Object;
                    test.alias_vtable_slot = true;
                    test.change_flags = mutate;
                    test.zero_low_result = zero_low;
                    Compare(test);
                    ++primary;
                }
        for (unsigned failures = 0; failures < 3; ++failures)
            for (bool globals : {false, true})
                for (bool holder : {false, true})
                    for (bool zero_low : {false, true})
                    {
                        Case test{Kind::Fallback};
                        test.failures = failures;
                        test.change_globals = globals;
                        test.change_holder = holder;
                        test.zero_low_result = zero_low;
                        test.arg4 ^= failures * 0x100000001ull;
                        test.arg5 ^= std::uint64_t{globals} << 60;
                        Compare(test);
                        ++fallback;
                    }
        for (unsigned failures = 0; failures < 3; ++failures)
            for (bool mutate_flags : {false, true})
                for (bool zero_low : {false, true})
                {
                    Case test{Kind::Fallback};
                    test.composed = true;
                    test.failures = failures;
                    test.change_flags = mutate_flags;
                    test.zero_low_result = zero_low;
                    test.change_holder = failures != 0;
                    Compare(test);
                    ++composed;
                }
        std::printf("PASS 823F2670 %u\nPASS 823F25E0 %u\n",
                    primary, fallback);
        std::printf("PASS manager-allocate-composed %u\n", composed);
        std::puts("LIMIT: pinned generated PPC C++; synthetic virtual/critical-section calls; generic ABI scratch excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
