#include "lo_semantics/heap_reallocate.h"
#include "lo_semantics/heap.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/memory_move.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::heap_reallocate;
constexpr GuestAddress Heap = 0x10000u, Block = 0x20000u;
constexpr GuestAddress Payload = Block + 16u;
constexpr GuestAddress NewBlock = 0x40000u;
constexpr GuestAddress Segment = 0x30000u;
constexpr std::size_t Space = 0x400000u;
constexpr std::uint64_t Stack = 0x1122334400380000ull;
enum class Mode { Null, Limit, Guard, Invalid, NoSplit, Split,
    Zero, Lock, Grow, GrowNoMove, Move };
constexpr std::array Cases{Mode::Null, Mode::Limit, Mode::Guard, Mode::Invalid,
    Mode::NoSplit, Mode::Split, Mode::Zero, Mode::Lock, Mode::Grow,
    Mode::GrowNoMove, Mode::Move};
struct Event
{
    int kind;
    std::uint64_t a, b, c;
    bool operator==(const Event&) const = default;
};

struct TestServices final : family::Services
{
    struct AllocationService final : HeapAllocateServices
    {
        std::uint32_t GetCurrentProcessType() override { throw std::runtime_error("unexpected allocation process"); }
        void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
            std::uint32_t, std::uint32_t) override { throw std::runtime_error("unexpected allocation bugcheck"); }
        void EnterCriticalSection(GuestAddress) override { throw std::runtime_error("unexpected allocation lock"); }
        void LeaveCriticalSection(GuestAddress) override { throw std::runtime_error("unexpected allocation unlock"); }
        GuestAddress GrowHeap(GuestAddress, std::uint32_t) override { return 0; }
        std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
            std::uint32_t, std::uint32_t, std::uint32_t) override { return -1; }
        void RaiseException(GuestAddress) override { throw std::runtime_error("unexpected allocation exception"); }
    } allocation;
    struct FreeService final : HeapFreeServices
    {
        std::uint32_t CompareMemoryUlong(GuestAddress, std::uint32_t,
            std::uint32_t) override { return 0; }
        std::uint32_t GetCurrentProcessType() override { throw std::runtime_error("unexpected free process"); }
        void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
            std::uint32_t, GuestAddress) override { throw std::runtime_error("unexpected free bugcheck"); }
        void EnterCriticalSection(GuestAddress) override { throw std::runtime_error("unexpected free lock"); }
        void LeaveCriticalSection(GuestAddress) override { throw std::runtime_error("unexpected free unlock"); }
        std::int32_t FreeVirtualMemory(GuestAddress, GuestAddress,
            std::uint32_t, std::uint32_t) override { throw std::runtime_error("unexpected free VM"); }
        void DecommitFreeBlock(GuestAddress, GuestAddress,
            std::uint32_t) override { throw std::runtime_error("unexpected free decommit"); }
    } free;
    struct SegmentService final : HeapSegmentServices
    {
        std::int32_t CommitRange(GuestAddress, GuestAddress,
            GuestAddress, GuestAddress) override { throw std::runtime_error("unexpected segment commit"); }
        std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
            std::uint32_t, std::uint32_t, std::uint32_t) override { throw std::runtime_error("unexpected segment VM"); }
        void InitializeUncommittedRange(GuestAddress, GuestAddress,
            std::uint32_t) override { throw std::runtime_error("unexpected segment bookkeeping"); }
    } segment;
    struct ResizeService final : heap_block_resize::NativeServices
    {
        std::uint64_t CompareMemoryUlong(GuestMemory&, GuestAddress,
            std::uint32_t, std::uint32_t, std::uint64_t,
            heap_block_resize::FrameRegisters&) override
        { throw std::runtime_error("unexpected resize compare"); }
    } resize;
    struct ExitService final : heap_lock_exit::NativeServices
    {
        TestServices& owner;
        explicit ExitService(TestServices& s) : owner(s) {}
        void LeaveCriticalSection(GuestMemory&, heap_lock_exit::Registers& r) override
        { owner.events.push_back({3, r.r3, r.sp, r.lr}); r.r3 = 0; }
    } exit{*this};
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    TestServices(std::vector<std::uint8_t>& bytes, Mode test)
        : memory(0, std::span<std::uint8_t>(bytes)), mode(test) {}
    HeapAllocateServices& Allocation() override { return allocation; }
    HeapFreeServices& Free() override { return free; }
    HeapSegmentServices& Segment() override { return segment; }
    heap_block_resize::NativeServices& ResizeNative() override { return resize; }
    heap_lock_exit::NativeServices& LockExitNative() override { return exit; }
    void GetCurrentProcessType(GuestMemory&, family::Registers& r) override
    { events.push_back({1, r.sp, r.lr, 0}); r.r3 = 3; }
    void BugCheck(GuestMemory&, family::Registers& r) override
    { events.push_back({2, r.r3, r.r6, r.r7}); r.r3 = 0; }
    void EnterCriticalSection(GuestMemory&, family::Registers& r) override
    {
        events.push_back({4, r.r3, r.sp, r.lr});
        r.r3 = 0;
        if (mode == Mode::Split) r.r21 = 3;
    }
    void FreeVirtualMemory(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected VM free"); }
    void CompareMemoryUlong(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected free-fill compare"); }
    void RaiseException(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected parent exception"); }
};
TestServices* active = nullptr;

void Seed(std::vector<std::uint8_t>& bytes, Mode mode)
{
    std::fill(bytes.begin(), bytes.end(), 0xbdu);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes));
    memory.WriteU32(Heap + 20u, mode == Mode::Guard ? 0x40000u : 0u);
    memory.WriteU8(Heap + 379u, 4u);
    memory.WriteU32(Heap + 24u, 0u);
    memory.WriteU32(Heap + 80u, 15u);
    memory.WriteU32(Heap + 84u, 0xfffffff0u);
    memory.WriteU32(Heap + 380u, 0u);
    memory.WriteU32(Heap + 96u, Segment);
    memory.WriteU32(Heap + 1408u, 0x12345678u);
    memory.WriteU32(Heap + 48u, 100u);
    memory.WriteU32(Segment + 64u, Block);
    for (std::uint32_t units = 1; units <= 10; ++units)
    {
        const auto head = Heap + (units + 48u) * 8u;
        memory.WriteU32(head, head);
        memory.WriteU32(head + 4u, head);
    }
    memory.WriteU32(Heap + 384u, Heap + 384u);
    memory.WriteU32(Heap + 388u, Heap + 384u);
    const auto old_units = mode == Mode::Split || mode == Mode::Zero ? 8u : 4u;
    memory.WriteU16(Block, static_cast<std::uint16_t>(old_units));
    memory.WriteU8(Block + 4u, 0u);
    memory.WriteU8(Block + 5u, mode == Mode::Invalid ? 0u : 1u);
    memory.WriteU8(Block + 6u, mode == Mode::Zero ? 64u : 0u);
    memory.WriteU8(Block + 7u, 0u);
    memory.WriteU16(Block + 2u, 0u);
    const auto next = Block + old_units * 16u;
    if (mode == Mode::Grow)
    {
        const auto units = 4u;
        const auto head = Heap + (units + 48u) * 8u;
        const auto link = next + 8u;
        memory.WriteU16(next, static_cast<std::uint16_t>(units));
        memory.WriteU8(next + 5u, 0u);
        memory.WriteU32(head, link);
        memory.WriteU32(head + 4u, link);
        memory.WriteU32(link, head);
        memory.WriteU32(link + 4u, head);
        const auto bitmap = Heap + ((units >> 5u) + 88u) * 4u;
        memory.WriteU32(bitmap, memory.ReadU32(bitmap) | (1u << (units & 31u)));
        memory.WriteU8(next + units * 16u + 5u, 1u);
    }
    else memory.WriteU8(next + 5u, 1u);
    if (mode == Mode::Move)
    {
        const auto units = 6u;
        const auto head = Heap + (units + 48u) * 8u;
        const auto link = NewBlock + 8u;
        memory.WriteU16(NewBlock, static_cast<std::uint16_t>(units));
        memory.WriteU8(NewBlock + 5u, 0u);
        memory.WriteU32(head, link);
        memory.WriteU32(head + 4u, link);
        memory.WriteU32(link, head);
        memory.WriteU32(link + 4u, head);
        const auto bitmap = Heap + ((units >> 5u) + 88u) * 4u;
        memory.WriteU32(bitmap, memory.ReadU32(bitmap) | (1u << (units & 31u)));
        for (unsigned i = 0; i != 64; ++i)
            memory.WriteU8(Payload + i, static_cast<std::uint8_t>(i ^ 0x5au));
    }
}

