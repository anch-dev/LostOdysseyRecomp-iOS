// Appended after ten independently extracted PPC bodies and six ABI helpers.
#include "lo_semantics/manager_init.h"
#include "lo_semantics/manager_construction.h"
#include "lo_semantics/manager_lock.h"
#include "lo_semantics/manager_storage.h"
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
constexpr std::size_t ObjectCompare = 0x90000;
constexpr GuestAddress StackTop = 0x9f000;
constexpr GuestAddress PrimaryObject = 0x10000;
constexpr GuestAddress FallbackObject = 0x64000;
constexpr GuestAddress Heap = 0x80000;
constexpr GuestAddress Errno = 0x81000;
constexpr GuestAddress ManagerGlobal = 0x8330b608;
constexpr GuestAddress HeapGlobal = 0x83245708;
constexpr GuestAddress RetryGlobal = 0x832d3aec;
constexpr GuestAddress SectionGlobal = 0x83315fd4;
constexpr GuestAddress PreviousGlobal = 0x83315fd8;
constexpr GuestAddress FailureCount = 0x83315fdc;
constexpr GuestAddress PrimaryConstant = 0x82000fe8;
constexpr GuestAddress PrimaryVtable = 0x82002700;
constexpr GuestAddress FallbackVtable = 0x8201dd90;
constexpr GuestAddress ZeroMethod = 0x829664e8;
constexpr GuestAddress StorageMethod = 0x827c5d88;
constexpr GuestAddress LockMethod = 0x827c5688;
constexpr std::uint64_t PrimaryRegister = 0x1111222200010000ull;
constexpr std::uint64_t FallbackRegister = 0x3333444400064000ull;
constexpr std::uint64_t LeaveResult = 0xfaceb00c12345678ull;
constexpr std::uint64_t SavedR28 = 0x1122334455667788ull;
constexpr std::uint64_t SavedR29 = 0x2233445566778899ull;
constexpr std::uint64_t SavedR30 = 0x33445566778899aaull;
constexpr std::uint64_t SavedR31 = 0x445566778899aabbull;
constexpr std::uint32_t CallerLR = 0x81234567;
constexpr std::array<GuestAddress, 7> Pages{
    0x82000000u, 0x82002000u, 0x8201d000u,
    0x83245000u, 0x832d3000u, 0x8330b000u, 0x83315000u};

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
            throw std::runtime_error("commit object and stack window");
        for (const GuestAddress page : Pages)
            if (VirtualAlloc(bytes + page, 0x1000,
                             MEM_COMMIT, PAGE_READWRITE) != bytes + page)
                throw std::runtime_error("commit constant/vtable/global page");
    }
    ~Window() { if (bytes != nullptr) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Case
{
    const char* name;
    unsigned lock_failures = 0;
    bool heap_retry = false;
    bool mutate_failure_count = false;
    std::uint32_t initial_failure_count = 0;
};

struct Event
{
    char kind = 0;
    std::array<std::uint64_t, 3> args{};
    std::array<std::uint32_t, 6> live{};
    std::uint64_t before = 0;
    std::uint64_t after = 0;
    bool operator==(const Event&) const = default;
};

struct Run
{
    std::uint8_t* bytes;
    const Case& test;
    std::vector<Event> events;
    unsigned primary_attempts = 0;
    unsigned fallback_attempts = 0;
    unsigned retries = 0;
    unsigned try_calls = 0;
    unsigned leave_calls = 0;
    unsigned zero_calls = 0;
    unsigned storage_calls = 0;
    unsigned lock_calls = 0;
    bool lock_started = false;

    [[nodiscard]] GuestAddress ManagerFrame() const { return StackTop - 112; }
    [[nodiscard]] GuestAddress FallbackFrame() const { return ManagerFrame() - 128; }
    [[nodiscard]] GuestAddress LockFrame() const { return ManagerFrame() - 112; }

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
    std::uint64_t Fingerprint() const
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto add = [&](const std::uint8_t* begin, std::size_t size)
        {
            for (std::size_t index = 0; index < size; ++index)
                hash = (hash ^ begin[index]) * 1099511628211ull;
        };
        add(bytes, ObjectCompare);
        for (const GuestAddress page : Pages) add(bytes + page, 0x1000);
        return hash;
    }
    std::array<std::uint32_t, 6> Live(bool primary_spill) const
    {
        return {
            Read32(ManagerFrame() + 80),
            Read32(FallbackFrame() + 80),
            Read32(FallbackFrame() + 148),
            lock_started ? Read32(LockFrame() + 80) : 0u,
            primary_spill ? Read32(ManagerFrame() - 16) : 0u,
            primary_spill ? Read32(ManagerFrame() - 12) : 0u,
        };
    }
    std::size_t Begin(char kind, std::uint64_t first = 0,
                      std::uint64_t second = 0, std::uint64_t third = 0,
                      bool primary_spill = false)
    {
        const std::size_t index = events.size();
        events.push_back({kind, {first, second, third},
                          Live(primary_spill), Fingerprint(), 0});
        return index;
    }
    void End(std::size_t index)
    {
        events[index].after = Fingerprint();
    }
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
                               std::uint64_t bytes_requested)
    {
        const std::size_t event = Begin('A', heap, flags, bytes_requested);
        Require(heap == Heap && flags == 0, "real GetProcessHeap guest value");
        std::uint64_t result = 0;
        if (bytes_requested == 0x48decu)
        {
            const unsigned attempt = primary_attempts++;
            if (!test.heap_retry || attempt > 0) result = PrimaryRegister;
        }
        else if (bytes_requested == 36)
        {
            ++fallback_attempts;
            result = FallbackRegister;
        }
        else throw std::runtime_error("unexpected raw allocation size");
        End(event);
        return result;
    }
    std::int32_t Retry(std::uint64_t bytes_requested)
    {
        const std::size_t event = Begin('R', bytes_requested);
        Require(bytes_requested == 0x48decu, "retry original primary size");
        const std::int32_t result = retries++ == 0 ? 1 : 0;
        End(event);
        return result;
    }
    GuestAddress ErrorAddress()
    {
        const std::size_t event = Begin('E'); End(event);
        return Errno;
    }
    void InitializeSection(std::uint64_t section_register)
    {
        const std::size_t event = Begin('K', section_register);
        Require(section_register == FallbackRegister + 8,
                "fallback constructor critical-section address");
        Write32(FallbackObject + 8, 0x5a5a1234);
        Write32(SectionGlobal, 0xfeed1234);
        End(event);
    }
    std::uint64_t Try(std::uint64_t section_register)
    {
        lock_started = true;
        const std::size_t event = Begin('T', section_register);
        Require(section_register == FallbackObject + 8,
                "lock helper keeps original section address on retry");
        const bool failed = try_calls++ < test.lock_failures;
        if (failed && test.mutate_failure_count)
            Write32(FailureCount, 0xfffffffeu);
        End(event);
        return failed ? 0xabcdef1200000000ull : 0x1234567800000001ull;
    }
    std::uint64_t Leave(std::uint64_t section_register)
    {
        const std::size_t event = Begin('L', section_register);
        ++leave_calls;
        Require(section_register == FallbackObject + 8,
                "leave address from live lock-wrapper frame slot");
        End(event);
        return LeaveResult;
    }
    std::size_t BeginMethod(GuestAddress method, std::uint64_t receiver)
    {
        const bool primary_spill = method == ZeroMethod;
        const std::size_t event = Begin('V', method, receiver, 0,
                                        primary_spill);
        if (method == ZeroMethod)
        {
            ++zero_calls;
            Require(receiver == PrimaryRegister,
                    "first vtable call uses full primary constructor r3");
        }
        else if (method == LockMethod)
        {
            ++lock_calls;
            Require(receiver == FallbackRegister,
                    "second vtable call uses full fallback constructor r3");
        }
        else if (method == StorageMethod)
        {
            ++storage_calls;
            Require(receiver == PrimaryObject,
                    "lock wrapper reloads zero-extended prior manager global");
        }
        else throw std::runtime_error("unexpected actual manager vtable target");
        return event;
    }
};

