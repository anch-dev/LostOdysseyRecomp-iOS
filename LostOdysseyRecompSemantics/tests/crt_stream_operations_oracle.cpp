// Appended after the five pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_stream_operations;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u, Environment=0x60000u;
constexpr GuestAddress ErrorState=0x61000u, Record=0x30140u;
constexpr GuestAddress Blocks=0x83378d80u, Count=0x83378d68u;
constexpr GuestAddress Stream=0x50000u, Buffer=0x52000u;
constexpr std::array<Region,5> Regions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Mode {CountZero,NullBuffer,OddMode,Binary,Text,ShortWrite,
    NativeFailureFive,Seek,NegativeSeek,LockedWrite,LockedSeek,LockedInvalidWrite,
    LockedInvalidSeek,InvalidAfterLock,BufferedChar,FlushChar};
struct Case {GuestAddress address; Mode mode;};
constexpr Case Cases[]={{0x82b81b88u,Mode::CountZero},
    {0x82b81b88u,Mode::NullBuffer},{0x82b81b88u,Mode::OddMode},
    {0x82b81b88u,Mode::Binary},{0x82b81b88u,Mode::Text},
    {0x82b81b88u,Mode::ShortWrite},
    {0x82b81b88u,Mode::NativeFailureFive},
    {0x82b85ea8u,Mode::Seek},
    {0x82b85ea8u,Mode::NegativeSeek},
    {0x82b81de0u,Mode::LockedWrite},
    {0x82b85f70u,Mode::LockedSeek},
    {0x82b81de0u,Mode::LockedInvalidWrite},
    {0x82b85f70u,Mode::LockedInvalidSeek},
    {0x82b81de0u,Mode::InvalidAfterLock},
    {0x82b82558u,Mode::BufferedChar},
    {0x82b82558u,Mode::FlushChar}};

