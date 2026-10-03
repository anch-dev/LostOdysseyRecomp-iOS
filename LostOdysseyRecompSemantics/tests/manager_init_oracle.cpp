// Appended after the independently extracted 827C5F38 PPC body.
#include "lo_semantics/manager_init.h"

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

constexpr std::uint64_t GuestSpace = std::uint64_t{1} << 32;
constexpr GuestAddress Global = 0x8330b608u;
constexpr GuestAddress GlobalPage = Global & ~GuestAddress{0xfff};
constexpr std::size_t LowCommit = 0x40000;
constexpr GuestAddress OrdinaryStack = 0x3f000;
constexpr GuestAddress PrimaryAllocation = 0x10000;
constexpr GuestAddress PrimaryManager = 0x11000;
constexpr GuestAddress FallbackAllocation = 0x12000;
constexpr GuestAddress FallbackManager = 0x13000;
constexpr GuestAddress AlternateManager = 0x14000;
constexpr GuestAddress NullVtable = 0x20000;
constexpr GuestAddress PrimaryVtable = 0x20100;
constexpr GuestAddress FallbackVtable = 0x20200;
constexpr GuestAddress AlternateVtable = 0x20300;
constexpr GuestAddress CheckMethod = 0x82345670;
constexpr GuestAddress FinishMethod = 0x82345680;
constexpr std::uint32_t CallerLR = 0x81234567;

void Require(bool condition, const char* reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}

struct Window
{
    std::uint8_t* bytes = nullptr;
    Window()
    {
        bytes = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(bytes != nullptr, "reserve sparse 4 GiB guest address space");
        if (VirtualAlloc(bytes, LowCommit, MEM_COMMIT, PAGE_READWRITE) != bytes ||
            VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT, PAGE_READWRITE) !=
                bytes + GlobalPage)
        {
            VirtualFree(bytes, 0, MEM_RELEASE);
            bytes = nullptr;
            throw std::runtime_error("commit guest object, stack, and global pages");
        }
    }
    ~Window() { if (bytes != nullptr) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Case
{
    const char* name;
    std::uint64_t first_allocation = PrimaryAllocation;
    std::uint64_t primary_result = PrimaryManager;
    std::uint64_t check_result = 1;
    std::uint64_t second_allocation = FallbackAllocation;
    std::uint64_t fallback_result = FallbackManager;
    std::uint64_t finish_result = 0x9876543212345678ull;
    bool change_global = false;
    bool change_global_to_null = false;
    bool change_local = false;
    bool local_aliases_global = false;
};

struct Event
{
    char kind = 0;
    std::array<std::uint64_t, 2> args{};
    std::uint64_t before = 0;
    std::uint64_t after = 0;
    bool operator==(const Event&) const = default;
};

struct Run
{
    std::uint8_t* bytes;
    const Case& test;
    GuestAddress frame;
    std::vector<Event> events;
    unsigned allocations = 0;
    unsigned method_calls = 0;

    std::uint32_t Read32(GuestAddress address) const
    {
        return (std::uint32_t{bytes[address]} << 24) |
               (std::uint32_t{bytes[address + 1]} << 16) |
               (std::uint32_t{bytes[address + 2]} << 8) |
               bytes[address + 3];
    }
    void Write32(GuestAddress address, std::uint32_t value)
    {
        bytes[address] = static_cast<std::uint8_t>(value >> 24);
        bytes[address + 1] = static_cast<std::uint8_t>(value >> 16);
        bytes[address + 2] = static_cast<std::uint8_t>(value >> 8);
        bytes[address + 3] = static_cast<std::uint8_t>(value);
    }
    void Write64(GuestAddress address, std::uint64_t value)
    {
        Write32(address, static_cast<std::uint32_t>(value >> 32));
        Write32(address + 4, static_cast<std::uint32_t>(value));
    }
    std::uint64_t Fingerprint() const
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto hash_bytes = [&](const std::uint8_t* begin, std::size_t size)
        {
            for (std::size_t index = 0; index < size; ++index)
                hash = (hash ^ begin[index]) * 1099511628211ull;
        };
        hash_bytes(bytes, LowCommit);
        hash_bytes(bytes + GlobalPage, 0x1000);
        return hash;
    }
    void Begin(char kind, std::uint64_t first = 0, std::uint64_t second = 0)
    {
        events.push_back({kind, {first, second}, Fingerprint(), 0});
    }
    void End() { events.back().after = Fingerprint(); }

    std::uint64_t Allocate(std::uint32_t requested)
    {
        Begin('A', requested);
        const bool first = allocations++ == 0;
        Require(requested == (first ? 0x48decu : 36u),
                "raw allocation request follows the original branch");
        const std::uint64_t result = first ? test.first_allocation : test.second_allocation;
        End();
        return result;
    }
    std::uint64_t ConstructFirst(std::uint64_t allocation)
    {
        Begin('P', allocation);
        Require(static_cast<GuestAddress>(allocation) != 0,
                "primary construction needs an allocation");
        Write32(static_cast<GuestAddress>(allocation) + 4, 0x5052494du);
        End();
        return test.primary_result;
    }
    std::uint64_t ConstructSecond(std::uint64_t allocation,
                                  GuestAddress current_manager)
    {
        Begin('F', allocation, current_manager);
        Require(static_cast<GuestAddress>(allocation) != 0,
                "fallback construction needs an allocation");
        Write32(static_cast<GuestAddress>(allocation) + 4, current_manager);
        End();
        return test.fallback_result;
    }
    std::uint64_t Call(GuestAddress method, std::uint64_t receiver)
    {
        Begin('V', method, receiver);
        const bool first = method_calls++ == 0;
        Require(method == (first ? CheckMethod : FinishMethod),
                "vtable slot address and low-bit mask");
        if (first)
        {
            if (test.change_global)
                Write32(Global, test.change_global_to_null ? 0 : AlternateManager);
            if (test.change_local)
                Write32(frame + 80, test.local_aliases_global
                    ? AlternateManager : Read32(frame + 80) ^ 0x55aacc33u);
        }
        End();
        return first ? test.check_result : test.finish_result;
    }
};