Run* active = nullptr;

struct Services final : ManagerInitServices,
                        RawAllocationServices,
                        ManagerConstructionServices,
                        ManagerLockServices
{
    Run& run;
    GuestMemory& memory;
    Services(Run& value, GuestMemory& guest) : run(value), memory(guest) {}

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        return AllocateRawMemory(memory, *this, bytes);
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        return ConstructPrimaryManager(memory, allocation, run.ManagerFrame());
    }
    std::uint64_t ConstructFallback(std::uint64_t allocation,
                                     GuestAddress current_manager) override
    {
        return ConstructFallbackManager(memory, *this, allocation,
                                        current_manager, run.FallbackFrame());
    }
    std::uint64_t CallMethod(GuestAddress method,
                             std::uint64_t receiver) override
    {
        const std::size_t event = run.BeginMethod(method, receiver);
        std::uint64_t result = 0;
        if (method == ZeroMethod)
            result = ReturnZeroStatus();
        else if (method == LockMethod)
            result = InvokeManagerUnderLock(memory, *this, run.LockFrame());
        else
            result = InitializePrimaryManagerStorage(memory, receiver);
        run.End(event);
        return result;
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
        run.InitializeSection(address);
    }
    std::uint64_t TryEnterCriticalSection(std::uint64_t address) override
    {
        return run.Try(address);
    }
    std::uint64_t LeaveCriticalSection(std::uint64_t address) override
    {
        return run.Leave(address);
    }
};