using Event=std::array<std::uint64_t,5>;
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices,
    crt_stream_locks::NativeServices,
    crt_stream_io::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned writes=0, seeks=0, locks=0, unlocks=0, traps=0;
    unsigned verified_text_buffers=0, converted_errors=0;
    Services(GuestWindow& window,Mode value):memory(window.Memory()),mode(value){}
    [[noreturn]] static void Unexpected()
    {throw std::runtime_error("unexpected accepted lower boundary");}

    // Existing stream locks and statically available CRT error slots are the
    // only lower paths required by these fixtures.
    std::uint64_t GetTlsValue(std::uint32_t) override {return 0x7001u;}
    void SetTlsValue(std::uint32_t,std::uint64_t) override {Unexpected();}
    std::uint64_t CallThreadDataGetter(GuestAddress,std::uint64_t) override
    {return 0;}
    std::uint64_t AllocateThreadData(std::uint32_t,std::uint32_t) override
    {return 0;}
    std::uint64_t BindThreadData(GuestAddress,std::uint64_t,
        std::uint64_t) override {Unexpected();}
    void FreeThreadData(std::uint64_t) override {Unexpected();}
    void CallHandler(GuestMemory&,GuestAddress,InvalidParameterCall&) override
    {Unexpected();}
    void Trap(const InvalidParameterCall&) override {++traps;}

    std::uint64_t AllocateHeap(GuestAddress,std::uint32_t,
        std::uint64_t) override {Unexpected();}
    void EnterMissingHeapPath() override {Unexpected();}
    void ReportMissingHeap(std::uint32_t) override {Unexpected();}
    void TerminateMissingHeap(std::uint32_t) override {Unexpected();}
    std::int32_t RetryAllocation(std::uint64_t) override {Unexpected();}
    GuestAddress GetErrorAddress() override {return 0x83215210u;}

    std::uint64_t GetThreadData() override {return 0;}
    std::uint64_t OutputErrorMessage(GuestAddress) override {Unexpected();}
    std::uint64_t BugCheck(std::uint32_t) override {Unexpected();}
    std::uint64_t CallNewHandler(GuestAddress,std::uint64_t) override
    {Unexpected();}
    void ReportInvalidParameter() override {Unexpected();}
    std::uint32_t GetCurrentProcessType() override {Unexpected();}
    void BugCheck(std::uint32_t,GuestAddress,GuestAddress,
        std::uint32_t,std::uint32_t) override {Unexpected();}
    void EnterCriticalSection(GuestAddress) override {Unexpected();}
    void LeaveCriticalSection(GuestAddress) override {Unexpected();}
    GuestAddress GrowHeap(GuestAddress,std::uint32_t) override {Unexpected();}
    std::int32_t AllocateVirtualMemory(GuestAddress,GuestAddress,
        std::uint32_t,std::uint32_t,std::uint32_t) override {Unexpected();}
    void RaiseException(GuestAddress) override {Unexpected();}
    std::int32_t FreeVirtualMemory(GuestAddress,GuestAddress,
        std::uint32_t,std::uint32_t) override {Unexpected();}
    void DecommitFreeBlock(GuestAddress,GuestAddress,std::uint32_t) override
    {Unexpected();}
    std::uint32_t CompareMemoryUlong(GuestAddress,std::uint32_t,
        std::uint32_t) override {Unexpected();}

    std::uint64_t InitializeCriticalSection(GuestMemory&,
        InvalidParameterCall&,crt_stream_state::FrameRegisters&) override
    {Unexpected();}
    std::uint64_t CallIndirect(GuestAddress,GuestMemory&,
        InvalidParameterCall&,crt_stream_state::FrameRegisters&) override
    {Unexpected();}
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_pointer_unlock::Registers& state) override
    {++unlocks;events.push_back({3,state.sp,state.lr,state.r3,state.r30});}
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers&) override {Unexpected();}
    void EnterCriticalSection(GuestMemory& m,
        crt_stream_locks::Registers& state) override
    {
        ++locks;
        events.push_back({2,state.sp,state.lr,state.r3,state.r31});
        if (mode==Mode::InvalidAfterLock)
            m.WriteU8(Record+4u,0);
        state.r3=0x1234567800000042ull;
    }
    void CallFatal(GuestAddress,GuestMemory&,
        crt_stream_locks::Registers&) override {Unexpected();}

    void NtWriteFile(GuestMemory& m,crt_stream_io::Registers& state) override
    {
        ++writes;
        events.push_back({4,state.sp,state.lr,state.r8,state.r9});
        const auto count=static_cast<std::uint32_t>(state.r9);
        if (mode==Mode::Text)
        {
            if (count!=4 || m.ReadU8(Address(state.r8))!='A' ||
                m.ReadU8(Address(state.r8+1u))!='\r' ||
                m.ReadU8(Address(state.r8+2u))!='\n' ||
                m.ReadU8(Address(state.r8+3u))!='B')
                throw std::runtime_error("original text path missed CRLF expansion");
            ++verified_text_buffers;
        }
        if (mode==Mode::NativeFailureFive)
        {
            state.r3=0xffffffffc0000017ull;
            return;
        }
        m.WriteU32(Address(state.sp+84u),
            mode==Mode::ShortWrite ? count-1u : count);
        state.r3=0;
    }
    void NtWaitForSingleObjectEx(GuestMemory&,
        crt_stream_io::Registers&) override {Unexpected();}
    void CallIndirect(GuestMemory& m,GuestAddress target,
        crt_stream_io::Registers& state) override
    {
        ++seeks;
        events.push_back({5,state.sp,state.lr,target,state.r6});
        if (target!=0x2400u && target!=0x2500u) Unexpected();
        if (target==0x2400u)
            WriteU64(m,Address(state.sp+80u),0x20u);
        else
            m.WriteU32(Address(state.sp+84u),0x24u);
        state.r3=0;
    }
    void NtStatusToDosError(GuestMemory&,
        crt_status_error::Registers& state) override
    {
        if (mode!=Mode::NativeFailureFive ||
            state.r3!=0xffffffffc0000017ull)
            Unexpected();
        ++converted_errors;
        events.push_back({6,state.sp,state.lr,state.r3,state.r13});
        state.r3=5;
    }
};

Services* active=nullptr;

family::Dependencies Dependencies(Services& s)
{
    return {s,s,s,s,{s,s,s,s,s,s,s,s},s,s};
}

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers s{};
    s.sp=c.r1.u64;s.lr=c.lr;s.ctr=c.ctr.u64;
    s.r={c.r0.u64,0,c.r2.u64,c.r3.u64,c.r4.u64,c.r5.u64,
        c.r6.u64,c.r7.u64,c.r8.u64,c.r9.u64,c.r10.u64,c.r11.u64,
        c.r12.u64,c.r13.u64,c.r14.u64,c.r15.u64,c.r16.u64,c.r17.u64,
        c.r18.u64,c.r19.u64,c.r20.u64,c.r21.u64,c.r22.u64,c.r23.u64,
        c.r24.u64,c.r25.u64,c.r26.u64,c.r27.u64,c.r28.u64,c.r29.u64,
        c.r30.u64,c.r31.u64};
    s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
    s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return s;
}