Run* active = nullptr;

struct Services final : ManagerInitServices
{
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        return run.Allocate(bytes);
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        return run.ConstructFirst(allocation);
    }
    std::uint64_t ConstructFallback(std::uint64_t allocation,
                                     GuestAddress manager) override
    {
        return run.ConstructSecond(allocation, manager);
    }
    std::uint64_t CallMethod(GuestAddress method,
                             std::uint64_t receiver) override
    {
        return run.Call(method, receiver);
    }
};

void Setup(Run& run)
{
    for (std::size_t index = 0; index < LowCommit; ++index)
        run.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    for (std::size_t index = 0; index < 0x1000; ++index)
        run.bytes[GlobalPage + index] =
            static_cast<std::uint8_t>(index * 19u + 29u);
    run.Write32(0, NullVtable);
    for (const auto [object, vtable] : {
             std::array<GuestAddress, 2>{PrimaryAllocation, PrimaryVtable},
             std::array<GuestAddress, 2>{PrimaryManager, PrimaryVtable},
             std::array<GuestAddress, 2>{FallbackAllocation, FallbackVtable},
             std::array<GuestAddress, 2>{FallbackManager, FallbackVtable},
             std::array<GuestAddress, 2>{AlternateManager, AlternateVtable}})
        run.Write32(object, vtable);
    for (const GuestAddress vtable : {NullVtable, PrimaryVtable,
                                      FallbackVtable, AlternateVtable})
    {
        run.Write32(vtable + 60, CheckMethod | 3u);
        run.Write32(vtable + 56, FinishMethod | 2u);
    }
    run.Write32(Global, 0xfeed1234);
}

void PrepareRecoveredFrame(Run& run, GuestAddress stack)
{
    run.Write32(stack - 8, CallerLR);
    run.Write64(stack - 24, 0x1122334455667788ull);
    run.Write64(stack - 16, 0x99aabbccddeeff00ull);
    run.Write32(stack - 112, stack);
}

void Compare(const Case& test)
{
    Window original_window, recovered_window;
    const GuestAddress stack = test.local_aliases_global
        ? Global + 32 : OrdinaryStack;
    const GuestAddress frame = stack - 112;
    Run original{original_window.bytes, test, frame};
    Run recovered{recovered_window.bytes, test, frame};
    Setup(original);
    std::memcpy(recovered_window.bytes, original_window.bytes, LowCommit);
    std::memcpy(recovered_window.bytes + GlobalPage,
                original_window.bytes + GlobalPage, 0x1000);

    PPCContext context{};
    context.r1.u64 = stack;
    context.lr = CallerLR;
    context.r30.u64 = 0x1122334455667788ull;
    context.r31.u64 = 0x99aabbccddeeff00ull;
    active = &original;
    oracle_InitializeManager(context, original_window.bytes);
    active = nullptr;
    Require(context.r1.u64 == stack && context.lr == CallerLR &&
                context.r30.u64 == 0x1122334455667788ull &&
                context.r31.u64 == 0x99aabbccddeeff00ull,
            "original guest frame and nonvolatile ABI restoration");

    PrepareRecoveredFrame(recovered, stack);
    GuestMemory memory(0, {recovered_window.bytes, GuestSpace});
    Services services(recovered);
    const std::uint64_t result = InitializeManager(memory, services, frame);
    if (result != context.r3.u64)
    {
        std::fprintf(stderr, "return mismatch original=%016llx recovered=%016llx\n",
                     static_cast<unsigned long long>(context.r3.u64),
                     static_cast<unsigned long long>(result));
        throw std::runtime_error("full r3 callback return");
    }
    Require(original.events == recovered.events,
            "callback sequence, arguments, and before/after memory snapshots");
    Require(std::memcmp(original_window.bytes, recovered_window.bytes,
                        LowCommit) == 0 &&
                std::memcmp(original_window.bytes + GlobalPage,
                            recovered_window.bytes + GlobalPage, 0x1000) == 0,
            "ordinary guest memory including frame local and global");
    Require(original.allocations == recovered.allocations &&
                original.method_calls == recovered.method_calls &&
                original.method_calls == 2,
            "allocator and virtual callback counts");
}

} // namespace

