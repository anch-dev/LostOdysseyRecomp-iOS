// Appended after the four pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_formatter.h"
#include "lo_semantics/crt_format_dispatch.h"
#include "lo_semantics/read_only_fields.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_formatter;
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
constexpr std::array<Region,6> Regions{{{0,0x120000u},
    {0x820d3000u,0x1000u},{0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};

enum class Mode {CountZero,NullBuffer,OddMode,Binary,Text,ShortWrite,
    NativeFailureFive,Seek,NegativeSeek,LockedWrite,LockedSeek,LockedInvalidWrite,
    LockedInvalidSeek,InvalidAfterLock,BufferedChar,FlushChar,
    WideBuffered,WideFlush,WideDirect,WideBufferFull,ModeFour,ModeTwo,
    ModeTwoFallback,Converted,Rejected,Reserved,CounterSkip,CounterSuccess,
    CounterFailure};
using Event=std::array<std::uint64_t,5>;
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices,
    crt_stream_locks::NativeServices,
    crt_stream_io::NativeServices,
    crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    family::DynamicServices
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
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override {Unexpected();}
    void CallGuestFormatter(GuestAddress,GuestMemory&,
        family::Registers&) override {Unexpected();}
};
Services* active=nullptr;

family::Dependencies Dependencies(Services& s)
{
    return {{{s,s,s,s,{s,s,s,s,s,s,s,s},s,s},s},{{s,s},s},s,s,s};
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
    PPCRegister* fields[]={&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,
        &c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,
        &c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,
        &c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,
        &c.r30,&c.r31};
    for(unsigned i=0;i<32u;++i) fields[i]->u64=s.r[i];
    c.r1.u64=s.sp;c.lr=s.lr;c.ctr.u64=s.ctr;
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

enum class Scenario {Literal,Decimal,Hex,Character,WideString,
    Count,Float,Invalid};
struct Case {GuestAddress entry;Scenario scenario;};
constexpr std::array<Case,8> Cases{{
    {0x82b7d158u,Scenario::Literal},
    {0x82b7d020u,Scenario::Decimal},
    {0x82b7ca10u,Scenario::Hex},
    {0x82319330u,Scenario::Character},
    {0x82319330u,Scenario::WideString},
    {0x82319330u,Scenario::Count},
    {0x82319330u,Scenario::Float},
    {0x82b7d020u,Scenario::Invalid}}};
constexpr GuestAddress Format=0x40000u,Arguments=0x41000u;
constexpr GuestAddress Output=0x42000u,Value=0x43000u;

void ReadImageRange(GuestMemory& memory,GuestAddress address,
    std::size_t length)
{
    std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",
        std::ios::binary);
    if(!image) throw std::runtime_error("missing private image fixture");
    image.seekg(static_cast<std::streamoff>(address-0x82000000u));
    std::vector<char> bytes(length);
    image.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(image.gcount()!=static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("short private image fixture range");
    for(std::size_t i=0;i<bytes.size();++i)
        memory.WriteU8(address+static_cast<GuestAddress>(i),
            static_cast<std::uint8_t>(bytes[i]));
}

void Seed(GuestWindow& window,const Case& item)
{
    window.Fill(0);
    auto memory=window.Memory();
    ReadImageRange(memory,0x820d3000u,0x1000u);
    ReadImageRange(memory,0x83214000u,0x3000u);
    memory.WriteU32(0x83214fc8u+24u,0x8231a2a0u);
    memory.WriteU32(0x83214fc8u+32u,0x82b7eef8u);
    memory.WriteU32(0x83214fc8u+36u,0x82b7ee58u);
    if(item.scenario==Scenario::Count)
        memory.WriteU32(0x832d3ce8u,
            memory.ReadU32(0x83216000u)|1u);
    memory.WriteU32(Environment+256u,ErrorState);
    memory.WriteU32(0x83214d74u,0x7200u);
    memory.WriteU32(0x83214d78u,1u);
    memory.WriteU32(Count,16u);
    memory.WriteU32(Blocks,0x30000u);
    memory.WriteU32(Record,0x3500u);
    memory.WriteU8(Record+4u,1u);
    memory.WriteU32(Record+8u,0x70000u);
    memory.WriteU32(Stream,Output);
    memory.WriteU32(Stream+8u,Output);
    memory.WriteU32(Stream+12u,0x42u);
    memory.WriteU32(Stream+4u,0x7fffffffu);
    memory.WriteU32(Stream+16u,0u);
    memory.WriteU32(Stream+24u,0x7fffffffu);
    const char* format="A";
    switch(item.scenario)
    {
    case Scenario::Decimal:format="%d";break;
    case Scenario::Hex:format="%#x";break;
    case Scenario::Character:format="%c";break;
    case Scenario::WideString:format="%s";break;
    case Scenario::Count:format="a%n";break;
    case Scenario::Float:format="%f";break;
    case Scenario::Invalid:format="%d";break;
    default:break;
    }
    for(unsigned index=0;format[index]!=0;++index)
        memory.WriteU16(Format+2u*index,
            static_cast<std::uint8_t>(format[index]));
    memory.WriteU16(Format+2u*static_cast<GuestAddress>(
        std::char_traits<char>::length(format)),0);
    memory.WriteU32(Arguments+4u,
        item.scenario==Scenario::Count?Value:
        item.scenario==Scenario::WideString?Value:
        item.scenario==Scenario::Character?'Z':
        item.scenario==Scenario::Hex?0x2au:42u);
    WriteU64(memory,Arguments,0x3ff0000000000000ull);
    if(item.scenario!=Scenario::Float)
        memory.WriteU32(Arguments+4u,
            item.scenario==Scenario::Count?Value:
            item.scenario==Scenario::WideString?Value:
            item.scenario==Scenario::Character?'Z':
            item.scenario==Scenario::Hex?0x2au:42u);
    memory.WriteU16(Value,'X');memory.WriteU16(Value+2u,'Y');
    memory.WriteU16(Value+4u,0);
}

PPCContext Initial(const Case& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef01u;
    context.r13.u64=Environment;
    context.r3.u64=item.entry==0x82319330u?Stream:Output;
    context.r4.u64=item.entry==0x82b7d020u ||
        item.entry==0x82b7d158u?128u:Format;
    context.r5.u64=item.entry==0x82b7d020u ||
        item.entry==0x82b7d158u?Format:0u;
    context.r6.u64=Arguments;
    context.r7.u64=Arguments;
    if(item.scenario==Scenario::Invalid)
        context.r5.u64=0;
    for(unsigned i=14u;i<=31u;++i)
    {
        PPCRegister* fields[]={&context.r14,&context.r15,&context.r16,
            &context.r17,&context.r18,&context.r19,&context.r20,
            &context.r21,&context.r22,&context.r23,&context.r24,
            &context.r25,&context.r26,&context.r27,&context.r28,
            &context.r29,&context.r30,&context.r31};
        fields[i-14u]->u64=0x1122334455660000ull+i;
    }
    context.xer.so=1;
    return context;
}

bool ExpectedPath(const Case& item,const Services& services,
    const PPCContext& context)
{
    const auto memory=services.memory;
    switch(item.scenario)
    {
    case Scenario::Literal:return context.r3.u32==1u &&
        memory.ReadU16(Output)=='A';
    case Scenario::Decimal:return context.r3.u32==2u &&
        memory.ReadU16(Output)=='4' && memory.ReadU16(Output+2u)=='2';
    case Scenario::Hex:return context.r3.u32>=4u &&
        memory.ReadU16(Output)=='0';
    case Scenario::Character:return context.r3.u32==1u &&
        memory.ReadU16(Output)=='Z';
    case Scenario::WideString:return context.r3.u32==2u &&
        memory.ReadU16(Output)=='X';
    case Scenario::Count:return memory.ReadU32(Value)==1u;
    case Scenario::Float:return context.r3.u32>=1u &&
        memory.ReadU16(Output)>='0';
    case Scenario::Invalid:return context.r3.u32==0xffffffffu;
    }
    return false;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item);Seed(recovered,item);
    Services expected(original,Mode::WideBuffered),
        actual(recovered,Mode::WideBuffered);
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    std::fprintf(stderr,"formatter original entry=%08X scenario=%u\n",
        item.entry,static_cast<unsigned>(item.scenario));
    switch(item.entry)
    {
    case 0x82b7d158u:__imp__sub_82B7D158(raw,original.Bytes());break;
    case 0x82b7d020u:__imp__sub_82B7D020(raw,original.Bytes());break;
    case 0x82319330u:__imp__sub_82319330(raw,original.Bytes());break;
    case 0x82b7ca10u:__imp__sub_82B7CA10(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown original formatter entry");
    }
    active=nullptr;
    if(!ExpectedPath(item,expected,raw))
    {
        std::fprintf(stderr,"formatter fixture missed original branch entry=%08X scenario=%u r3=%llX output=%04X %04X\n",
            item.entry,static_cast<unsigned>(item.scenario),
            static_cast<unsigned long long>(raw.r3.u64),
            expected.memory.ReadU16(Output),
            expected.memory.ReadU16(Output+2u));
        throw std::runtime_error("formatter original fixture path");
    }
    std::fprintf(stderr,"formatter model entry=%08X scenario=%u\n",
        item.entry,static_cast<unsigned>(item.scenario));
    if(!family::Apply(item.entry,actual.memory,Dependencies(actual),state))
        throw std::runtime_error("recovered formatter entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL formatter %08X scenario=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX ctr=%llX/%llX RAM=%u events=%zu/%zu\n",
            item.entry,static_cast<unsigned>(item.scenario),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            original.EqualCommitted(recovered),
            expected.events.size(),actual.events.size());
        for(unsigned i=0;i<32u;++i)
            if(observed.r[i]!=state.r[i])
                std::fprintf(stderr," r%u=%llX/%llX",i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fprintf(stderr,"\n");
        std::fprintf(stderr,"  CR0 %u%u%u%u/%u%u%u%u CR6 %u%u%u%u/%u%u%u%u XER so,ca=%u,%u/%u,%u\n",
            observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,
            observed.cr0.so,state.cr0.lt,state.cr0.gt,state.cr0.eq,
            state.cr0.so,observed.cr6.lt,observed.cr6.gt,
            observed.cr6.eq,observed.cr6.so,state.cr6.lt,state.cr6.gt,
            state.cr6.eq,state.cr6.so,observed.xer_so,observed.xer_ca,
            state.xer_so,state.xer_ca);
        unsigned shown=0;
        for(const auto region:Regions)
        {
            for(std::size_t offset=0;offset<region.size;++offset)
            {
                const auto address=region.base+
                    static_cast<GuestAddress>(offset);
                if(original.Bytes()[address]==recovered.Bytes()[address])
                    continue;
                std::fprintf(stderr,"  RAM %08X:",address);
                for(unsigned byte=0;byte<12u &&
                    offset+byte<region.size;++byte)
                    std::fprintf(stderr," %02X/%02X",
                        original.Bytes()[address+byte],
                        recovered.Bytes()[address+byte]);
                std::fprintf(stderr,"\n");
                if(++shown==3u) break;
                offset+=11u;
            }
            if(shown==3u) break;
        }
        return false;
    }
    return true;
}
} // namespace

void OriginalSave14(PPCContext& context,std::uint8_t*)
{
    auto memory=active->memory;
    auto state=FromPpc(context);
    for(unsigned i=14u;i<=31u;++i)
        WriteU64(memory,Address(context.r1.u64-16u-8u*(31u-i)),
            state.r[i]);
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalRestore14(PPCContext& context,std::uint8_t*)
{
    auto memory=active->memory;
    auto state=FromPpc(context);
    for(unsigned i=14u;i<=31u;++i)
        state.r[i]=ReadU64(memory,
            Address(context.r1.u64-16u-8u*(31u-i)));
    ToPpc(context,state);
    context.r12.u64=memory.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

void OriginalAccepted(GuestAddress address,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    auto deps=Dependencies(*active);
    if(crt_wide_stream_output::ApplyAcceptedCallee(address,active->memory,
            deps.output,state))
    {
        ToPpc(context,state);return;
    }
    if(address==0x822a07a0u || address==0x82b85420u)
    {
        crt_formatting_support::Registers call{};
        call.sp=state.sp;call.lr=state.lr;
        call.r3=state.r[3];call.r4=state.r[4];call.r5=state.r[5];
        call.r6=state.r[6];call.r7=state.r[7];call.r8=state.r[8];
        call.r9=state.r[9];call.r10=state.r[10];call.r11=state.r[11];
        call.r12=state.r[12];call.r13=state.r[13];call.r31=state.r[31];
        call.xer_so=state.xer_so;
        call.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so};
        call.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
        if(!crt_formatting_support::Apply(address,active->memory,
                deps.support,call))
            throw std::runtime_error("formatter support model missing");
        state.sp=call.sp;state.lr=call.lr;
        state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
        state.r[6]=call.r6;state.r[7]=call.r7;state.r[8]=call.r8;
        state.r[9]=call.r9;state.r[10]=call.r10;state.r[11]=call.r11;
        state.r[12]=call.r12;state.r[13]=call.r13;state.r[31]=call.r31;
        state.xer_so=call.xer_so;
        state.cr0={call.cr0.lt,call.cr0.gt,call.cr0.eq,call.cr0.so};
        state.cr6={call.cr6.lt,call.cr6.gt,call.cr6.eq,call.cr6.so};
    }
    else if(address==0x82b7a680u)
    {
        lo::semantic::read_only_fields::Registers call{state.r[3],state.r[4],
            state.r[5],state.r[8],state.r[9],state.r[10],state.r[11],
            state.r[13],state.r[18]};
        if(!lo::semantic::read_only_fields::Apply(address,call,active->memory))
            throw std::runtime_error("formatter character model missing");
        state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
        state.r[8]=call.r8;state.r[9]=call.r9;state.r[10]=call.r10;
        state.r[11]=call.r11;state.r[13]=call.r13;state.r[18]=call.r18;
    }
    else if(address==0x823acbd0u)
        state.r[3]=AllocateRawMemory(active->memory,
            deps.output.streams.raw,state.r[3]);
    else if(address==0x823addc0u)
        state.r[3]=FreeCrtRecord(active->memory,deps.allocation,
            state.r[3],Address(state.r[13]),Address(state.sp));
    else if(crt_float_formatting::Apply(address,active->memory,
                deps.floating,state) ||
        crt_format_dispatch::Apply(address,active->memory,
            deps.output.streams.locks,state))
    {}
    else
        throw std::runtime_error("formatter original direct callee unhandled");
    ToPpc(context,state);
}

void OriginalIndirect(GuestAddress target,PPCContext& context,
    std::uint8_t* base)
{
    OriginalAccepted(target,context,base);
}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        std::printf("PASS crt-formatter %zu focused original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected state/RAM and accepted lower ABI; other formats, faults, MMIO and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
