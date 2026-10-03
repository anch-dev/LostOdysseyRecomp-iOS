// Appended after independently extracted PPC bodies for the manager, raw
// allocator, process-heap lookup, and both constructors.
#include "lo_semantics/manager_init.h"
#include "lo_semantics/manager_construction.h"
#include "lo_semantics/raw_allocation.h"

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
constexpr std::size_t LowCommit = 0xa0000;
constexpr GuestAddress StackTop = 0x9f000;
constexpr GuestAddress PrimaryObject = 0x10000;
constexpr GuestAddress FallbackObject = 0x64000;
constexpr GuestAddress AlternateObject = 0x70000;
constexpr GuestAddress Heap = 0x80000;
constexpr GuestAddress Errno = 0x81000;
constexpr GuestAddress Global = 0x8330b608;
constexpr GuestAddress HeapGlobal = 0x83245708;
constexpr GuestAddress RetryGlobal = 0x832d3aec;
constexpr GuestAddress SectionGlobal = 0x83315fd4;
constexpr GuestAddress PreviousGlobal = 0x83315fd8;
constexpr GuestAddress PrimaryConstant = 0x82000fe8;
constexpr GuestAddress PrimaryVtable = 0x82002700;
constexpr GuestAddress FallbackVtable = 0x8201dd90;
constexpr GuestAddress NullVtable = 0x3000;
constexpr GuestAddress AlternateVtable = 0x3100;
constexpr GuestAddress CheckMethod = 0x82345670;
constexpr GuestAddress FinishMethod = 0x82345680;
constexpr std::uint64_t SavedR28 = 0x1122334455667788ull;
constexpr std::uint64_t SavedR29 = 0x2233445566778899ull;
constexpr std::uint64_t SavedR30 = 0x33445566778899aaull;
constexpr std::uint64_t SavedR31 = 0x445566778899aabbull;
constexpr std::uint64_t ManagerR30 = 0xffffffff83310000ull;
constexpr std::uint32_t CallerLR = 0x81234567;
constexpr std::array<GuestAddress, 6> Pages{
    0x82000000u, 0x82002000u, 0x8201d000u, 0x83245000u,
    0x832d3000u, 0x8330b000u};
constexpr GuestAddress SectionPage = 0x83315000u;

void Require(bool condition, const char* reason)
{
    if (!condition) throw std::runtime_error(reason);
}

