#include "lo_semantics/fallback_resize.h"
#include "lo_semantics/memory_move.h"

#include <array>
#include <bit>
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
constexpr std::size_t LowCommit = 0x400000;
constexpr GuestAddress GlobalPage = 0x83315000;
constexpr GuestAddress SectionGlobal = 0x83315fd4;
constexpr GuestAddress ManagerGlobal = 0x83315fd8;
constexpr GuestAddress FailureCount = 0x83315fdc;
constexpr GuestAddress StackTop = 0x3f0000;
constexpr GuestAddress Frame = StackTop - 176;
constexpr GuestAddress ThreadSlot = 0x1000;
constexpr GuestAddress ThreadState = 0x1100;
constexpr GuestAddress Pool = 0x20000;
constexpr GuestAddress Manager = 0x40000;
constexpr GuestAddress Manager2 = 0x48000;
constexpr GuestAddress Vtable = 0x50000;
constexpr GuestAddress Vtable2 = 0x50100;
constexpr GuestAddress Lookup = 0x70000;
constexpr GuestAddress Old = 0x28000;
constexpr GuestAddress New = 0x29000;
constexpr GuestAddress Section = 0x60000;
constexpr GuestAddress Section2 = 0x60100;
constexpr GuestAddress FirstMethod = 0x81234540;
constexpr GuestAddress SecondMethod = 0x81234550;
constexpr GuestAddress FreeMethod = 0x81234560;
constexpr GuestAddress FreeMethod2 = 0x81234570;
constexpr std::uint64_t ReallocResult = 0xabcdef1287654321ull;
constexpr std::uint64_t GrowResult = 0x1122334455667788ull;
constexpr std::uint32_t CallerLR = 0x81234567;

void Require(bool value, const char* reason)
{
    if (!value) throw std::runtime_error(reason);
}

