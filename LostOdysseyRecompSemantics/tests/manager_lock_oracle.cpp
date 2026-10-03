// Appended after three independently extracted original PPC bodies and their
// actual ABI save/restore helpers.
#include "lo_semantics/manager_lock.h"

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
constexpr std::size_t LowCommit = 0x40000;
constexpr GuestAddress GlobalPage = 0x83315000;
constexpr GuestAddress SectionGlobal = 0x83315fd4;
constexpr GuestAddress ManagerGlobal = 0x83315fd8;
constexpr GuestAddress FailureCount = 0x83315fdc;
constexpr GuestAddress StackTop = 0x3f000;
constexpr GuestAddress Section = 0x10000;
constexpr GuestAddress AlternateSection = 0x10100;
constexpr GuestAddress Manager = 0x11000;
constexpr GuestAddress AlternateManager = 0x11100;
constexpr GuestAddress Vtable = 0x12000;
constexpr GuestAddress AlternateVtable = 0x12100;
constexpr GuestAddress Holder = 0x20000;
constexpr GuestAddress OrdinaryMethod = 0x82345670;
constexpr GuestAddress AlternateMethod = 0x82345680;
constexpr GuestAddress ZeroMethod = 0x829664e8;
constexpr std::uint64_t SavedR29 = 0x1122334455667788ull;
constexpr std::uint64_t SavedR30 = 0x2233445566778899ull;
constexpr std::uint64_t SavedR31 = 0x33445566778899aaull;
constexpr std::uint32_t CallerLR = 0x81234567;
constexpr std::uint64_t LeaveResult = 0xfaceb00c12345678ull;

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
        Require(bytes != nullptr, "reserve sparse 4 GiB guest address space");
        if (VirtualAlloc(bytes, LowCommit, MEM_COMMIT, PAGE_READWRITE) != bytes ||
            VirtualAlloc(bytes + GlobalPage, 0x1000,
                         MEM_COMMIT, PAGE_READWRITE) != bytes + GlobalPage)
        {
            VirtualFree(bytes, 0, MEM_RELEASE);
            bytes = nullptr;
            throw std::runtime_error("commit guest object, stack, and global windows");
        }
    }
    ~Window() { if (bytes != nullptr) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

enum class Kind { Zero, DirectLock, Wrapper };
struct Case
{
    Kind kind;
    const char* name;
    std::uint64_t holder_register = Holder;
    std::uint64_t lock_register = Section;
    std::uint64_t initial_r3 = 0;
    unsigned failures = 0;
    std::uint32_t initial_failure_count = 0;
    bool change_counter_on_failure = false;
    bool change_globals_on_failure = false;
    bool change_holder_on_failure = false;
    bool change_holder_in_method = false;
    bool use_zero_method = false;
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
    GuestAddress holder;
    std::vector<Event> events;
    unsigned try_calls = 0;
    unsigned method_calls = 0;
    unsigned leave_calls = 0;

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
        add(bytes + GlobalPage, 0x1000);
        return hash;
    }
    void Begin(char kind, std::uint64_t first, std::uint64_t second = 0)
    {
        events.push_back({kind, {first, second}, Fingerprint(), 0});
    }
    void End() { events.back().after = Fingerprint(); }

    std::uint64_t Try(std::uint64_t section_register)
    {
        Begin('T', section_register);
        const unsigned attempt = try_calls++;
        const bool failed = attempt < test.failures;
        const std::uint64_t expected = test.kind == Kind::Wrapper
            ? std::uint64_t{Section} + 4 : test.lock_register + 4;
        Require(section_register == expected,
                "critical-section address remains fixed across retries");
        if (failed)
        {
            if (test.change_counter_on_failure)
                Write32(FailureCount, 0xfffffffeu - attempt);
            if (test.change_globals_on_failure)
            {
                Write32(SectionGlobal, AlternateSection);
                Write32(ManagerGlobal, AlternateManager);
            }
            if (test.change_holder_on_failure)
                Write32(holder, AlternateSection);
        }
        End();
        return failed ? 0xabcdef1200000000ull : 0x1234567800000001ull;
    }
    void BeginMethod(GuestAddress method, std::uint64_t receiver)
    {
        Begin('V', method, receiver);
        ++method_calls;
        Require(receiver == (test.change_globals_on_failure && test.failures
            ? AlternateManager : Manager),
            "manager global reloaded after acquisition");
        const GuestAddress expected = test.use_zero_method
            ? ZeroMethod
            : receiver == AlternateManager ? AlternateMethod : OrdinaryMethod;
        Require(method == expected, "indirect method address and tag mask");
        if (test.change_holder_in_method)
            Write32(holder, AlternateSection);
    }
    void EndMethod() { End(); }
    std::uint64_t Leave(std::uint64_t section_register)
    {
        Begin('L', section_register);
        ++leave_calls;
        const GuestAddress expected = test.change_holder_in_method ||
                                      test.change_holder_on_failure && test.failures
            ? AlternateSection + 4 : Section + 4;
        Require(section_register == expected,
                "leave reloads the holder slot after indirect callback");
        End();
        return LeaveResult;
    }
};

