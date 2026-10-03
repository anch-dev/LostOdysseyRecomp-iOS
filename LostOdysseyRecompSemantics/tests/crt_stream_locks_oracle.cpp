// Appended after the four pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_stream_locks.h"
#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_services.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::crt_stream_locks;
using test::GuestWindow;
using test::Region;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress LockTable = 0x83215358u;
constexpr GuestAddress HeapGlobal = 0x83245708u;
constexpr GuestAddress FatalGlobal = 0x83214d70u;
constexpr GuestAddress OnceTarget = 0x832d3ca8u;
constexpr GuestAddress StreamBlocks = 0x83378d80u;
constexpr GuestAddress StreamBlock = 0x30000u;
constexpr GuestAddress OuterAllocation = 0x50000u;
constexpr GuestAddress InnerAllocation = 0x50100u;
constexpr std::array<Region, 5> Regions{{
    {0, 0x90000u}, {0x83214000u, 0x3000u},
    {0x83245000u, 0x2000u}, {0x832d3000u, 0x2000u},
    {0x83378000u, 0x3000u}
}};

enum class Mode
{
    ExistingLock, LazyGlobal, AllocationFailure, InitializationFailure,
    RecursiveLockTen, FatalReturning, RaceCleanup,
    StreamExisting, StreamLazy, StreamInitFailure, StreamAlias
};
struct Case { GuestAddress address; Mode mode; std::uint64_t input; };
constexpr Case Cases[] = {
    {0x82b81b28u, Mode::ExistingLock, 0xaabbccdd00000003ull},
    {0x82b819e8u, Mode::LazyGlobal, 0xaabbccdd00000003ull},
    {0x82b819e8u, Mode::AllocationFailure, 0xaabbccdd00000003ull},
    {0x82b819e8u, Mode::InitializationFailure, 0xaabbccdd00000003ull},
    {0x82b81b28u, Mode::RecursiveLockTen, 0xaabbccdd00000003ull},
    {0x82b81b28u, Mode::FatalReturning, 0xaabbccdd00000003ull},
    {0x82b819e8u, Mode::RaceCleanup, 0xaabbccdd00000003ull},
    {0x82b7bed8u, Mode::FatalReturning, 0x1122334400000011ull},
    {0x82b862f8u, Mode::StreamExisting, 0xaabbccdd00000005ull},
    {0x82b862f8u, Mode::StreamLazy, 0xaabbccdd00000005ull},
    {0x82b862f8u, Mode::StreamInitFailure, 0xaabbccdd00000005ull},
    {0x82b862f8u, Mode::StreamAlias, 0xaabbccdd00000005ull},
};