struct Window
{
    std::uint8_t* bytes;
    Window()
        : bytes(static_cast<std::uint8_t*>(
              VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS)))
    {
        Require(bytes != nullptr, "reserve sparse 4GiB guest address space");
        if (VirtualAlloc(bytes, LowCommit, MEM_COMMIT, PAGE_READWRITE) != bytes ||
            VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT,
                         PAGE_READWRITE) != bytes + GlobalPage)
        {
            VirtualFree(bytes, 0, MEM_RELEASE);
            throw std::runtime_error("commit ordinary and global guest windows");
        }
    }
    ~Window() { VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

enum class Kind { Append, Resize };
enum class Path {
    DirectAppend, NoPool, NewZero, NewPop, NewEmpty, Untracked,
    FreeClass, FreeManager, MoveClass, MoveManager, MoveEmpty, Oversize
};
struct Case
{
    Kind kind;
    Path path;
    const char* name;
    std::uint64_t old_register = Old;
    std::uint64_t size_register = 24;
    std::uint64_t arg_register = 0x12345678abcdef90ull;
    std::uint64_t vector_register = Pool + 20;
    std::uint32_t old_size = 40;
    std::uint32_t class_count = 1;
    std::uint32_t vector_count = 0;
    std::uint32_t vector_capacity = 4;
    unsigned try_failures = 0;
    bool mutate_globals = false;
    bool mutate_holder = false;
    bool grow_mutates = false;
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 5> args{};
    std::uint64_t before = 0;
    std::uint64_t after = 0;
    GuestAddress holder_before = 0;
    GuestAddress holder_after = 0;
    bool operator==(const Event&) const = default;
};

struct Run
{
    std::uint8_t* bytes;
    const Case& test;
    std::vector<Event> events;
    unsigned tries = 0;
    unsigned grows = 0;
    unsigned reallocs = 0;
    unsigned releases = 0;
    unsigned leaves = 0;

    std::uint32_t Read32(GuestAddress address) const
    {
        return (std::uint32_t{bytes[address]} << 24) |
               (std::uint32_t{bytes[address + 1]} << 16) |
               (std::uint32_t{bytes[address + 2]} << 8) | bytes[address + 3];
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
    std::uint64_t HashOrdinary() const
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto add = [&](GuestAddress start, GuestAddress end)
        {
            for (GuestAddress i = start; i < end; ++i)
                hash = (hash ^ bytes[i]) * 1099511628211ull;
        };
        add(0, Frame - 0x100);
        add(StackTop, LowCommit);
        add(GlobalPage, GlobalPage + 0x1000);
        return hash;
    }
    void Begin(char kind, std::array<std::uint64_t, 5> args)
    {
        events.push_back({kind, args, HashOrdinary(), 0,
                          Read32(Frame + 80), 0});
    }
    void End()
    {
        events.back().after = HashOrdinary();
        events.back().holder_after = Read32(Frame + 80);
    }
    std::uint64_t Try(std::uint64_t section)
    {
        Begin('T', {section});
        Require(section == Section + 4u, "Try address");
        const bool failure = tries++ < test.try_failures;
        if (failure)
        {
            if (test.mutate_globals)
            {
                Write32(ManagerGlobal, Manager2);
                Write32(SectionGlobal, Section2);
            }
            Write32(FailureCount, Read32(FailureCount) + 100u);
        }
        End();
        return failure ? 0x1234567800000000ull : 0x1234567800000001ull;
    }
    std::uint64_t Leave(std::uint64_t section)
    {
        Begin('L', {section});
        ++leaves;
        const GuestAddress expected =
            test.mutate_holder ? Section2 + 4u : Section + 4u;
        Require(section == expected, "Leave reloads frame+80");
        End();
        return 0x123456789abcdef0ull;
    }
    std::uint64_t Grow(std::uint64_t vector, std::uint64_t value)
    {
        Begin('G', {vector, value});
        ++grows;
        Require(static_cast<GuestAddress>(value) == Old,
                "Grow receives full old pointer");
        if (test.grow_mutates)
        {
            const GuestAddress target = static_cast<GuestAddress>(vector);
            Write32(target, Lookup + 0x1000);
            Write32(target + 12, 2);
            Write32(target + 16, 8);
        }
        End();
        return GrowResult;
    }
    std::uint64_t Realloc(GuestAddress method, std::uint64_t receiver,
                          std::uint64_t old_value, std::uint64_t size,
                          std::uint64_t arg)
    {
        Begin('R', {method, receiver, old_value, size, arg});
        ++reallocs;
        Require(method == (receiver == Manager2 ? SecondMethod : FirstMethod),
                "reallocate vtable target");
        if (test.mutate_holder) Write32(Frame + 80, Section2);
        End();
        return ReallocResult;
    }
    std::uint64_t Release(GuestAddress method, std::uint64_t receiver,
                          std::uint64_t old_value)
    {
        Begin('F', {method, receiver, old_value});
        ++releases;
        Require(method == (receiver == Manager2 ? FreeMethod2 : FreeMethod),
                "release vtable target");
        if (test.mutate_holder) Write32(Frame + 80, Section2);
        End();
        return 0xdeadbeef12345678ull;
    }
};

Run* active = nullptr;

struct Services final : ManagerLockServices, FallbackResizeServices
{
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint64_t TryEnterCriticalSection(std::uint64_t section) override
    { return run.Try(section); }
    std::uint64_t LeaveCriticalSection(std::uint64_t section) override
    { return run.Leave(section); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unused lock method"); }
    std::uint64_t GrowPointerVector(std::uint64_t vector,
                                    std::uint64_t value) override
    { return run.Grow(vector, value); }
    std::uint64_t ReallocateThroughManager(
        GuestAddress method, std::uint64_t receiver, std::uint64_t old_value,
        std::uint64_t size, std::uint64_t arg) override
    { return run.Realloc(method, receiver, old_value, size, arg); }
    std::uint64_t ReleaseThroughManager(
        GuestAddress method, std::uint64_t receiver,
        std::uint64_t old_value) override
    { return run.Release(method, receiver, old_value); }
};

GuestAddress TableOffset(GuestAddress pointer)
{
    return std::rotl(pointer, 21) & 0xffe0u;
}
GuestAddress TableSlot(GuestAddress pointer)
{
    return (std::rotl(pointer, 5) & 31u) + 846u;
}
void Setup(Run& run)
{
    for (std::size_t i = 0; i < LowCommit; ++i)
        run.bytes[i] = static_cast<std::uint8_t>(i * 29u + 7u);
    for (std::size_t i = 0; i < 0x1000; ++i)
        run.bytes[GlobalPage + i] = static_cast<std::uint8_t>(i * 17u + 11u);
    run.Write32(ThreadSlot, ThreadState);
    run.Write32(ThreadState + 4,
                run.test.path == Path::NoPool ? 0 : Pool);
    run.Write32(SectionGlobal, Section);
    run.Write32(ManagerGlobal, Manager);
    run.Write32(FailureCount, 0xfffffffdu);
    run.Write32(Manager, Vtable);
    run.Write32(Manager2, Vtable2);
    run.Write32(Vtable + 8, FirstMethod | 3u);
    run.Write32(Vtable2 + 8, SecondMethod | 3u);
    run.Write32(Vtable + 12, FreeMethod | 3u);
    run.Write32(Vtable2 + 12, FreeMethod2 | 3u);
    run.Write32(Manager + TableSlot(Old) * 4u, Lookup);
    run.Write32(Manager2 + TableSlot(Old) * 4u, Lookup);
    run.Write32(Lookup + TableOffset(Old),
                run.test.path == Path::Untracked ? 0xffffffffu :
                run.test.old_size);
    run.Write32(Pool + 108, 256);
    for (std::uint32_t i = 1; i <= 6; ++i)
    {
        const GuestAddress descriptor = Pool + i * 20u;
        const GuestAddress backing = 0x90000 + i * 0x100u;
        run.Write32(descriptor, backing);
        run.Write32(descriptor + 4, i == 1 ? 32 : i == 2 ? 64 :
                    i == 3 ? 128 : i == 4 ? 256 : i == 5 ? 384 : 511);
        run.Write32(descriptor + 8, i == 1 ? 64 : i == 2 ? 128 :
                    i == 3 ? 256 : i == 4 ? 384 : i == 5 ? 512 : 1024);
        run.Write32(descriptor + 12, i == 1 ? run.test.class_count : 1);
        run.Write32(backing, New + (i - 1u) * 0x1000u);
        run.Write32(backing + 4, New + (i - 1u) * 0x1000u + 0x100u);
    }
    run.Write32(Pool, 0x98000);
    run.Write32(Pool + 12, run.test.vector_count);
    run.Write32(Pool + 16, run.test.vector_capacity);
    run.Write32(0x98000, 0);
    run.Write32(0x98004, 0);
    run.Write32(Pool + 20, 0x90000 + 0x100);
    run.Write32(Pool + 32, run.test.vector_count);
    run.Write32(Pool + 36, run.test.vector_capacity);
    if (run.test.kind == Kind::Append)
    {
        const auto vector = static_cast<GuestAddress>(
            run.test.vector_register);
        run.Write32(vector, 0x90000);
        run.Write32(vector + 12, run.test.vector_count);
        run.Write32(vector + 16, run.test.vector_capacity);
    }
    if (run.test.path == Path::MoveEmpty)
        run.Write32(Pool + 32, 0);
}
void InitContext(PPCContext& context, const Case& test)
{
    context.r1.u64 = StackTop;
    context.lr = CallerLR;
    context.r13.u64 = 0x1122334400000000ull | ThreadSlot;
    context.r3.u64 = test.kind == Kind::Append
        ? test.vector_register : 0x9988776655443322ull;
    context.r4.u64 = test.kind == Kind::Append
        ? test.old_register : test.old_register;
    context.r5.u64 = test.size_register;
    context.r6.u64 = test.arg_register;
    unsigned i = 23;
    for (PPCRegister* reg : std::array<PPCRegister*, 9>{
             &context.r23, &context.r24, &context.r25,
             &context.r26, &context.r27, &context.r28,
             &context.r29, &context.r30, &context.r31})
        reg->u64 = 0x1111111100000000ull + i++ * 0x100000001ull;
}
void PrepareSemanticFrame(Run& run, const PPCContext& seed)
{
    if (run.test.kind == Kind::Append)
    {
        run.Write32(StackTop - 8, CallerLR);
        run.Write64(StackTop - 24, seed.r30.u64);
        run.Write64(StackTop - 16, seed.r31.u64);
        run.Write32(StackTop - 112, StackTop);
    }
    else
    {
        const std::array<const PPCRegister*, 9> regs{
            &seed.r23, &seed.r24, &seed.r25, &seed.r26, &seed.r27,
            &seed.r28, &seed.r29, &seed.r30, &seed.r31};
        for (unsigned i = 0; i < regs.size(); ++i)
            run.Write64(StackTop - 80 + i * 8u, regs[i]->u64);
        run.Write32(StackTop - 8, CallerLR);
        run.Write32(Frame, StackTop);
    }
}
void Compare(const Case& test)
{
    Window first, second;
    Run original{first.bytes, test};
    Run recovered{second.bytes, test};
    Setup(original);
    std::memcpy(second.bytes, first.bytes, LowCommit);
    std::memcpy(second.bytes + GlobalPage, first.bytes + GlobalPage, 0x1000);
    PPCContext context{};
    InitContext(context, test);
    const PPCContext saved = context;
    active = &original;
    if (test.kind == Kind::Append)
        oracle_AppendPointerToVector(context, first.bytes);
    else
        oracle_ResizeFallbackAllocation(context, first.bytes);
    active = nullptr;
    Require(context.r1.u64 == StackTop && context.lr == CallerLR,
            "original stack pointer/LR restoration");
    const std::array<std::pair<const PPCRegister*, const PPCRegister*>, 9> saved_regs{{
        {&context.r23, &saved.r23}, {&context.r24, &saved.r24},
        {&context.r25, &saved.r25}, {&context.r26, &saved.r26},
        {&context.r27, &saved.r27}, {&context.r28, &saved.r28},
        {&context.r29, &saved.r29}, {&context.r30, &saved.r30},
        {&context.r31, &saved.r31}}};
    for (const auto& [actual, expected] : saved_regs)
        Require(actual->u64 == expected->u64,
                "original nonvolatile register restoration");
    PrepareSemanticFrame(recovered, saved);
    GuestMemory memory(0, {second.bytes, GuestSpace});
    Services services(recovered);
    const std::uint64_t result = test.kind == Kind::Append
        ? AppendPointerToVector(memory, services,
                                test.vector_register, test.old_register)
        : ResizeFallbackAllocation(memory, services, services,
                                   test.old_register, test.size_register,
                                   test.arg_register, ThreadSlot, Frame);
    if (result != context.r3.u64 || original.events != recovered.events)
    {
        std::fprintf(stderr,
            "return original=%016llx semantic=%016llx, events=%zu/%zu\n",
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            original.events.size(), recovered.events.size());
        for (std::size_t i = 0;
             i < original.events.size() && i < recovered.events.size(); ++i)
            if (!(original.events[i] == recovered.events[i]))
                std::fprintf(stderr, "event %zu %c/%c\n", i,
                             original.events[i].kind, recovered.events[i].kind);
        throw std::runtime_error("return/callback differential");
    }
    Require(std::memcmp(first.bytes, second.bytes, Frame - 0x100) == 0 &&
            std::memcmp(first.bytes + StackTop, second.bytes + StackTop,
                        LowCommit - StackTop) == 0 &&
            std::memcmp(first.bytes + GlobalPage, second.bytes + GlobalPage,
                        0x1000) == 0,
            "ordinary guest memory differential");
    if (test.kind == Kind::Resize)
        Require(original.Read32(Frame + 80) == recovered.Read32(Frame + 80),
                "live frame holder");
    Require(original.tries == recovered.tries &&
            original.grows == recovered.grows &&
            original.reallocs == recovered.reallocs &&
            original.releases == recovered.releases &&
            original.leaves == recovered.leaves,
            "callback counts");
}
} // namespace

PPC_FUNC(sub_827C5050)
{
    oracle_AppendPointerToVector(ctx, base);
}
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
PPC_FUNC(sub_827C4FA0)
{
    ctx.r3.u64 = active->Grow(ctx.r3.u64, ctx.r4.u64);
}
void oracle_Indirect(PPCContext& ctx, std::uint8_t*,
                     std::uint32_t method)
{
    const GuestAddress receiver = ctx.r3.u32;
    if (method == FirstMethod || method == SecondMethod)
        ctx.r3.u64 = active->Realloc(method, receiver, ctx.r4.u64,
                                    ctx.r5.u64, ctx.r6.u64);
    else
        ctx.r3.u64 = active->Release(method, receiver, ctx.r4.u64);
}

int main()
{
    try
    {
        unsigned direct = 0, resize = 0, composed = 0;
        const auto run = [&](Case scenario)
        {
            try { Compare(scenario); }
            catch (...)
            {
                std::fprintf(stderr, "case %s\n", scenario.name);
                throw;
            }
            if (scenario.kind == Kind::Append) ++direct;
            else
            {
                ++resize;
                if (scenario.path == Path::FreeClass ||
                    scenario.path == Path::MoveManager ||
                    (scenario.path == Path::MoveClass &&
                     static_cast<GuestAddress>(scenario.size_register) != 500u))
                    ++composed;
            }
        };
        for (std::uint32_t count : {0u, 2u, 4u, 0x80000000u})
        {
            Case scenario{Kind::Append, Path::DirectAppend, "direct vector append"};
            scenario.vector_register = 0xaabbccdd00000000ull | (Pool + 20u);
            scenario.old_register = 0x1122334400000000ull | Old;
            scenario.vector_count = count;
            scenario.vector_capacity = count == 0x80000000u ? 0x7fffffffu :
                                       count == 4 ? 4u : count == 2 ? 3u : 8u;
            scenario.grow_mutates = count == 4;
            run(scenario);
        }
        for (Path path : {Path::NoPool, Path::NewZero, Path::NewPop,
                          Path::NewEmpty, Path::Untracked, Path::FreeClass,
                          Path::FreeManager, Path::MoveClass,
                          Path::MoveManager, Path::MoveEmpty,
                          Path::Oversize})
            for (unsigned variant = 0; variant < 3; ++variant)
            {
                Case scenario{Kind::Resize, path, "fallback resize"};
                scenario.try_failures = variant;
                scenario.mutate_globals = variant == 2;
                scenario.mutate_holder = variant == 1;
                scenario.old_register = 0x1122334400000000ull | Old;
                scenario.size_register = 24;
                if (path == Path::NewZero || path == Path::NewPop ||
                    path == Path::NewEmpty || path == Path::NoPool)
                    scenario.old_register = 0;
                if (path == Path::NewZero || path == Path::FreeClass ||
                    path == Path::FreeManager)
                    scenario.size_register = 0;
                if (path == Path::NewEmpty || path == Path::MoveEmpty)
                    scenario.class_count = 0;
                if (path == Path::FreeManager || path == Path::MoveManager)
                    scenario.old_size = 2048;
                if (path == Path::Oversize)
                    scenario.size_register = 600;
                if (variant == 2 && scenario.size_register != 0)
                    scenario.size_register |= 0x1234567800000000ull;
                if (path == Path::FreeClass || path == Path::MoveClass)
                {
                    scenario.vector_capacity = variant == 0 ? 0u : 4u;
                    scenario.grow_mutates = variant == 0;
                }
                run(scenario);
            }
        Case farther_new{Kind::Resize, Path::NewPop, "third size class pop"};
        farther_new.old_register = 0;
        farther_new.size_register = 100;
        run(farther_new);
        Case farther_move{Kind::Resize, Path::MoveClass, "third size class move"};
        farther_move.size_register = 100;
        farther_move.vector_capacity = 4;
        run(farther_move);
        Case sixth_new{Kind::Resize, Path::NewPop,
                       "sixth-class-only request falls back"};
        sixth_new.old_register = 0;
        sixth_new.size_register = 500;
        run(sixth_new);
        Case sixth_move{Kind::Resize, Path::MoveClass,
                        "sixth-class-only move falls back"};
        sixth_move.size_register = 500;
        run(sixth_move);
        Case sixth_free{Kind::Resize, Path::FreeClass,
                        "sixth-class old size returns to base pool"};
        sixth_free.old_size = 600;
        sixth_free.size_register = 0;
        sixth_free.vector_capacity = 4;
        run(sixth_free);
        Case sixth_recycle{Kind::Resize, Path::MoveClass,
                           "pop and recycle sixth-class old size"};
        sixth_recycle.old_size = 600;
        sixth_recycle.size_register = 24;
        sixth_recycle.vector_capacity = 4;
        run(sixth_recycle);
        std::printf("PASS 827C5050 %u\n", direct);
        std::printf("PASS 82295530 %u\n", resize);
        std::printf("PASS fallback-resize-composed %u\n", composed);
        std::puts("LIMIT: generated PPC C++ and bounded ordinary memory; "
                  "virtual/kernel/grow callbacks are synthetic, ABI saves "
                  "are explicit test adapter writes");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
