// Appended after the six pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_format_dispatch.h"
#include "lo_semantics/read_only_fields.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_format_dispatch;
using test::GuestWindow;
using recovery_abi::Address;
constexpr test::Region Regions[] = {
    {0, 0x90000u}, {0x83214000u, 0x4000u}
};
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Text = 0x50000u;
constexpr GuestAddress LocalePointer = 0x40000u;
constexpr GuestAddress LocaleCharacter = 0x41000u;
constexpr GuestAddress CharacterTable = 0x42000u;
enum class Mode { Initialize, InitializeTail, AsciiA, AsciiZ, AsciiBefore,
    AsciiAfter, TrimEmpty, TrimNoPoint, TrimZeros, TrimWhole,
    TrimFraction, InsertExponent, InsertScanned, MissingSupport };
struct Case { GuestAddress entry; Mode mode; const char* path; };
constexpr Case Cases[] = {
    {0x82b7a5e0u, Mode::Initialize, "write-ten-slots"},
    {0x82b7a678u, Mode::InitializeTail, "tail-write-ten-slots"},
    {0x82b833d0u, Mode::AsciiA, "ascii-A"},
    {0x82b833d0u, Mode::AsciiZ, "ascii-Z"},
    {0x82b833d0u, Mode::AsciiBefore, "below-uppercase"},
    {0x82b833d0u, Mode::AsciiAfter, "above-uppercase"},
    {0x82b7eef8u, Mode::TrimEmpty, "empty-return"},
    {0x82b7eef8u, Mode::TrimNoPoint, "no-decimal-return"},
    {0x82b7eef8u, Mode::TrimZeros, "trim-before-exponent"},
    {0x82b7eef8u, Mode::TrimWhole, "trim-point-and-zeros"},
    {0x82b7eef8u, Mode::TrimFraction, "keep-fraction"},
    {0x82b7ee58u, Mode::InsertExponent, "leading-E-direct"},
    {0x82b7ee58u, Mode::InsertScanned, "classification-scan"},
    {0x82b84c90u, Mode::MissingSupport, "fatal-tail-returning"},
};