using Event = std::array<std::uint64_t, 7>;
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices, family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned allocations = 0;
    Services(GuestWindow& window, Mode selected)
        : memory(window.Memory()), mode(selected) {}

    [[noreturn]] static void Unexpected()
    { throw std::runtime_error("unexpected accepted lower boundary"); }

    // 823ACBD0 raw allocation's remaining heap import.
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint64_t bytes) override
    {
        const auto ordinal = allocations++;
        events.push_back({1, heap, flags, bytes, ordinal});
        if (mode == Mode::AllocationFailure || mode == Mode::FatalReturning)
            return 0;
        return 0xaabbccdd00050000ull;
    }
    void EnterMissingHeapPath() override { Unexpected(); }
    void ReportMissingHeap(std::uint32_t) override { Unexpected(); }
    void TerminateMissingHeap(std::uint32_t) override { Unexpected(); }
    std::int32_t RetryAllocation(std::uint64_t) override
    { Unexpected(); }
    GuestAddress GetErrorAddress() override
    { return 0x83215210u; }

    // 82B7FD78 and the accepted CRT allocation/free models.
    std::uint64_t GetThreadData() override
    { events.push_back({2}); return 0; }
    std::uint64_t OutputErrorMessage(GuestAddress message) override
    { events.push_back({3, message}); return message; }
    std::uint64_t BugCheck(std::uint32_t code) override
    { events.push_back({4, code}); return 0x9988776600000000ull; }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { Unexpected(); }
    void ReportInvalidParameter() override { Unexpected(); }
    std::uint32_t GetCurrentProcessType() override { Unexpected(); }
    void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override { Unexpected(); }
    void EnterCriticalSection(GuestAddress address) override
    { events.push_back({5, address}); }
    void LeaveCriticalSection(GuestAddress address) override
    { events.push_back({6, address}); }
    GuestAddress GrowHeap(GuestAddress, std::uint32_t) override
    { Unexpected(); }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override
    { Unexpected(); }
    void RaiseException(GuestAddress) override { Unexpected(); }
    std::int32_t FreeVirtualMemory(GuestAddress base_out,
        GuestAddress size_out, std::uint32_t type,
        std::uint32_t zero) override
    {
        events.push_back({7, base_out, size_out, type, zero});
        return 0;
    }
    void DecommitFreeBlock(GuestAddress, GuestAddress,
        std::uint32_t) override { Unexpected(); }
    std::uint32_t CompareMemoryUlong(GuestAddress,
        std::uint32_t, std::uint32_t) override { Unexpected(); }

    // These are unused by this focused closure but required by lower APIs.
    std::uint64_t GetTlsValue(std::uint32_t) override { Unexpected(); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override { Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress,
        std::uint64_t) override { Unexpected(); }
    std::uint64_t AllocateThreadData(std::uint32_t,
        std::uint32_t) override { Unexpected(); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override { Unexpected(); }
    void FreeThreadData(std::uint64_t) override { Unexpected(); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override { Unexpected(); }
    void Trap(const InvalidParameterCall&) override { Unexpected(); }

    std::uint64_t InitializeCriticalSection(GuestMemory&,
        InvalidParameterCall& call,
        crt_stream_state::FrameRegisters& frame) override
    {
        events.push_back({8, frame.sp, frame.lr, frame.r31,
            call.arguments[0], call.arguments[1]});
        return 0xaabbccdd00000001ull;
    }
    std::uint64_t CallIndirect(GuestAddress target, GuestMemory&,
        InvalidParameterCall& call,
        crt_stream_state::FrameRegisters& frame) override
    {
        events.push_back({9, target, frame.sp, frame.lr,
            call.arguments[0], call.arguments[1]});
        if (target != 0x7100u) Unexpected();
        return 0;
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_pointer_unlock::Registers& call) override
    {
        events.push_back({10, call.sp, call.lr, call.r3,
            call.r30, call.r31});
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers& call) override
    { events.push_back({11, call.sp, call.lr, call.r3, call.r31}); }
    void EnterCriticalSection(GuestMemory&,
        family::Registers& call) override
    {
        events.push_back({12, call.sp, call.lr, call.r3,
            call.r29, call.r30, call.r31});
        if (mode == Mode::RaceCleanup &&
            call.r3 == memory.ReadU32(LockTable + 10u * 8u))
            memory.WriteU32(LockTable + 3u * 8u, 0x70000u);
        call.r3 = 0x8877665500000042ull;
        if (mode == Mode::StreamAlias)
        {
            // The final epilogue uses live r31. A redirected save area and
            // full-width SP expose both the alias and the restore order.
            call.r31 = Stack - 0x180u;
            call.sp = 0xaabbccdd00000100ull;
        }
    }
    void CallFatal(GuestAddress target, GuestMemory&,
        family::Registers& call) override
    {
        events.push_back({13, target, call.sp, call.lr,
            call.r3, call.r31});
        if (target != 0x7100u || call.r3 != 255 ||
            call.lr != 0x82b7bf0cu) Unexpected();
        call.r3 = 0x12345678000000ffull;
    }
};

Services* active = nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64; s.lr=c.lr; s.ctr=c.ctr.u64;
    s.r3=c.r3.u64; s.r4=c.r4.u64; s.r5=c.r5.u64;
    s.r6=c.r6.u64; s.r7=c.r7.u64; s.r8=c.r8.u64;
    s.r9=c.r9.u64; s.r10=c.r10.u64; s.r11=c.r11.u64;
    s.r12=c.r12.u64; s.r13=c.r13.u64;
    s.r28=c.r28.u64; s.r29=c.r29.u64;
    s.r30=c.r30.u64; s.r31=c.r31.u64;
    s.cr0={c.cr0.lt!=0,c.cr0.gt!=0,c.cr0.eq!=0,c.cr0.so!=0};
    s.cr6={c.cr6.lt!=0,c.cr6.gt!=0,c.cr6.eq!=0,c.cr6.so!=0};
    s.xer_ca=c.xer.ca; s.xer_so=c.xer.so;
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64=s.sp; c.lr=s.lr; c.ctr.u64=s.ctr;
    c.r3.u64=s.r3; c.r4.u64=s.r4; c.r5.u64=s.r5;
    c.r6.u64=s.r6; c.r7.u64=s.r7; c.r8.u64=s.r8;
    c.r9.u64=s.r9; c.r10.u64=s.r10; c.r11.u64=s.r11;
    c.r12.u64=s.r12; c.r13.u64=s.r13;
    c.r28.u64=s.r28; c.r29.u64=s.r29;
    c.r30.u64=s.r30; c.r31.u64=s.r31;
    c.cr0.lt=s.cr0.lt; c.cr0.gt=s.cr0.gt;
    c.cr0.eq=s.cr0.eq; c.cr0.so=s.cr0.so;
    c.cr6.lt=s.cr6.lt; c.cr6.gt=s.cr6.gt;
    c.cr6.eq=s.cr6.eq; c.cr6.so=s.cr6.so;
    c.xer.ca=s.xer_ca; c.xer.so=s.xer_so;
}

family::Dependencies Dependencies(Services& services)
{
    return {services, services, services, services, services,
        services, services, services};
}

void SeedHeapBlock(GuestMemory& memory, GuestAddress payload)
{
    const auto block = payload - 16u;
    const auto vm_base = block - 32u;
    memory.WriteU8(block + 5u, 8u);
    memory.WriteU32(vm_base, vm_base);
    memory.WriteU32(vm_base + 4u, vm_base);
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(HeapGlobal, 0x10000u);
    memory.WriteU32(0x10000u + 24u, 1u); // no heap lock during cleanup
    memory.WriteU32(FatalGlobal, 0x7103u);
    memory.WriteU32(StreamBlocks, StreamBlock);
    memory.WriteU32(0x832d3aecu, 0);
    memory.WriteU32(OnceTarget,
        mode == Mode::InitializationFailure ||
        mode == Mode::StreamInitFailure ? 0x7100u : 0);
    memory.WriteU32(LockTable + 10u * 8u, 0x60000u);
    memory.WriteU32(LockTable + 3u * 8u,
        mode == Mode::ExistingLock ? 0x61000u : 0);
    const bool stream_existing = mode == Mode::StreamExisting ||
        mode == Mode::StreamAlias;
    memory.WriteU32(StreamBlock + 5u * 64u + 8u,
        stream_existing ? 1u : 0u);
    SeedHeapBlock(memory, OuterAllocation);
    SeedHeapBlock(memory, InnerAllocation);
    // Alternate save slots for the live-r31 stream epilogue case.
    WriteU64(memory, Address(Stack - 0x100u - 32u),
        0x1122334400000029ull);
    WriteU64(memory, Address(Stack - 0x100u - 24u),
        0x1122334400000030ull);
    WriteU64(memory, Address(Stack - 0x100u - 16u),
        0x1122334400000031ull);
    memory.WriteU32(Address(Stack - 0x100u - 8u), 0x87654321u);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=Stack; c.lr=0x1234567887654321ull;
    c.r3.u64=item.input; c.r4.u64=0xaabbccdd00000400ull;
    c.r8.u64=0x1122334400000008ull;
    c.r9.u64=0x1122334400000009ull;
    c.r10.u64=0x112233440000000aull;
    c.r11.u64=0x112233440000000bull;
    c.r12.u64=0x112233440000000cull;
    c.r13.u64=0x1122334400009000ull;
    c.r28.u64=0x1122334400000028ull;
    c.r29.u64=0x1122334400000029ull;
    c.r30.u64=0x1122334400000030ull;
    c.r31.u64=0x1122334400000031ull;
    c.xer.so=1; c.xer.ca=1;
    return c;
}

unsigned CountEvent(const Services& services, std::uint64_t kind)
{
    unsigned count=0;
    for (const auto& event : services.events)
        if (event[0] == kind) ++count;
    return count;
}

bool ExpectedPath(const Case& item, const Services& services)
{
    const auto memory=services.memory;
    switch (item.mode)
    {
    case Mode::ExistingLock:
        return CountEvent(services,1)==0 && CountEvent(services,12)==1;
    case Mode::LazyGlobal:
        return CountEvent(services,1)==1 && CountEvent(services,8)==1 &&
            CountEvent(services,10)==1 &&
            memory.ReadU32(LockTable+3u*8u)==OuterAllocation;
    case Mode::AllocationFailure:
        return CountEvent(services,1)==1 && CountEvent(services,2)==1 &&
            memory.ReadU32(0x83215210u)==12;
    case Mode::InitializationFailure:
        return CountEvent(services,9)==1 && CountEvent(services,7)==1 &&
            memory.ReadU32(LockTable+3u*8u)==0 &&
            memory.ReadU32(0x83215210u)==12;
    case Mode::RecursiveLockTen:
        return CountEvent(services,1)==1 && CountEvent(services,12)==2 &&
            CountEvent(services,10)==1;
    case Mode::FatalReturning:
        return CountEvent(services,13)==1 &&
            (item.address==0x82b7bed8u || CountEvent(services,12)==1);
    case Mode::RaceCleanup:
        return CountEvent(services,7)==1 && CountEvent(services,8)==0 &&
            memory.ReadU32(LockTable+3u*8u)==0x70000u;
    case Mode::StreamExisting:
        return CountEvent(services,11)==0 && CountEvent(services,12)==1;
    case Mode::StreamLazy:
        return CountEvent(services,8)==1 && CountEvent(services,11)==1 &&
            memory.ReadU32(StreamBlock+5u*64u+8u)==1;
    case Mode::StreamInitFailure:
        return CountEvent(services,9)==1 && CountEvent(services,11)==1 &&
            memory.ReadU32(StreamBlock+5u*64u+8u)==1;
    case Mode::StreamAlias:
        return CountEvent(services,12)==1;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, item.mode); Seed(recovered, item.mode);
    Services expected(original,item.mode), actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    switch (item.address)
    {
    case 0x82b81b28u: __imp__sub_82B81B28(raw, original.Bytes()); break;
    case 0x82b819e8u: __imp__sub_82B819E8(raw, original.Bytes()); break;
    case 0x82b7bed8u: __imp__sub_82B7BED8(raw, original.Bytes()); break;
    case 0x82b862f8u: __imp__sub_82B862F8(raw, original.Bytes()); break;
    default: throw std::runtime_error("unknown selected entry");
    }
    active=nullptr;
    if (!ExpectedPath(item,expected))
    {
        std::fprintf(stderr,
            "fixture path %08X mode=%u allocations=%u "
            "events[1,2,7,8,9,10,11,12,13]=%u,%u,%u,%u,%u,%u,%u,%u,%u "
            "lock3=%08X errno=%08X stream_count=%u\n",
            item.address,static_cast<unsigned>(item.mode),expected.allocations,
            CountEvent(expected,1),CountEvent(expected,2),
            CountEvent(expected,7),CountEvent(expected,8),
            CountEvent(expected,9),CountEvent(expected,10),
            CountEvent(expected,11),CountEvent(expected,12),
            CountEvent(expected,13),
            expected.memory.ReadU32(LockTable+3u*8u),
            expected.memory.ReadU32(0x83215210u),
            expected.memory.ReadU32(StreamBlock+5u*64u+8u));
        throw std::runtime_error("fixture missed its selected PPC path");
    }
    if (!family::Apply(item.address,actual.memory,
        Dependencies(actual),state))
        throw std::runtime_error("recovered entry missing");
    const auto original_state=FromPpc(raw);
    const bool same=original_state==state &&
        expected.events==actual.events &&
        original.EqualCommitted(recovered);
    if (!same)
        std::fprintf(stderr,"FAIL crt-stream-locks %08X mode=%u "
            "r3=%llx/%llx sp=%llx/%llx lr=%llx/%llx "
            "r31=%llx/%llx events=%zu/%zu RAM=%u\n",
            item.address,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(original_state.r3),
            static_cast<unsigned long long>(state.r3),
            static_cast<unsigned long long>(original_state.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(original_state.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(original_state.r31),
            static_cast<unsigned long long>(state.r31),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
    return same;
}
} // namespace

// Adapters for the pinned PPC bodies. Each lower call uses the same accepted
// semantic implementation as the recovered side, within that model's ABI.
void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    auto& m=active->memory; const auto sp=c.r1.u32;
    WriteU64(m,sp-40u,c.r28.u64); WriteU64(m,sp-32u,c.r29.u64);
    WriteU64(m,sp-24u,c.r30.u64); WriteU64(m,sp-16u,c.r31.u64);
    m.WriteU32(sp-8u,c.r12.u32);
}
void OriginalSave29(PPCContext& c, std::uint8_t*)
{
    auto& m=active->memory; const auto sp=c.r1.u32;
    WriteU64(m,sp-32u,c.r29.u64); WriteU64(m,sp-24u,c.r30.u64);
    WriteU64(m,sp-16u,c.r31.u64); m.WriteU32(sp-8u,c.r12.u32);
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    auto& m=active->memory; const auto sp=c.r1.u32;
    c.r28.u64=ReadU64(m,sp-40u); c.r29.u64=ReadU64(m,sp-32u);
    c.r30.u64=ReadU64(m,sp-24u); c.r31.u64=ReadU64(m,sp-16u);
    c.r12.u64=m.ReadU32(sp-8u); c.lr=c.r12.u64;
}
void OriginalRestore29(PPCContext& c, std::uint8_t*)
{
    auto& m=active->memory; const auto sp=c.r1.u32;
    c.r29.u64=ReadU64(m,sp-32u); c.r30.u64=ReadU64(m,sp-24u);
    c.r31.u64=ReadU64(m,sp-16u); c.r12.u64=m.ReadU32(sp-8u);
    c.lr=c.r12.u64;
}
void OriginalHeap(PPCContext& c,std::uint8_t*)
{ c.r3.u64=GetProcessHeap(active->memory); }
void OriginalAllocate(PPCContext& c,std::uint8_t*)
{ c.r3.u64=AllocateRawMemory(active->memory,*active,c.r3.u64); }
void OriginalBanner(PPCContext& c,std::uint8_t*)
{ c.r3.u64=ReportMissingHeapBanner(active->memory,*active); }
void OriginalReport(PPCContext& c,std::uint8_t*)
{ c.r3.u64=ReportRuntimeError(active->memory,*active,c.r3.u64); }
void OriginalTerminate(PPCContext& c,std::uint8_t*)
{ c.r3.u64=TerminateAllocationFailure(*active); }
void OriginalError(PPCContext& c,std::uint8_t*)
{ c.r3.u64=GetAllocationErrorAddress(*active); }
void OriginalFree(PPCContext& c,std::uint8_t*)
{ c.r3.u64=FreeCrtRecord(active->memory,*active,c.r3.u64,
    c.r13.u32,c.r1.u32); }
void OriginalInitialize(PPCContext& c,std::uint8_t*)
{
    InvalidParameterCall call{{{c.r3.u64,c.r4.u64,c.r5.u64,c.r6.u64,
        c.r7.u64,c.r8.u64,c.r9.u64,c.r10.u64}},c.r13.u64};
    crt_stream_state::FrameRegisters frame{c.lr,c.r31.u64,c.r1.u64};
    std::uint64_t result=0;
    (void)crt_stream_state::Apply(0x82b821b0u,active->memory,
        *active,*active,*active,*active,call,c.r1.u64,frame,result);
    c.r3.u64=result; c.r4.u64=call.arguments[1];
    c.r5.u64=call.arguments[2]; c.r6.u64=call.arguments[3];
    c.r7.u64=call.arguments[4]; c.r8.u64=call.arguments[5];
    c.r9.u64=call.arguments[6]; c.r10.u64=call.arguments[7];
    c.r13.u64=call.thread_environment;
    c.r1.u64=frame.sp; c.lr=frame.lr; c.r31.u64=frame.r31;
}
void OriginalPointerUnlock(PPCContext& c,std::uint8_t*)
{
    crt_stream_pointer_unlock::Registers r{c.r1.u64,c.lr,c.r3.u64,
        c.r9.u64,c.r10.u64,c.r11.u64,c.r12.u64,
        c.r30.u64,c.r31.u64,c.xer.ca};
    (void)crt_stream_pointer_unlock::Apply(0x82b81af8u,
        active->memory,*active,r);
    c.r1.u64=r.sp; c.lr=r.lr; c.r3.u64=r.r3;
    c.r9.u64=r.r9; c.r10.u64=r.r10; c.r11.u64=r.r11;
    c.r12.u64=r.r12; c.r30.u64=r.r30; c.r31.u64=r.r31;
    c.xer.ca=r.xer_ca;
}
void OriginalIndexUnlock(PPCContext& c,std::uint8_t*)
{
    crt_stream_index_unlock::Registers r{c.r1.u64,c.lr,c.r3.u64,
        c.r10.u64,c.r11.u64,c.r12.u64,c.r29.u64,c.r31.u64};
    (void)crt_stream_index_unlock::Apply(0x82b863b8u,
        active->memory,*active,r);
    c.r1.u64=r.sp; c.lr=r.lr; c.r3.u64=r.r3;
    c.r10.u64=r.r10; c.r11.u64=r.r11; c.r12.u64=r.r12;
    c.r29.u64=r.r29; c.r31.u64=r.r31;
}
void OriginalNative(PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    active->EnterCriticalSection(active->memory,state);
    ToPpc(c,state);
}
void OriginalFatalIndirect(GuestAddress target,PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    active->CallFatal(target,active->memory,state);
    ToPpc(c,state);
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        GuestWindow window(Regions); Seed(window,Mode::ExistingLock);
        Services services(window,Mode::ExistingLock);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xffffffffu,services.memory,
                Dependencies(services),state) || state!=saved ||
            !services.events.empty())
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-stream-locks %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected state, committed RAM and native events; accepted lower models have bounded ABI; no faults/MMIO/concurrency/runtime acceptance");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