PPC_FUNC(sub_823ACBD0)
{
    ctx.r3.u64 = active->Allocate(ctx.r3.u32);
}
PPC_FUNC(sub_827C5970)
{
    ctx.r3.u64 = active->ConstructFirst(ctx.r3.u64);
}
PPC_FUNC(sub_827C4ED0)
{
    ctx.r3.u64 = active->ConstructSecond(ctx.r3.u64, ctx.r4.u32);
}
void oracle_Indirect(PPCContext& ctx, std::uint8_t*, std::uint32_t address)
{
    ctx.r3.u64 = active->Call(address, ctx.r3.u64);
}

int main()
{
    try
    {
        unsigned cases = 0;
        const auto test = [&](const Case& scenario)
        {
            try { Compare(scenario); }
            catch (...)
            {
                std::fprintf(stderr, "case %u: %s\n", cases, scenario.name);
                throw;
            }
            ++cases;
        };
        test({"primary and check success"});
        test({"primary and fallback", PrimaryAllocation, PrimaryManager, 0});
        test({"null initial allocation", 0, PrimaryManager, 1});
        test({"high-half only allocation", 0xabcdef1200000000ull,
              PrimaryManager, 0});
        test({"null primary constructor", PrimaryAllocation, 0, 1});
        test({"high-half only primary constructor", PrimaryAllocation,
              0xabcdef1200000000ull, 0});
        test({"fallback allocation zero", PrimaryAllocation, PrimaryManager,
              0, 0});
        test({"fallback allocation high-half only", PrimaryAllocation,
              PrimaryManager, 0, 0xabcdef1200000000ull});
        test({"fallback constructor zero", PrimaryAllocation, PrimaryManager,
              0, FallbackAllocation, 0});
        test({"high halves and callbacks", 0x1111222200010000ull,
              0x3333444400011000ull, 0x5555666600000000ull,
              0x7777888800012000ull, 0x9999aaaa00013000ull,
              0xbbbccccd12345678ull});
        for (const bool fallback : {false, true})
            for (const bool change_global : {false, true})
                for (const bool null_global : {false, true})
                    for (const bool change_local : {false, true})
                        for (const bool alias : {false, true})
                        {
                            Case scenario{"branch and callback mutation"};
                            scenario.check_result = fallback ? 0 : 1;
                            scenario.change_global = change_global;
                            scenario.change_global_to_null = null_global;
                            scenario.change_local = change_local;
                            scenario.local_aliases_global = alias;
                            test(scenario);
                        }
        for (const std::uint64_t primary : {std::uint64_t{0},
                                            std::uint64_t{PrimaryManager}})
            for (const std::uint64_t second_allocation : {
                     std::uint64_t{0}, std::uint64_t{FallbackAllocation}})
                for (const std::uint64_t fallback_result : {
                         std::uint64_t{0}, std::uint64_t{FallbackManager}})
                    for (const bool alias : {false, true})
                    {
                        Case scenario{"fallback/null combinations"};
                        scenario.primary_result = primary;
                        scenario.check_result = 0;
                        scenario.second_allocation = second_allocation;
                        scenario.fallback_result = fallback_result;
                        scenario.local_aliases_global = alias;
                        test(scenario);
                    }
        std::printf("PASS 827C5F38 %u\n", cases);
        std::puts("LIMIT: synthetic raw allocator, constructors, and vtable methods; "
                  "ordinary guest memory and r3 only, not complete volatile "
                  "register, fault, MMIO, concurrency, or game runtime equivalence");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