std::array<PPCRegister*, 13> Nonvolatile(PPCContext& c)
{ return {&c.r19, &c.r20, &c.r21, &c.r22, &c.r23, &c.r24, &c.r25,
    &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31}; }

family::Registers FromPpc(PPCContext& c)
{
    family::Registers r{};
    r.sp=c.r1.u64; r.lr=c.lr; r.ctr=c.ctr.u64;
    r.r3=c.r3.u64; r.r4=c.r4.u64; r.r5=c.r5.u64; r.r6=c.r6.u64;
    r.r7=c.r7.u64; r.r8=c.r8.u64; r.r9=c.r9.u64; r.r10=c.r10.u64;
    r.r11=c.r11.u64; r.r12=c.r12.u64; r.r13=c.r13.u64;
    auto regs=Nonvolatile(c);
    std::array<std::uint64_t*,13> values{&r.r19,&r.r20,&r.r21,&r.r22,&r.r23,
        &r.r24,&r.r25,&r.r26,&r.r27,&r.r28,&r.r29,&r.r30,&r.r31};
    for (std::size_t i=0;i<regs.size();++i) *values[i]=regs[i]->u64;
    r.xer_so=c.xer.so;
    r.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    r.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return r;
}

bool Check(Mode mode)
{
    std::vector<std::uint8_t> before(Space), after(Space);
    Seed(before, mode); after = before;
    TestServices expected(before, mode), actual(after, mode);
    PPCContext raw{};
    raw.r1.u64=Stack; raw.lr=0x123456789abcdef0ull;
    raw.r3.u64=0x6677889900010000ull;
    raw.r4.u64=(mode==Mode::Lock || mode==Mode::Split) ? 0u :
        (mode==Mode::GrowNoMove ? 0x11u :
        mode==Mode::Zero || mode==Mode::Move ? 9u : 1u);
    raw.r5.u64=mode==Mode::Null ? 0u : 0x778899aa00020010ull;
    raw.r6.u64=mode==Mode::Limit ? 0x80000000u :
        mode==Mode::NoSplit ? 40u : mode==Mode::Split ? 32u :
        mode==Mode::Zero ? 100u :
        mode==Mode::Grow || mode==Mode::GrowNoMove || mode==Mode::Move ? 80u : 64u;
    raw.xer.so=1;
    auto preserved=Nonvolatile(raw);
    for (std::size_t i=0;i<preserved.size();++i)
        preserved[i]->u64=0x1122334400000100ull+i;
    family::Registers recovered=FromPpc(raw);
    active=&expected;
    __imp__sub_827CCF80(raw, before.data());
    if (!family::Apply(0x827ccf80u, actual.memory, actual, recovered))
        throw std::runtime_error("heap reallocate entry missing");
    bool same=raw.r3.u64==recovered.r3 && raw.r1.u64==recovered.sp &&
        raw.lr==recovered.lr && before==after && expected.events==actual.events;
    const auto saved=Nonvolatile(raw);
    const std::array<std::uint64_t,13> actual_regs{recovered.r19,recovered.r20,
        recovered.r21,recovered.r22,recovered.r23,recovered.r24,recovered.r25,
        recovered.r26,recovered.r27,recovered.r28,recovered.r29,recovered.r30,
        recovered.r31};
    for (std::size_t i=0;i<saved.size();++i)
        same &= saved[i]->u64==actual_regs[i];
    if (mode == Mode::Move)
    {
        same &= raw.r3.u32 == NewBlock + 16u;
        for (unsigned i = 0; i != 64; ++i)
            same &= after[NewBlock + 16u + i] == static_cast<std::uint8_t>(i ^ 0x5au);
        for (unsigned i = 64; i != 80; ++i)
            same &= after[NewBlock + 16u + i] == 0u;
    }
    if (mode == Mode::Null || mode == Mode::Limit)
    {
        same &= raw.r10.u64 == recovered.r10 &&
            raw.r11.u64 == recovered.r11 &&
            raw.cr0.lt == recovered.cr0.lt &&
            raw.cr0.gt == recovered.cr0.gt &&
            raw.cr0.eq == recovered.cr0.eq &&
            raw.cr0.so == recovered.cr0.so &&
            raw.cr6.lt == recovered.cr6.lt &&
            raw.cr6.gt == recovered.cr6.gt &&
            raw.cr6.eq == recovered.cr6.eq &&
            raw.cr6.so == recovered.cr6.so;
    }
    if (!same)
    {
        std::size_t difference=0;
        while (difference<Space && before[difference]==after[difference]) ++difference;
        std::fprintf(stderr,"FAIL heap-reallocate mode=%d raw-r3=%llx model-r3=%llx ram-diff=%zx events=%zu/%zu\n",
            static_cast<int>(mode), static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(recovered.r3), difference,
            expected.events.size(), actual.events.size());
    }
    return same;
}
} // namespace