struct Window
{
    std::uint8_t* bytes = nullptr;
    Window()
    {
        bytes = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(bytes != nullptr, "reserve sparse 4 GiB guest space");
        if (VirtualAlloc(bytes, LowCommit, MEM_COMMIT, PAGE_READWRITE) != bytes)
            throw std::runtime_error("commit low guest/object/stack window");
        for (const GuestAddress page : Pages)
            if (VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE) !=
                bytes + page)
                throw std::runtime_error("commit guest constant/global page");
        if (VirtualAlloc(bytes + SectionPage, 0x1000,
                         MEM_COMMIT, PAGE_READWRITE) != bytes + SectionPage)
            throw std::runtime_error("commit section global page");
    }
    ~Window() { if (bytes != nullptr) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Case
{
    const char* name;
    bool fallback = false;
    bool first_allocation_null = false;
    bool fallback_allocation_null = false;
    bool retry_first_allocation = false;
    bool change_global_in_check = false;
    bool local_aliases_global = false;
};

struct Event
{
    char kind = 0;
    std::array<std::uint64_t, 3> args{};
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
    unsigned primary_attempts = 0;
    unsigned fallback_attempts = 0;
    unsigned retries = 0;
    unsigned virtual_calls = 0;

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
        const auto add = [&](const std::uint8_t* begin, std::size_t size)
        {
            for (std::size_t index = 0; index < size; ++index)
                hash = (hash ^ begin[index]) * 1099511628211ull;
        };
        add(bytes, LowCommit);
        for (const GuestAddress page : Pages) add(bytes + page, 0x1000);
        add(bytes + SectionPage, 0x1000);
        return hash;
    }
    void Begin(char kind, std::uint64_t a = 0, std::uint64_t b = 0,
               std::uint64_t c = 0)
    {
        events.push_back({kind, {a, b, c}, Fingerprint(), 0});
    }
    void End() { events.back().after = Fingerprint(); }

    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
                               std::uint64_t bytes_requested)
    {
        Begin('A', heap, flags, bytes_requested);
        Require(heap == Heap && flags == 0, "raw backend receives process heap");
        std::uint64_t result = 0;
        if (bytes_requested == 0x48decu)
        {
            const unsigned attempt = primary_attempts++;
            if (!test.first_allocation_null &&
                (!test.retry_first_allocation || attempt > 0))
                result = 0x1111222200010000ull;
        }
        else if (bytes_requested == 36)
        {
            ++fallback_attempts;
            if (!test.fallback_allocation_null)
                result = 0x3333444400064000ull;
        }
        else
            throw std::runtime_error("unexpected raw allocation size");
        End();
        return result;
    }
    std::int32_t Retry(std::uint64_t requested)
    {
        Begin('R', requested);
        const std::int32_t result = test.retry_first_allocation && retries++ == 0
            ? 1 : 0;
        End();
        return result;
    }
    GuestAddress ErrorAddress()
    {
        Begin('E'); End();
        return Errno;
    }
    void CriticalSection(std::uint64_t section_register)
    {
        Begin('K', section_register);
        Require(static_cast<GuestAddress>(section_register) == FallbackObject + 8,
                "fallback constructor section address");
        Write32(FallbackObject + 8, 0x5a5a1234);
        Write32(SectionGlobal, 0xfeed1234);
        End();
    }
    std::uint64_t Virtual(GuestAddress method, std::uint64_t receiver)
    {
        Begin('V', method, receiver);
        const bool first = virtual_calls++ == 0;
        Require(method == (first ? CheckMethod : FinishMethod),
                "vtable slot and tagged address mask");
        if (first && test.change_global_in_check)
            Write32(Global, AlternateObject);
        End();
        return first ? (test.fallback ? 0 : 0x5555666600000001ull)
                     : 0x777788889999aaaauLL;
    }
};

Run* active = nullptr;

void CopyPages(const Window& source, Window& destination)
{
    std::memcpy(destination.bytes, source.bytes, LowCommit);
    for (const GuestAddress page : Pages)
        std::memcpy(destination.bytes + page, source.bytes + page, 0x1000);
    std::memcpy(destination.bytes + SectionPage,
                source.bytes + SectionPage, 0x1000);
}

void Setup(Run& run)
{
    for (std::size_t index = 0; index < LowCommit; ++index)
        run.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    for (const GuestAddress page : Pages)
        for (std::uint32_t index = 0; index < 0x1000; ++index)
            run.bytes[page + index] =
                static_cast<std::uint8_t>(index * 19u + (page >> 12));
    for (std::uint32_t index = 0; index < 0x1000; ++index)
        run.bytes[SectionPage + index] =
            static_cast<std::uint8_t>(index * 29u + 3u);
    run.Write32(0, NullVtable);
    run.Write32(AlternateObject, AlternateVtable);
    run.Write32(PrimaryVtable + 60, CheckMethod | 3u);
    run.Write32(PrimaryVtable + 56, FinishMethod | 2u);
    run.Write32(FallbackVtable + 60, CheckMethod | 3u);
    run.Write32(FallbackVtable + 56, FinishMethod | 2u);
    for (const GuestAddress table : {NullVtable, AlternateVtable})
    {
        run.Write32(table + 60, CheckMethod | 3u);
        run.Write32(table + 56, FinishMethod | 2u);
    }
    run.Write32(PrimaryConstant, 0x12345678);
    run.Write32(PrimaryConstant + 4, 0x9abcdef0);
    run.Write32(HeapGlobal, Heap);
    run.Write32(RetryGlobal, run.test.retry_first_allocation ? 1 : 0);
    run.Write32(Global, 0xfeed1234);
}

void PrepareManagerFrame(Run& run, GuestAddress stack)
{
    run.Write32(stack - 8, CallerLR);
    run.Write64(stack - 24, SavedR30);
    run.Write64(stack - 16, SavedR31);
    run.Write32(stack - 112, stack);
}

