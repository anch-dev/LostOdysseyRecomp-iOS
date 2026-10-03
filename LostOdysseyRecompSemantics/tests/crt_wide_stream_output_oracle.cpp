// Appended after the three pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_wide_stream_output.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_wide_stream_output;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u, Environment=0x60000u;
constexpr GuestAddress ErrorState=0x61000u, Record=0x30140u;
constexpr GuestAddress Blocks=0x83378d80u, Count=0x83378d68u;
constexpr GuestAddress Stream=0x50000u, Buffer=0x52000u;
constexpr GuestAddress Counter=0x53000u, Unicode=0x54000u, Ansi=0x55000u;
constexpr std::array<Region,5> Regions{{{0,0x120000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Mode {CountZero,NullBuffer,OddMode,Binary,Text,ShortWrite,
    NativeFailureFive,Seek,NegativeSeek,LockedWrite,LockedSeek,LockedInvalidWrite,
    LockedInvalidSeek,InvalidAfterLock,BufferedChar,FlushChar,
    WideBuffered,WideFlush,WideDirect,WideBufferFull,ModeFour,ModeTwo,
    ModeTwoFallback,Converted,Rejected,Reserved,CounterSkip,CounterSuccess,
    CounterFailure};
struct Case {GuestAddress address; Mode mode;};
constexpr Case Cases[]={{0x82b87868u,Mode::WideBuffered},
    {0x82b87868u,Mode::WideFlush},{0x82b87868u,Mode::WideDirect},
    {0x82b87868u,Mode::WideBufferFull},
    {0x822a03d8u,Mode::ModeFour},{0x822a03d8u,Mode::ModeTwo},
    {0x822a03d8u,Mode::ModeTwoFallback},
    {0x822a03d8u,Mode::Converted},{0x822a03d8u,Mode::Rejected},
    {0x822a03d8u,Mode::Reserved},
    {0x82b84a08u,Mode::CounterSkip},
    {0x82b84a08u,Mode::CounterSuccess},
    {0x82b84a08u,Mode::CounterFailure}};

using Event=std::array<std::uint64_t,5>;
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices,
    crt_stream_locks::NativeServices,
    crt_stream_io::NativeServices,
    crt_formatting_support::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    unsigned writes=0, seeks=0, locks=0, unlocks=0, traps=0;
    unsigned verified_text_buffers=0, converted_errors=0;
    unsigned verified_wide_buffers=0, unicode_outputs=0;
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
        if (mode==Mode::WideDirect)
        {
            if (count!=2 || m.ReadU8(Address(state.r8))!=0x12u ||
                m.ReadU8(Address(state.r8+1u))!=0x34u)
                throw std::runtime_error("wide direct native payload");
            ++verified_wide_buffers;
        }
        if (mode==Mode::WideFlush)
        {
            if (count!=2 || m.ReadU8(Address(state.r8))!=0xabu ||
                m.ReadU8(Address(state.r8+1u))!=0xcdu)
                throw std::runtime_error("wide flush native payload");
            ++verified_wide_buffers;
        }
        if (mode==Mode::WideBufferFull)
        {
            if (count!=16 || m.ReadU8(Address(state.r8))!=0xabu ||
                m.ReadU8(Address(state.r8+1u))!=0xcdu)
                throw std::runtime_error("full wide buffer native payload");
            ++verified_wide_buffers;
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
    void InitializeUnicodeString(GuestMemory& m,
        crt_formatting_support::Registers& state) override
    {
        events.push_back({7,state.sp,state.lr,state.r3,state.r4});
        m.WriteU16(Address(state.r3),2);
        m.WriteU16(Address(state.r3+2u),4);
        m.WriteU32(Address(state.r3+4u),Unicode);
    }
    void UnicodeStringToAnsiString(GuestMemory& m,
        crt_formatting_support::Registers& state) override
    {
        events.push_back({8,state.sp,state.lr,state.r3,state.r4});
        m.WriteU16(Address(state.r3),1);
        m.WriteU16(Address(state.r3+2u),2);
        m.WriteU32(Address(state.r3+4u),Ansi);
        state.r3=0;
    }
    void FreeAnsiString(GuestMemory&,
        crt_formatting_support::Registers& state) override
    {
        events.push_back({9,state.sp,state.lr,state.r3,0});
        state.r3=0x1122334455667788ull;
    }
    void InitAnsiString(GuestMemory& m,GuestAddress descriptor,
        GuestAddress message) override
    {
        events.push_back({10,descriptor,message,0,0});
        m.WriteU16(descriptor,1);m.WriteU16(descriptor+2u,2);
        m.WriteU32(descriptor+4u,message);
    }
    std::uint64_t WriteAnsi(GuestAddress buffer,std::uint16_t length) override
    {
        ++unicode_outputs;
        events.push_back({11,buffer,length,0,0});
        return 0;
    }
};
Services* active=nullptr;

family::Dependencies Dependencies(Services& s)
{
    return {{s,s,s,s,{s,s,s,s,s,s,s,s},s,s},s};
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

void Seed(GuestWindow& window,const Case& item)
{
    window.Fill(0);
    auto m=window.Memory();
    m.WriteU32(Environment+256u,ErrorState);
    m.WriteU32(0x83214d74u,0x7200u);
    m.WriteU32(0x83214d78u,1u);
    m.WriteU32(Count,16u);
    m.WriteU32(Blocks,0x30000u);
    m.WriteU32(Record,0x3500u);
    m.WriteU8(Record+4u,
        item.mode==Mode::Converted || item.mode==Mode::Rejected ? 0x80u : 1u);
    m.WriteU32(Record+8u,0x70000u);
    const auto descriptor_mode=item.mode==Mode::ModeFour?4u:
        item.mode==Mode::ModeTwo || item.mode==Mode::ModeTwoFallback?2u:0u;
    m.WriteU8(Record+40u,static_cast<std::uint8_t>(descriptor_mode));
    m.WriteU32(0x831e7df4u,0x34000u);
    m.WriteU32(0x34020u,0x2401u);
    m.WriteU32(0x34024u,0x2501u);
    m.WriteU32(Stream+8u,Buffer);
    m.WriteU32(Stream,Buffer+(item.mode==Mode::WideFlush?2u:
        item.mode==Mode::WideBufferFull?16u:0u));
    std::uint32_t flags=0x8au;
    if (item.mode==Mode::WideBuffered || item.mode==Mode::WideFlush ||
        item.mode==Mode::WideBufferFull || item.mode==Mode::ModeTwoFallback)
        flags=0x10au;
    if (item.mode==Mode::WideDirect) flags=6u;
    if (item.mode==Mode::CounterSkip || item.mode==Mode::CounterSuccess ||
        item.mode==Mode::CounterFailure) flags=0x40u;
    m.WriteU32(Stream+12u,flags);
    std::uint32_t available=8u;
    if (item.mode==Mode::ModeTwoFallback || item.mode==Mode::WideBufferFull)
        available=0;
    m.WriteU32(Stream+4u,available);
    m.WriteU32(Stream+16u,5u);
    m.WriteU32(Stream+24u,16u);
    m.WriteU8(Buffer,0xabu);m.WriteU8(Buffer+1u,0xcdu);
    m.WriteU32(Counter,7u);
    m.WriteU16(Unicode,0x1234u);
    m.WriteU8(Ansi,'X');m.WriteU8(Ansi+1u,0);
    if (item.mode==Mode::CounterSkip) m.WriteU32(Stream+8u,0);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0xabcdef01u;c.r13.u64=Environment;
    c.r3.u64=0x1234u;c.r4.u64=Stream;c.r5.u64=Counter;
    c.r27.u64=0x1122334455667727ull;
    c.r28.u64=0x1122334455667728ull;
    c.r29.u64=0x1122334455667729ull;
    c.r30.u64=0x1122334455667730ull;
    c.r31.u64=0x1122334455667731ull;
    c.xer.so=1;c.cr0.gt=1;c.cr6.lt=1;
    if (item.mode==Mode::Converted) c.r3.u64=0xe9u;
    if (item.mode==Mode::Rejected) c.r3.u64=0x3456u;
    if (item.mode==Mode::Reserved) c.r4.u64=0x83214b10u;
    if (item.mode==Mode::CounterFailure) c.r3.u64=0xffffu;
    return c;
}

bool ExpectedPath(const Case& item,const Services& s,const PPCContext& c)
{
    const auto m=s.memory;
    switch (item.mode)
    {
    case Mode::WideBuffered:
        return s.writes==0 && m.ReadU16(Buffer)==0x1234u;
    case Mode::WideFlush:
        return s.writes==1 && s.verified_wide_buffers==1 &&
            m.ReadU16(Buffer)==0x1234u;
    case Mode::WideDirect:
        return s.writes==1 && s.verified_wide_buffers==1;
    case Mode::WideBufferFull:
        return s.writes==1 && s.verified_wide_buffers==1 &&
            m.ReadU16(Buffer)==0x1234u;
    case Mode::ModeFour:
        return s.writes==0 && m.ReadU16(Buffer)==0x1234u &&
            m.ReadU32(Stream+4u)==6u;
    case Mode::ModeTwo:
        return s.writes==0 && m.ReadU8(Buffer)==0x12u &&
            m.ReadU8(Buffer+1u)==0x34u;
    case Mode::ModeTwoFallback:
        return m.ReadU8(Buffer)==0x12u && m.ReadU8(Buffer+1u)==0x34u;
    case Mode::Converted:
        return s.writes==0 && m.ReadU8(Buffer)==0xe9u &&
            m.ReadU32(Stream+4u)==7u;
    case Mode::Rejected:
        return s.writes==0 && m.ReadU32(0x83215210u)==42u &&
            c.r3.u64==UINT64_MAX;
    case Mode::Reserved:
        return s.unicode_outputs==1 && s.writes==0 &&
            m.ReadU16(Address(c.r1.u64-144u+80u))==0x1234u;
    case Mode::CounterSkip:
        return s.writes==0 && m.ReadU32(Counter)==8u &&
            c.r3.u64==0x1234u;
    case Mode::CounterSuccess:
        return s.writes==0 && m.ReadU32(Counter)==8u &&
            m.ReadU16(Buffer)==0x1234u;
    case Mode::CounterFailure:
        return s.writes==0 && m.ReadU32(Counter)==UINT32_MAX &&
            m.ReadU16(Buffer)==0xffffu;
    default:return false;
    }
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item);Seed(recovered,item);
    Services expected(original,item.mode),actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    switch (item.address)
    {
    case 0x82b87868u:__imp__sub_82B87868(raw,original.Bytes());break;
    case 0x822a03d8u:__imp__sub_822A03D8(raw,original.Bytes());break;
    case 0x82b84a08u:__imp__sub_82B84A08(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown selected entry");
    }
    active=nullptr;
    if (!ExpectedPath(item,expected,raw))
    {
        std::fprintf(stderr,"fixture path %08X mode=%u writes=%u unicode=%u "
            "buffer=%02X %02X count=%08X stream_count=%08X r3=%llX\n",
            item.address,static_cast<unsigned>(item.mode),expected.writes,
            expected.unicode_outputs,expected.memory.ReadU8(Buffer),
            expected.memory.ReadU8(Buffer+1u),expected.memory.ReadU32(Counter),
            expected.memory.ReadU32(Stream+4u),
            static_cast<unsigned long long>(raw.r3.u64));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if (!family::Apply(item.address,actual.memory,Dependencies(actual),state))
        throw std::runtime_error("recovered entry missing");
    const auto was=FromPpc(raw);
    if (!Same(was,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL wide-stream %08X mode=%u r3=%llX/%llX "
            "r11=%llX/%llX sp=%llX/%llX events=%zu/%zu RAM=%u\n",
            item.address,static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(was.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(was.r[11]),
            static_cast<unsigned long long>(state.r[11]),
            static_cast<unsigned long long>(was.sp),
            static_cast<unsigned long long>(state.sp),
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
        Seed(window,Cases[0]);Seed(baseline,Cases[0]);
        Services services(window,Mode::WideBuffered);
        auto state=FromPpc(Initial(Cases[0]));
        const auto saved=state;
        if (family::Apply(0xffffffffu,services.memory,
                Dependencies(services),state) || !Same(saved,state) ||
            !services.events.empty() || !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed state");
        std::printf("PASS crt-wide-stream-output %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT selected state/RAM and accepted lower ABI; native internals, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