void OriginalGetProcess(PPCContext& c, std::uint8_t*)
{ active->events.push_back({1,c.r1.u64,c.lr,0}); c.r3.u64=3; }
void OriginalBugCheck(PPCContext& c, std::uint8_t*)
{ active->events.push_back({2,c.r3.u64,c.r6.u64,c.r7.u64}); c.r3.u64=0; }
void OriginalEnter(PPCContext& c, std::uint8_t*)
{
    active->events.push_back({4,c.r3.u64,c.r1.u64,c.lr});
    c.r3.u64=0;
    if (active->mode == Mode::Split) c.r21.u64=3;
}
void OriginalLeave(PPCContext& c, std::uint8_t*)
{ active->events.push_back({3,c.r3.u64,c.r1.u64,c.lr}); c.r3.u64=0; }
void OriginalFreeVM(PPCContext&,std::uint8_t*)
{ throw std::runtime_error("unexpected original VM free"); }
void OriginalCompare(PPCContext&,std::uint8_t*)
{ throw std::runtime_error("unexpected original compare"); }
void OriginalRaise(PPCContext&,std::uint8_t*)
{ throw std::runtime_error("unexpected original raise"); }
void OriginalExtend(PPCContext& c,std::uint8_t*)
{ c.r3.u64=ExtendHeapSegment(active->memory,active->segment,c.r3.u32,
    c.r4.u32,c.r5.u32,c.r6.u32,c.r1.u32-160u); }