Run* active = nullptr;

struct Services final : ManagerLockServices
{
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint64_t TryEnterCriticalSection(
        std::uint64_t section_register) override
    {
        return run.Try(section_register);
    }
    std::uint64_t CallMethod(GuestAddress method,
                             std::uint64_t receiver) override
    {
        run.BeginMethod(method, receiver);
        const std::uint64_t result = run.test.use_zero_method
            ? ReturnZeroStatus() : 0x9876543212345678ull;
        run.EndMethod();
        return result;
    }
    std::uint64_t LeaveCriticalSection(
        std::uint64_t section_register) override
    {
        return run.Leave(section_register);
    }
};

void Setup(Run& run)
{
    for (std::size_t index = 0; index < LowCommit; ++index)
        run.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    for (std::size_t index = 0; index < 0x1000; ++index)
        run.bytes[GlobalPage + index] =
            static_cast<std::uint8_t>(index * 19u + 29u);
    run.Write32(SectionGlobal, Section);
    run.Write32(ManagerGlobal, Manager);
    run.Write32(FailureCount, run.test.initial_failure_count);
    run.Write32(Manager, Vtable);
    run.Write32(AlternateManager, AlternateVtable);
    for (const GuestAddress table : {Vtable, AlternateVtable})
        run.Write32(table + 56,
            (run.test.use_zero_method ? ZeroMethod :
             table == Vtable ? OrdinaryMethod : AlternateMethod) | 3u);
}

void PrepareDirectHelperFrame(Run& run)
{
    run.Write64(StackTop - 32, SavedR29);
    run.Write64(StackTop - 24, SavedR30);
    run.Write64(StackTop - 16, SavedR31);
    run.Write32(StackTop - 8, CallerLR);
    run.Write32(StackTop - 112, StackTop);
}

void PrepareWrapperAndChildFrames(Run& run)
{
    const GuestAddress frame = StackTop - 112;
    run.Write64(StackTop - 16, SavedR31);
    run.Write32(StackTop - 8, CallerLR);
    run.Write32(frame, StackTop);
    run.Write64(frame - 32, SavedR29);
    run.Write64(frame - 24, SavedR30);
    run.Write64(frame - 16, frame);
    run.Write32(frame - 8, 0x827c56ac);
    run.Write32(frame - 112, frame);
}

void Compare(const Case& test)
{
    Window original_window, recovered_window;
    const GuestAddress holder = test.kind == Kind::Wrapper
        ? StackTop - 112 + 80 : static_cast<GuestAddress>(test.holder_register);
    Run original{original_window.bytes, test, holder};
    Run recovered{recovered_window.bytes, test, holder};
    Setup(original);
    std::memcpy(recovered_window.bytes, original_window.bytes, LowCommit);
    std::memcpy(recovered_window.bytes + GlobalPage,
                original_window.bytes + GlobalPage, 0x1000);

    PPCContext context{};
    context.r1.u64 = StackTop;
    context.lr = CallerLR;
    context.r29.u64 = SavedR29;
    context.r30.u64 = SavedR30;
    context.r31.u64 = SavedR31;
    context.r3.u64 = test.kind == Kind::Zero ? test.initial_r3
                        : test.kind == Kind::DirectLock
                        ? test.holder_register : 0xabcdef1234567890ull;
    context.r4.u64 = test.lock_register;
    active = &original;
    if (test.kind == Kind::Zero)
        oracle_ReturnZeroStatus(context, original_window.bytes);
    else if (test.kind == Kind::DirectLock)
        oracle_StoreLockAndWaitForEnter(context, original_window.bytes);
    else
        oracle_InvokeManagerUnderLock(context, original_window.bytes);
    active = nullptr;
    Require(context.r1.u64 == StackTop && context.lr == CallerLR &&
            context.r29.u64 == SavedR29 && context.r30.u64 == SavedR30 &&
            context.r31.u64 == SavedR31,
            "original PPC stack and nonvolatile ABI restoration");

    GuestMemory memory(0, {recovered_window.bytes, GuestSpace});
    Services services(recovered);
    std::uint64_t result = 0;
    if (test.kind == Kind::Zero)
        result = ReturnZeroStatus();
    else if (test.kind == Kind::DirectLock)
    {
        PrepareDirectHelperFrame(recovered);
        result = StoreLockAndWaitForEnter(memory, services,
                                         test.holder_register, test.lock_register);
    }
    else
    {
        PrepareWrapperAndChildFrames(recovered);
        result = InvokeManagerUnderLock(memory, services, StackTop - 112);
    }
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
                    "event %zu %c/%016llx/%016llx/%016llx/%016llx "
                    "vs %c/%016llx/%016llx/%016llx/%016llx\n",
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
        throw std::runtime_error("PPC/recovered lock result or callback mismatch");
    }
    Require(std::memcmp(original_window.bytes, recovered_window.bytes,
                        LowCommit) == 0 &&
            std::memcmp(original_window.bytes + GlobalPage,
                        recovered_window.bytes + GlobalPage, 0x1000) == 0,
            "entire committed ordinary guest memory including live frame");
    Require(original.try_calls == recovered.try_calls &&
            original.try_calls == (test.kind == Kind::Zero ? 0 : test.failures + 1),
            "bounded retry-loop count");
    Require(original.method_calls == recovered.method_calls &&
            original.leave_calls == recovered.leave_calls &&
            original.method_calls == (test.kind == Kind::Wrapper ? 1 : 0) &&
            original.leave_calls == (test.kind == Kind::Wrapper ? 1 : 0),
            "virtual and leave callback counts");
}

} // namespace