void PrepareChildFrame(Run& run, std::uint32_t return_address,
                       std::uint64_t r30)
{
    run.Write64(run.frame - 40, SavedR28);
    run.Write64(run.frame - 32, SavedR29);
    run.Write64(run.frame - 24, r30);
    run.Write64(run.frame - 16, run.frame);
    run.Write32(run.frame - 8, return_address);
    run.Write32(run.frame - 128, run.frame);
}

struct Services final : ManagerInitServices,
                        RawAllocationServices,
                        ManagerConstructionServices
{
    Run& run;
    GuestMemory& memory;
    unsigned raw_calls = 0;
    Services(Run& value, GuestMemory& guest) : run(value), memory(guest) {}

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        const bool first = raw_calls++ == 0;
        PrepareChildFrame(run, first ? 0x827c5f5c : 0x827c5f9c,
                          first ? SavedR30 : ManagerR30);
        return AllocateRawMemory(memory, *this, bytes);
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        return ConstructPrimaryManager(memory, allocation, run.frame);
    }
    std::uint64_t ConstructFallback(std::uint64_t allocation,
                                     GuestAddress current_manager) override
    {
        PrepareChildFrame(run, 0x827c5fb0, ManagerR30);
        return ConstructFallbackManager(memory, *this, allocation,
                                        current_manager, run.frame - 128);
    }
    std::uint64_t CallMethod(GuestAddress method,
                             std::uint64_t receiver) override
    {
        return run.Virtual(method, receiver);
    }
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
                               std::uint64_t bytes) override
    {
        return run.AllocateHeap(heap, flags, bytes);
    }
    void EnterMissingHeapPath() override
    {
        throw std::runtime_error("unexpected missing process heap");
    }
    void ReportMissingHeap(std::uint32_t) override
    {
        throw std::runtime_error("unexpected CRT report");
    }
    void TerminateMissingHeap(std::uint32_t) override
    {
        throw std::runtime_error("unexpected CRT termination");
    }
    std::int32_t RetryAllocation(std::uint64_t bytes) override
    {
        return run.Retry(bytes);
    }
    GuestAddress GetErrorAddress() override
    {
        return run.ErrorAddress();
    }
    void InitializeCriticalSection(std::uint64_t address) override
    {
        run.CriticalSection(address);
    }
};

void Compare(const Case& test)
{
    Window original_window, semantic_window;
    const GuestAddress stack = test.local_aliases_global
        ? Global + 32 : StackTop;
    const GuestAddress frame = stack - 112;
    Run original{original_window.bytes, test, frame};
    Run recovered{semantic_window.bytes, test, frame};
    Setup(original);
    CopyPages(original_window, semantic_window);

    PPCContext ctx{};
    ctx.r1.u64 = stack;
    ctx.lr = CallerLR;
    ctx.r28.u64 = SavedR28;
    ctx.r29.u64 = SavedR29;
    ctx.r30.u64 = SavedR30;
    ctx.r31.u64 = SavedR31;
    active = &original;
    oracle_InitializeManager(ctx, original_window.bytes);
    active = nullptr;
    Require(ctx.r1.u64 == stack && ctx.lr == CallerLR &&
            ctx.r28.u64 == SavedR28 && ctx.r29.u64 == SavedR29 &&
            ctx.r30.u64 == SavedR30 && ctx.r31.u64 == SavedR31,
            "original composed ABI restoration");

    PrepareManagerFrame(recovered, stack);
    GuestMemory memory(0, {semantic_window.bytes, GuestSpace});
    Services services(recovered, memory);
    const std::uint64_t result = InitializeManager(memory, services, frame);
    if (result != ctx.r3.u64 || original.events != recovered.events)
    {
        std::fprintf(stderr,
            "return original=%016llx recovered=%016llx events=%zu/%zu\n",
            static_cast<unsigned long long>(ctx.r3.u64),
            static_cast<unsigned long long>(result),
            original.events.size(), recovered.events.size());
        for (std::size_t index = 0;
             index < original.events.size() && index < recovered.events.size();
             ++index)
            if (!(original.events[index] == recovered.events[index]))
                std::fprintf(stderr,
                    "event %zu original=%c/%016llx/%016llx/%016llx/%016llx "
                    "recovered=%c/%016llx/%016llx/%016llx/%016llx\n",
                    index, original.events[index].kind,
                    static_cast<unsigned long long>(original.events[index].args[0]),
                    static_cast<unsigned long long>(original.events[index].args[1]),
                    static_cast<unsigned long long>(original.events[index].before),
                    static_cast<unsigned long long>(original.events[index].after),
                    recovered.events[index].kind,
                    static_cast<unsigned long long>(recovered.events[index].args[0]),
                    static_cast<unsigned long long>(recovered.events[index].args[1]),
                    static_cast<unsigned long long>(recovered.events[index].before),
                    static_cast<unsigned long long>(recovered.events[index].after));
        throw std::runtime_error("composed return or callback transition mismatch");
    }
    Require(std::memcmp(original_window.bytes, semantic_window.bytes,
                        LowCommit) == 0,
            "composed low guest memory including nested stack frames");
    for (const GuestAddress page : Pages)
        Require(std::memcmp(original_window.bytes + page,
                            semantic_window.bytes + page, 0x1000) == 0,
                "composed guest constant/global/vtable page");
    Require(std::memcmp(original_window.bytes + SectionPage,
                        semantic_window.bytes + SectionPage, 0x1000) == 0,
            "composed fallback section globals");
    Require(original.virtual_calls == 2 && recovered.virtual_calls == 2,
            "both manager virtual methods called");
}

} // namespace