void Setup(Run& run)
{
    for (std::size_t index = 0; index < LowCommit; ++index)
        run.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    for (const GuestAddress page : Pages)
        for (std::uint32_t index = 0; index < 0x1000; ++index)
            run.bytes[page + index] =
                static_cast<std::uint8_t>(index * 19u + (page >> 12));
    run.Write32(PrimaryConstant, 0x12345678);
    run.Write32(PrimaryConstant + 4, 0x9abcdef0);
    run.Write32(PrimaryVtable + 60, ZeroMethod);
    run.Write32(PrimaryVtable + 56, StorageMethod);
    run.Write32(FallbackVtable + 60, ZeroMethod);
    run.Write32(FallbackVtable + 56, LockMethod);
    run.Write32(HeapGlobal, Heap);
    run.Write32(RetryGlobal, run.test.heap_retry ? 1 : 0);
    run.Write32(ManagerGlobal, 0xfeed1234);
    run.Write32(FailureCount, run.test.initial_failure_count);
}

void Compare(const Case& test)
{
    Window original_window, semantic_window;
    Run original{original_window.bytes, test};
    Run recovered{semantic_window.bytes, test};
    Setup(original);
    std::memcpy(semantic_window.bytes, original_window.bytes, LowCommit);
    for (const GuestAddress page : Pages)
        std::memcpy(semantic_window.bytes + page,
                    original_window.bytes + page, 0x1000);

    PPCContext context{};
    context.r1.u64 = StackTop;
    context.lr = CallerLR;
    context.r28.u64 = SavedR28;
    context.r29.u64 = SavedR29;
    context.r30.u64 = SavedR30;
    context.r31.u64 = SavedR31;
    active = &original;
    oracle_InitializeManager(context, original_window.bytes);
    active = nullptr;
    Require(context.r1.u64 == StackTop && context.lr == CallerLR &&
            context.r28.u64 == SavedR28 && context.r29.u64 == SavedR29 &&
            context.r30.u64 == SavedR30 && context.r31.u64 == SavedR31,
            "original nested ABI restoration");

    GuestMemory memory(0, {semantic_window.bytes, GuestSpace});
    Services services(recovered, memory);
    const std::uint64_t result = InitializeManager(memory, services,
                                                    recovered.ManagerFrame());
    if (result != context.r3.u64 || original.events != recovered.events)
    {
        std::fprintf(stderr,
            "return original=%016llx recovered=%016llx events=%zu/%zu\n",
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            original.events.size(), recovered.events.size());
        for (std::size_t index = 0;
             index < original.events.size() && index < recovered.events.size();
             ++index)
            if (!(original.events[index] == recovered.events[index]))
                std::fprintf(stderr,
                    "event %zu kind=%c/%c args=%016llx/%016llx "
                    "live0=%08x/%08x hash=%016llx/%016llx\n",
                    index, original.events[index].kind,
                    recovered.events[index].kind,
                    static_cast<unsigned long long>(original.events[index].args[0]),
                    static_cast<unsigned long long>(recovered.events[index].args[0]),
                    original.events[index].live[0],
                    recovered.events[index].live[0],
                    static_cast<unsigned long long>(original.events[index].before),
                    static_cast<unsigned long long>(recovered.events[index].before));
        throw std::runtime_error("startup return, event, or live local mismatch");
    }
    Require(std::memcmp(original_window.bytes, semantic_window.bytes,
                        ObjectCompare) == 0,
            "full ordinary object/lookup/bucket/pool memory");
    for (const GuestAddress page : Pages)
        Require(std::memcmp(original_window.bytes + page,
                            semantic_window.bytes + page, 0x1000) == 0,
                "full constant, vtable, and global pages");
    for (const GuestAddress address : {
             recovered.ManagerFrame() + 80,
             recovered.FallbackFrame() + 80,
             recovered.FallbackFrame() + 148,
             recovered.LockFrame() + 80})
        Require(original.Read32(address) == recovered.Read32(address),
                "final live nested-frame slot");
    Require(result == LeaveResult &&
            original.primary_attempts == recovered.primary_attempts &&
            original.fallback_attempts == recovered.fallback_attempts &&
            original.try_calls == recovered.try_calls &&
            original.leave_calls == recovered.leave_calls &&
            original.zero_calls == recovered.zero_calls &&
            original.lock_calls == recovered.lock_calls &&
            original.storage_calls == recovered.storage_calls &&
            original.zero_calls == 1 && original.lock_calls == 1 &&
            original.storage_calls == 1 && original.leave_calls == 1,
            "all three concrete virtual targets run once");
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
    active->InitializeSection(ctx.r3.u64);
}
PPC_FUNC(__imp__RtlTryEnterCriticalSection)
{
    ctx.r3.u64 = active->Try(ctx.r3.u64);
}
PPC_FUNC(__imp__RtlLeaveCriticalSection)
{
    ctx.r3.u64 = active->Leave(ctx.r3.u64);
}
void oracle_Indirect(PPCContext& context, std::uint8_t* base,
                     std::uint32_t address)
{
    const std::size_t event = active->BeginMethod(address, context.r3.u64);
    if (address == ZeroMethod)
        sub_829664E8(context, base);
    else if (address == LockMethod)
        sub_827C5688(context, base);
    else if (address == StorageMethod)
        sub_827C5D88(context, base);
    else
        throw std::runtime_error("unexpected original virtual target");
    active->End(event);
}

int main()
{
    try
    {
        const std::array cases{
            Case{"startup success"},
            Case{"one lock retry", 1},
            Case{"three lock retries and counter wrap", 3, false, false,
                 0xffffffffu},
            Case{"heap retry then startup", 0, true},
            Case{"heap and lock retries", 2, true},
            Case{"callback mutates failure counter", 2, false, true,
                 0xfffffffeu},
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
        std::printf("PASS manager-startup %zu\n", cases.size());
        std::puts("LIMIT: ten original-PPC C++ bodies compose against ten "
                  "recovered functions; heap/CRT/kernel calls are synthetic, "
                  "generic ABI scratch and volatile context are excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