void Require(bool value, const char* why)
{ if (!value) throw std::runtime_error(why); }

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices,
    crt_stream_locks::NativeServices
{
    std::vector<std::array<std::uint64_t, 6>> events;
    [[noreturn]] static void Unexpected()
    { throw std::runtime_error("unexpected fatal lower boundary"); }
    std::uint64_t GetTlsValue(std::uint32_t) override { Unexpected(); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override { Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { Unexpected(); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { Unexpected(); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override { Unexpected(); }
    void FreeThreadData(std::uint64_t) override { Unexpected(); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override { Unexpected(); }
    void Trap(const InvalidParameterCall&) override { Unexpected(); }
    std::uint64_t AllocateHeap(GuestAddress, std::uint32_t,
        std::uint64_t) override { Unexpected(); }
    void EnterMissingHeapPath() override { Unexpected(); }
    void ReportMissingHeap(std::uint32_t) override { Unexpected(); }
    void TerminateMissingHeap(std::uint32_t) override { Unexpected(); }
    std::int32_t RetryAllocation(std::uint64_t) override { Unexpected(); }
    GuestAddress GetErrorAddress() override { Unexpected(); }
    std::uint64_t GetThreadData() override { return 0; }
    std::uint64_t OutputErrorMessage(GuestAddress) override { Unexpected(); }
    std::uint64_t BugCheck(std::uint32_t) override { Unexpected(); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { Unexpected(); }
    void ReportInvalidParameter() override { Unexpected(); }
    std::uint32_t GetCurrentProcessType() override { Unexpected(); }
    void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override { Unexpected(); }
    void EnterCriticalSection(GuestAddress) override { Unexpected(); }
    void LeaveCriticalSection(GuestAddress) override { Unexpected(); }
    GuestAddress GrowHeap(GuestAddress, std::uint32_t) override { Unexpected(); }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override { Unexpected(); }
    void RaiseException(GuestAddress) override { Unexpected(); }
    std::int32_t FreeVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override { Unexpected(); }
    void DecommitFreeBlock(GuestAddress, GuestAddress,
        std::uint32_t) override { Unexpected(); }
    std::uint32_t CompareMemoryUlong(GuestAddress, std::uint32_t,
        std::uint32_t) override { Unexpected(); }
    std::uint64_t InitializeCriticalSection(GuestMemory&,
        InvalidParameterCall&, crt_stream_state::FrameRegisters&) override
    { Unexpected(); }
    std::uint64_t CallIndirect(GuestAddress, GuestMemory&,
        InvalidParameterCall&, crt_stream_state::FrameRegisters&) override
    { Unexpected(); }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_pointer_unlock::Registers&) override { Unexpected(); }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers&) override { Unexpected(); }
    void EnterCriticalSection(GuestMemory&,
        crt_stream_locks::Registers&) override { Unexpected(); }
    void CallFatal(GuestAddress target, GuestMemory& memory,
        crt_stream_locks::Registers& state) override
    {
        events.push_back({target, state.sp, state.lr, state.r3,
            state.r31, state.ctr});
        Require(target == 0x7100u && state.r3 == 255 &&
            state.lr == 0x82b7bf0cu, "fatal call arguments");
        memory.WriteU32(Address(state.sp + 80u), 0x24681357u);
        memory.WriteU32(Address(state.sp + 84u), 0x13572468u);
        state.r3 = 0xaabbccdd000000ffull;
        state.r11 = 0x1234567812345678ull;
    }
};

family::Dependencies Deps(Services& s)
{ return {s,s,s,s,s,s,s,s}; }

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp = c.r1.u64; s.lr = c.lr; s.ctr = c.ctr.u64;
#define COPY_FROM(N) s.r[N] = c.r##N.u64
    COPY_FROM(0); COPY_FROM(2); COPY_FROM(3); COPY_FROM(4);
    COPY_FROM(5); COPY_FROM(6); COPY_FROM(7); COPY_FROM(8);
    COPY_FROM(9); COPY_FROM(10); COPY_FROM(11); COPY_FROM(12);
    COPY_FROM(13); COPY_FROM(14); COPY_FROM(15); COPY_FROM(16);
    COPY_FROM(17); COPY_FROM(18); COPY_FROM(19); COPY_FROM(20);
    COPY_FROM(21); COPY_FROM(22); COPY_FROM(23); COPY_FROM(24);
    COPY_FROM(25); COPY_FROM(26); COPY_FROM(27); COPY_FROM(28);
    COPY_FROM(29); COPY_FROM(30); COPY_FROM(31);
#undef COPY_FROM
    s.r[1] = 0;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr0 = {c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    s.cr6 = {c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64 = s.sp; c.lr = s.lr; c.ctr.u64 = s.ctr;
#define COPY_TO(N) c.r##N.u64 = s.r[N]
    COPY_TO(0); COPY_TO(2); COPY_TO(3); COPY_TO(4);
    COPY_TO(5); COPY_TO(6); COPY_TO(7); COPY_TO(8);
    COPY_TO(9); COPY_TO(10); COPY_TO(11); COPY_TO(12);
    COPY_TO(13); COPY_TO(14); COPY_TO(15); COPY_TO(16);
    COPY_TO(17); COPY_TO(18); COPY_TO(19); COPY_TO(20);
    COPY_TO(21); COPY_TO(22); COPY_TO(23); COPY_TO(24);
    COPY_TO(25); COPY_TO(26); COPY_TO(27); COPY_TO(28);
    COPY_TO(29); COPY_TO(30); COPY_TO(31);
#undef COPY_TO
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr0.lt=s.cr0.lt; c.cr0.gt=s.cr0.gt;
    c.cr0.eq=s.cr0.eq; c.cr0.so=s.cr0.so;
    c.cr6.lt=s.cr6.lt; c.cr6.gt=s.cr6.gt;
    c.cr6.eq=s.cr6.eq; c.cr6.so=s.cr6.so;
}

crt_stream_locks::Registers ToLock(const PPCContext& c)
{
    return {c.r1.u64,c.lr,c.ctr.u64,c.r3.u64,c.r4.u64,c.r5.u64,
        c.r6.u64,c.r7.u64,c.r8.u64,c.r9.u64,c.r10.u64,c.r11.u64,
        c.r12.u64,c.r13.u64,c.r28.u64,c.r29.u64,c.r30.u64,c.r31.u64,
        {c.cr0.lt!=0,c.cr0.gt!=0,c.cr0.eq!=0,c.cr0.so!=0},
        {c.cr6.lt!=0,c.cr6.gt!=0,c.cr6.eq!=0,c.cr6.so!=0},
        c.xer.ca,c.xer.so};
}
void FromLock(PPCContext& c, const crt_stream_locks::Registers& s)
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

Services* active = nullptr;
GuestMemory* active_memory = nullptr;

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=Stack; c.lr=0x123456789abcdef0ull;
    c.ctr.u64=0x8877665544332211ull;
    c.r3.u64=0xaaaabbbb00050000ull;
    c.r4.u64=0x1111222233334444ull;
    c.r8.u64=0x1122334455667788ull;
    c.r10.u64=0xaabbccdd11223344ull;
    c.r11.u64=0x5566778899aabbccull;
    c.r12.u64=0xfeedfacecafebeefull;
    c.r30.u64=0x3456789abcde0001ull;
    c.r31.u64=0xfedcba9876543210ull;
    c.xer.so=1; c.cr0.lt=1; c.cr6.gt=1;
    switch (item.mode)
    {
    case Mode::AsciiA: c.r3.u64=0x1234567800000041ull; break;
    case Mode::AsciiZ: c.r3.u64=0x123456780000005aull; break;
    case Mode::AsciiBefore: c.r3.u64=0x1234567800000040ull; break;
    case Mode::AsciiAfter: c.r3.u64=0x123456780000005bull; break;
    case Mode::MissingSupport: c.r3.u64=0x9988776600000002ull; break;
    default: break;
    }
    return c;
}

void Seed(GuestWindow& window, const Case& item)
{
    window.Fill(0);
    auto m=window.Memory();
    m.WriteU32(0x83215648u,LocalePointer);
    m.WriteU32(LocalePointer,LocaleCharacter);
    m.WriteU8(LocaleCharacter,'.');
    m.WriteU32(0x832152e8u,CharacterTable);
    m.WriteU16(CharacterTable+'1'*2u,4);
    m.WriteU16(CharacterTable+'2'*2u,4);
    m.WriteU32(0x83214d70u,0x7103u);
    const char* content="";
    switch(item.mode)
    {
    case Mode::TrimNoPoint: content="123"; break;
    case Mode::TrimZeros: content="12.3400e+2"; break;
    case Mode::TrimWhole: content="12.000"; break;
    case Mode::TrimFraction: content="12.5"; break;
    case Mode::InsertExponent: content="E9"; break;
    case Mode::InsertScanned: content="12!"; break;
    default: break;
    }
    for(unsigned i=0;content[i];++i)
        m.WriteU8(Text+i,static_cast<std::uint8_t>(content[i]));
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    if(a.sp!=b.sp||a.lr!=b.lr||a.ctr!=b.ctr||a.r!=b.r||
       a.xer_so!=b.xer_so||a.xer_ca!=b.xer_ca) return false;
    return a.cr0.lt==b.cr0.lt&&a.cr0.gt==b.cr0.gt&&
        a.cr0.eq==b.cr0.eq&&a.cr0.so==b.cr0.so&&
        a.cr6.lt==b.cr6.lt&&a.cr6.gt==b.cr6.gt&&
        a.cr6.eq==b.cr6.eq&&a.cr6.so==b.cr6.so;
}

void Check(const Case& item)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original,item); Seed(recovered,item);
    auto left_memory=original.Memory(), right_memory=recovered.Memory();
    Services left,right;
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    active=&left; active_memory=&left_memory;
    switch(item.entry)
    {
    case 0x82b7a5e0u: __imp__sub_82B7A5E0(raw,original.Bytes()); break;
    case 0x82b7a678u: __imp__sub_82B7A678(raw,original.Bytes()); break;
    case 0x82b7eef8u: __imp__sub_82B7EEF8(raw,original.Bytes()); break;
    case 0x82b7ee58u: __imp__sub_82B7EE58(raw,original.Bytes()); break;
    case 0x82b833d0u: __imp__sub_82B833D0(raw,original.Bytes()); break;
    case 0x82b84c90u: __imp__sub_82B84C90(raw,original.Bytes()); break;
    default: throw std::runtime_error("invalid dispatch case");
    }
    Require(family::Apply(item.entry,right_memory,Deps(right),state),
        "dispatch entry");
    if(!Same(FromPpc(raw),state)||!original.EqualCommitted(recovered)||
       left.events!=right.events)
    {
        std::fprintf(stderr,
            "mismatch %08X path=%s r3=%llX/%llX r11=%llX/%llX cr0=%u/%u cr6=%u/%u\n",
            item.entry,item.path,raw.r3.u64,state.r[3],raw.r11.u64,
            state.r[11],raw.cr0.eq,state.cr0.eq,raw.cr6.eq,state.cr6.eq);
        throw std::runtime_error("original PPC comparison");
    }
    if(item.mode==Mode::Initialize||item.mode==Mode::InitializeTail)
    {
        constexpr GuestAddress targets[]={0x82b7fc40u,0x82b7f040u,
            0x82b7f030u,0x82b7f038u,0x82b7efb0u,0x82b7fc40u,
            0x8231a2a0u,0x82b7efd0u,0x82b7eef8u,0x82b7ee58u};
        for(unsigned i=0;i<10;++i)
            Require(right_memory.ReadU32(0x83214fc8u+i*4u)==targets[i],
                "float dispatch target order");
    }
    if(item.mode==Mode::TrimZeros)
    {
        const char expected[]="12.34e+2";
        for(unsigned i=0;i<sizeof(expected);++i)
            Require(right_memory.ReadU8(Text+i)==expected[i],
                "trim zeros before exponent");
    }
    if(item.mode==Mode::TrimWhole)
    {
        Require(right_memory.ReadU8(Text+2u)==0,
            "trim all zero fraction and point");
    }
    if(item.mode==Mode::InsertScanned)
    {
        const char expected[]="12.!";
        for(unsigned i=0;i<sizeof(expected);++i)
            Require(right_memory.ReadU8(Text+i)==expected[i],
                "character-classified decimal insertion");
    }
    if(item.mode==Mode::MissingSupport)
        Require(right.events.size()==1&&state.r[31]==0x2468135713572468ull,
            "live fatal callback and saved slot");
}
} // namespace

void OriginalCharacterFlag(PPCContext& c,std::uint8_t*)
{
    lo::semantic::read_only_fields::Registers call{c.r3.u64,c.r4.u64,c.r5.u64,
        c.r8.u64,c.r9.u64,c.r10.u64,c.r11.u64,c.r13.u64,c.r18.u64};
    (void)lo::semantic::read_only_fields::Apply(0x823588a0u,call,*active_memory);
    c.r3.u64=call.r3; c.r4.u64=call.r4; c.r5.u64=call.r5;
    c.r8.u64=call.r8; c.r9.u64=call.r9; c.r10.u64=call.r10;
    c.r11.u64=call.r11; c.r13.u64=call.r13; c.r18.u64=call.r18;
}
void OriginalFatal(PPCContext& c,std::uint8_t*)
{
    auto call=ToLock(c);
    (void)crt_stream_locks::Apply(0x82b7bed8u,*active_memory,
        Deps(*active),call);
    FromLock(c,call);
}

int main()
{
    try
    {
        for(const auto& item:Cases) Check(item);
        GuestWindow window(Regions), baseline(Regions);
        Seed(window,Cases[0]); Seed(baseline,Cases[0]);
        auto memory=window.Memory();
        auto saved=FromPpc(Initial(Cases[0]));
        auto state=saved;
        Services services;
        Require(!family::Apply(0xffffffffu,memory,Deps(services),state)&&
            Same(state,saved)&&window.EqualCommitted(baseline)&&
            services.events.empty(),"unknown entry changed state");
        std::printf("PASS crt-format-dispatch %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected PPC state/ordinary RAM; accepted character-table and fatal models; no large float conversion, native internals, faults, MMIO, concurrency or runtime proof");
        return 0;
    }
    catch(const std::exception& error)
    { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