void ToPpc(PPCContext& c,const family::Registers& s)
{
    c.r1.u64=s.sp;c.lr=s.lr;c.ctr.u64=s.ctr;
    c.r3.u64=s.r[3];c.r4.u64=s.r[4];c.r5.u64=s.r[5];
    c.r6.u64=s.r[6];c.r7.u64=s.r[7];c.r8.u64=s.r[8];
    c.r9.u64=s.r[9];c.r10.u64=s.r[10];c.r11.u64=s.r[11];
    c.r12.u64=s.r[12];c.r13.u64=s.r[13];
    c.r21.u64=s.r[21];c.r22.u64=s.r[22];c.r23.u64=s.r[23];
    c.r24.u64=s.r[24];c.r25.u64=s.r[25];c.r26.u64=s.r[26];
    c.r27.u64=s.r[27];c.r28.u64=s.r[28];c.r29.u64=s.r[29];
    c.r30.u64=s.r[30];c.r31.u64=s.r[31];
    c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
    c.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,{s.cr0.so}};
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
}

bool Same(const family::Registers& a,const family::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr && a.ctr==b.ctr &&
        a.r==b.r && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca &&
        a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt &&
        a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
        a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt &&
        a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}

void Seed(GuestWindow& window,Mode mode)
{
    window.Fill(0);
    auto m=window.Memory();
    m.WriteU32(Environment+256u,ErrorState);
    m.WriteU32(0x83214d74u,0x7200u);
    m.WriteU32(0x83214d78u,1u);
    m.WriteU32(Count,16u);
    m.WriteU32(Blocks,0x30000u);
    m.WriteU32(Record,0x3500u);
    m.WriteU8(Record+4u,mode==Mode::Text ? 0x81u : 1u);
    m.WriteU32(Record+8u,0x70000u);
    m.WriteU8(Record+40u,mode==Mode::OddMode?4u:0u);
    m.WriteU32(0x831e7df4u,0x34000u);
    m.WriteU32(0x34020u,0x2401u);
    m.WriteU32(0x34024u,0x2501u);
    m.WriteU8(0x40000u,'A');m.WriteU8(0x40001u,'\n');
    m.WriteU8(0x40002u,'B');
    m.WriteU32(Stream+8u,Buffer);
    m.WriteU32(Stream,Buffer+(mode==Mode::FlushChar?3u:0u));
    m.WriteU32(Stream+12u,0x10au);
    m.WriteU32(Stream+16u,5u);
    m.WriteU32(Stream+24u,16u);
    m.WriteU8(Buffer,'x');m.WriteU8(Buffer+1u,'y');
    m.WriteU8(Buffer+2u,'z');
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef01u;
    c.r13.u64=Environment;
    c.r3.u64=item.address==0x82b82558u?0x51u:5u;
    c.r4.u64=item.address==0x82b82558u?Stream:0x40000u;
    c.r5.u64=3;
    if (item.mode==Mode::CountZero) c.r5.u64=0;
    if (item.mode==Mode::NullBuffer) c.r4.u64=0;
    if (item.mode==Mode::LockedInvalidWrite ||
        item.mode==Mode::LockedInvalidSeek)
        c.r3.u64=UINT64_MAX-1u;
    if (item.mode==Mode::Seek || item.mode==Mode::LockedSeek)
    {c.r4.u64=0x0000000100000004ull;c.r5.u64=0;}
    if (item.mode==Mode::NegativeSeek)
    {c.r4.u64=0xabcdef0100000004ull;c.r5.u64=0;}
    if (item.mode==Mode::LockedInvalidSeek)
    {c.r4.u64=0x0000000100000004ull;c.r5.u64=0;}
    return c;
}