PPC_FUNC(sub_823ACCB0)
{
    ctx.r3.u64 = active->AllocateHeap(ctx.r3.u32, ctx.r4.u32, ctx.r5.u64);
}
PPC_FUNC(sub_82B7FCE0)
{
    throw std::runtime_error("unexpected original missing process heap");
}
PPC_FUNC(sub_82B7FC98)
{
    throw std::runtime_error("unexpected original CRT report");
}
PPC_FUNC(sub_82B7BF20)
{
    throw std::runtime_error("unexpected original CRT termination");
}
PPC_FUNC(sub_82B7FE68)
{
    ctx.r3.s64 = active->Retry(ctx.r3.u64);
}
PPC_FUNC(sub_82B7FD78)
{
    ctx.r3.u64 = active->ErrorAddress();
}
PPC_FUNC(__imp__RtlInitializeCriticalSection)
{
    active->CriticalSection(ctx.r3.u64);
}
void oracle_Indirect(PPCContext& ctx, std::uint8_t*, std::uint32_t address)
{
    ctx.r3.u64 = active->Virtual(address, ctx.r3.u64);
}

int main()
{
    try
    {
        const std::array cases{
            Case{"primary success"},
            Case{"fallback success", true},
            Case{"primary allocation null", false, true},
            Case{"fallback allocation null", true, false, true},
            Case{"first virtual changes global", false, false, false,
                 false, true},
            Case{"fallback uses changed global", true, false, false,
                 false, true},
            Case{"raw CRT retry then success", false, false, false,
                 true},
            Case{"raw retry then fallback", true, false, false, true},
            Case{"local aliases global primary", false, false, false,
                 false, false, true},
            Case{"local aliases global fallback", true, false, false,
                 false, false, true},
            Case{"alias, fallback, global mutation", true, false, false,
                 false, true, true},
            Case{"alias, primary, global mutation", false, false, false,
                 false, true, true},
        };
        for (unsigned index = 0; index < cases.size(); ++index)
        {
            try { Compare(cases[index]); }
            catch (...)
            {
                std::fprintf(stderr, "case %u: %s\n", index,
                             cases[index].name);
                throw;
            }
        }
        std::printf("PASS manager-lifecycle %zu\n", cases.size());
        std::puts("LIMIT: native generated PPC composition with synthetic "
                  "heap allocation, CRT callbacks, kernel critical section "
                  "and virtual methods; no runtime, MMIO, fault, or "
                  "concurrency equivalence");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