void OriginalCoalesce(PPCContext& c,std::uint8_t*)
{ c.r3.u64=CoalesceFreeBlocks(active->memory,active->free,c.r3.u32,
    c.r4.u32,c.r5.u32,c.r6.u32); }
void OriginalInsert(PPCContext& c,std::uint8_t*)
{ InsertFreeBlocks(active->memory,c.r3.u32,c.r4.u32,c.r5.u32); }
void OriginalFill(PPCContext& c,std::uint8_t*)
{ c.r3.u64=FillGuestMemory(active->memory,c.r3.u32,c.r4.u32,c.r5.u32); }
void OriginalMove(PPCContext& c,std::uint8_t*)
{ c.r3.u64=MoveGuestMemory(active->memory,c.r3.u64,c.r4.u32,c.r5.u64,c.r1.u32); }
void OriginalSaveLower(PPCContext& c, unsigned first, unsigned frame_bytes)
{
    const auto saved=Nonvolatile(c);
    for (unsigned reg=first;reg<=31;++reg)
    {
        const auto address=c.r1.u32-8u*(33u-reg);
        const auto value=saved[reg-19u]->u64;
        active->memory.WriteU32(address,static_cast<std::uint32_t>(value>>32));
        active->memory.WriteU32(address+4u,static_cast<std::uint32_t>(value));
    }
    active->memory.WriteU32(c.r1.u32-8u,static_cast<std::uint32_t>(c.lr));
    active->memory.WriteU32(c.r1.u32-frame_bytes,c.r1.u32);
}
void OriginalRestoreLower(PPCContext& c, unsigned first)
{
    const auto saved=Nonvolatile(c);
    for (unsigned reg=first;reg<=31;++reg)
    {
        const auto address=c.r1.u32-8u*(33u-reg);
        saved[reg-19u]->u64=(std::uint64_t{active->memory.ReadU32(address)}<<32)|
            active->memory.ReadU32(address+4u);
    }
    c.r12.u64=active->memory.ReadU32(c.r1.u32-8u);
    c.lr=c.r12.u64;
}
void OriginalAllocate(PPCContext& c,std::uint8_t*)
{
    OriginalSaveLower(c,22,320);
    c.r3.u64=AllocateHeapBlock(active->memory,active->allocation,c.r3.u32,
        c.r4.u32,c.r5.u32,c.r1.u32-320u);
    OriginalRestoreLower(c,22);
}
void OriginalFree(PPCContext& c,std::uint8_t*)
{
    OriginalSaveLower(c,25,176);
    c.r3.u64=FreeHeapBlock(active->memory,active->free,c.r3.u32,
        c.r4.u32,c.r5.u32,c.r1.u32-176u);
    OriginalRestoreLower(c,25);
}

int main()
{
    try
    {
        for (Mode mode:Cases) if (!Check(mode)) return 1;
        std::vector<std::uint8_t> bytes(128);
        TestServices services(bytes, Mode::Null);
        family::Registers registers{}; registers.r3=123;
        if (family::Apply(0xffffffffu,services.memory,services,registers) ||
            registers.r3!=123u) throw std::runtime_error("unknown address changed state");
        std::printf("PASS heap-reallocate %zu original PPC cases + unknown\n",Cases.size());
        std::puts("LIMIT ordinary RAM, selected parent frame/return; native behavior explicit and lower generic ABI/runtime unverified");
        return 0;
    }
    catch (const std::exception& e)
    { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