PPC_FUNC(sub_822958F8)
{
    oracle_StoreLockAndWaitForEnter(ctx, base);
}
PPC_FUNC(__imp__RtlTryEnterCriticalSection)
{
    ctx.r3.u64 = active->Try(ctx.r3.u64);
}
PPC_FUNC(__imp__RtlLeaveCriticalSection)
{
    ctx.r3.u64 = active->Leave(ctx.r3.u64);
}
void oracle_Indirect(PPCContext& ctx, std::uint8_t* base,
                     std::uint32_t method)
{
    active->BeginMethod(method, ctx.r3.u64);
    if (active->test.use_zero_method)
        oracle_ReturnZeroStatus(ctx, base);
    else
        ctx.r3.u64 = 0x9876543212345678ull;
    active->EndMethod();
}

int main()
{
    try
    {
        unsigned zero_cases = 0, direct_cases = 0;
        unsigned wrapper_cases = 0, composed_cases = 0;
        const auto test = [&](const Case& scenario)
        {
            try { Compare(scenario); }
            catch (...)
            {
                std::fprintf(stderr, "case %s\n", scenario.name);
                throw;
            }
            if (scenario.kind == Kind::Zero) ++zero_cases;
            else if (scenario.kind == Kind::DirectLock) ++direct_cases;
            else
            {
                ++wrapper_cases;
                if (scenario.use_zero_method) ++composed_cases;
            }
        };
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            Case scenario{Kind::Zero, "constant zero return"};
            scenario.initial_r3 = 0x123456789abcdef0ull ^
                                  (seed * 0x1111111111111111ull);
            test(scenario);
        }
        for (unsigned failures = 0; failures < 4; ++failures)
            for (const bool change_counter : {false, true})
                for (unsigned alias = 0; alias < 3; ++alias)
                {
                    Case scenario{Kind::DirectLock, "direct lock acquisition"};
                    scenario.failures = failures;
                    scenario.initial_failure_count = alias == 1
                        ? 0xffffffffu : 0x1234u;
                    scenario.change_counter_on_failure = change_counter;
                    scenario.change_holder_on_failure = alias == 2;
                    scenario.holder_register = alias == 1
                        ? 0x1122334400000000ull | FailureCount
                        : alias == 2 ? 0x1122334400000000ull | Holder
                                     : Holder;
                    scenario.lock_register = 0x9988776600000000ull | Section;
                    test(scenario);
                }
        for (unsigned failures = 0; failures < 3; ++failures)
            for (const bool change_globals : {false, true})
                for (const bool change_holder : {false, true})
                    for (const bool zero_method : {false, true})
                    {
                        Case scenario{Kind::Wrapper, "manager lock wrapper"};
                        scenario.failures = failures;
                        scenario.initial_failure_count = failures == 2
                            ? 0xfffffffeu : 0x10u;
                        scenario.change_globals_on_failure = change_globals;
                        scenario.change_holder_in_method = change_holder;
                        scenario.use_zero_method = zero_method;
                        test(scenario);
                    }
        std::printf("PASS 829664E8 %u\n", zero_cases);
        std::printf("PASS 822958F8 %u\n", direct_cases);
        std::printf("PASS 827C5688 %u\n", wrapper_cases);
        std::printf("PASS manager-lock-composed %u\n", composed_cases);
        std::puts("LIMIT: original-PPC C++ and bounded ordinary guest memory; "
                  "Try/Leave kernel calls and nonzero virtual methods are "
                  "synthetic, ABI frame saves are test-only adapter writes");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