bool ExpectedPath(const Case& item,const Services& s)
{
    switch (item.mode)
    {
    case Mode::CountZero:return s.writes==0 && s.seeks==0;
    case Mode::NullBuffer:return s.traps==1 && s.writes==0;
    case Mode::OddMode:return s.traps==1 && s.writes==0;
    case Mode::Binary:return s.writes==1 && s.seeks==0;
    case Mode::Text:return s.writes==1 && s.seeks==0 &&
        s.verified_text_buffers==1;
    case Mode::ShortWrite:return s.writes==1;
    case Mode::NativeFailureFive:return s.writes==1 &&
        s.converted_errors==1 &&
        s.memory.ReadU32(ErrorState+352u)==5u &&
        s.memory.ReadU32(0x83215210u)==9u &&
        s.memory.ReadU32(0x83215214u)==5u;
    case Mode::Seek:return s.seeks==1;
    case Mode::NegativeSeek:return s.seeks==0 &&
        s.memory.ReadU32(ErrorState+352u)==131u &&
        s.memory.ReadU32(0x83215210u)==22u &&
        s.memory.ReadU32(0x83215214u)==131u;
    case Mode::LockedWrite:return s.locks==1 && s.unlocks==1 && s.writes==1;
    case Mode::LockedSeek:return s.locks==1 && s.unlocks==1 && s.seeks==1;
    case Mode::LockedInvalidWrite:
    case Mode::LockedInvalidSeek:
        return s.locks==0 && s.unlocks==0 && s.writes==0 && s.seeks==0;
    case Mode::InvalidAfterLock:return s.locks==1 && s.unlocks==1 && s.writes==0;
    case Mode::BufferedChar:return s.writes==0 && s.seeks==0;
    case Mode::FlushChar:return s.locks==1 && s.unlocks==1 && s.writes==1;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item.mode);Seed(recovered,item.mode);
    Services expected(original,item.mode),actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    switch (item.address)
    {
    case 0x82b85ea8u:__imp__sub_82B85EA8(raw,original.Bytes());break;
    case 0x82b81b88u:__imp__sub_82B81B88(raw,original.Bytes());break;
    case 0x82b81de0u:__imp__sub_82B81DE0(raw,original.Bytes());break;
    case 0x82b85f70u:__imp__sub_82B85F70(raw,original.Bytes());break;
    case 0x82b82558u:__imp__sub_82B82558(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown selected entry");
    }
    active=nullptr;
    if (!ExpectedPath(item,expected))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u writes=%u seeks=%u "
            "locks=%u unlocks=%u traps=%u text=%u converted=%u "
            "last=%08X errno=%08X crt=%08X r3=%llX\n",
            item.address,static_cast<unsigned>(item.mode),expected.writes,
            expected.seeks,expected.locks,expected.unlocks,expected.traps,
            expected.verified_text_buffers,expected.converted_errors,
            expected.memory.ReadU32(ErrorState+352u),
            expected.memory.ReadU32(0x83215210u),
            expected.memory.ReadU32(0x83215214u),
            static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if (!family::Apply(item.address,actual.memory,Dependencies(actual),state))
        throw std::runtime_error("recovered entry missing");
    const auto was=FromPpc(raw);
    if (!Same(was,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL stream-operations %08X mode=%u "
            "r3=%llx/%llx sp=%llx/%llx lr=%llx/%llx "
            "events=%zu/%zu RAM=%u\n",item.address,
            static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(was.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(was.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(was.lr),
            static_cast<unsigned long long>(state.lr),
            expected.events.size(),actual.events.size(),
            original.EqualCommitted(recovered));
        return false;
    }
    return true;
}
} // namespace

void OriginalSave(unsigned first,PPCContext& c)
{
    auto& m=active->memory;
    const auto s=FromPpc(c);
    for (unsigned i=first;i<=31;++i)
        WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),s.r[i]);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalRestore(unsigned first,PPCContext& c)
{
    auto s=FromPpc(c);auto& m=active->memory;
    for (unsigned i=first;i<=31;++i)
        s.r[i]=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    s.r[12]=m.ReadU32(Address(c.r1.u64-8u));
    s.lr=s.r[12];ToPpc(c,s);
}
void OriginalCallee(GuestAddress address,PPCContext& c,std::uint8_t*)
{
    auto s=FromPpc(c);
    if (!family::ApplyAcceptedCallee(address,active->memory,
        Dependencies(*active),s))
        throw std::runtime_error("missing accepted original adapter");
    ToPpc(c,s);
}

int main()
{
    try
    {
        for (const auto& item:Cases)
            if (!Check(item)) return 1;
        GuestWindow window(Regions),baseline(Regions);
        Seed(window,Mode::Binary);Seed(baseline,Mode::Binary);
        Services services(window,Mode::Binary);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xffffffffu,services.memory,
                Dependencies(services),state) || !Same(saved,state) ||
            !services.events.empty() || !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-stream-operations %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected state/RAM; accepted lower ABI and native internals, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
