// Appended after the actual A8, CRT, formatter, and heap PPC bodies.
#include "lo_semantics/legacy_config_format_full_heap_chain.h"
#include "lo_semantics/crt_formatter.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_reallocate.h"
#include "lo_semantics/heap_segment.h"
#include "lo_semantics/heap_block_resize.h"
#include "lo_semantics/heap_lock_exit.h"
#include "lo_semantics/heap.h"
#include "lo_semantics/memory_fill.h"
#include "legacy_config_format_dispatch_heap_fixture.h"
#include "lo_semantics/crt_format_dispatch.h"
#include "lo_semantics/read_only_fields.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <span>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_formatter;
namespace chain = legacy_config_format_dispatch;
namespace full_chain = legacy_config_format_full_heap_chain;
namespace heap_context = heap_allocation_context;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u, Environment=0x60000u;
constexpr GuestAddress ErrorState=0x61000u, Record=0x70140u;
constexpr GuestAddress Blocks=0x83378d80u, Count=0x83378d68u;
constexpr GuestAddress Stream=0x50000u, Buffer=0x52000u;
constexpr GuestAddress Counter=0x53000u, Unicode=0x54000u, Ansi=0x55000u;
constexpr GuestAddress Format=0x40000u, Object=0x90000u;
constexpr GuestAddress Vtable=0x91000u, VirtualTarget=0x92004u;
constexpr GuestAddress FreedSink=0x93000u;
constexpr GuestAddress GrowthBlock=0x180000u;
constexpr std::array<Region,8> Regions{{{0,0x400000u},
    {0x820d3000u,0x1000u},{0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x832d3000u,0x2000u},{0x83378000u,0x3000u},
    {0x83245000u,0x2000u},{0x83369000u,0x1000u}}};

enum class Mode {CountZero,NullBuffer,OddMode,Binary,Text,ShortWrite,
    NativeFailureFive,Seek,NegativeSeek,LockedWrite,LockedSeek,LockedInvalidWrite,
    LockedInvalidSeek,InvalidAfterLock,BufferedChar,FlushChar,
    WideBuffered,WideFlush,WideDirect,WideBufferFull,ModeFour,ModeTwo,
    ModeTwoFallback,Converted,Rejected,Reserved,CounterSkip,CounterSuccess,
    CounterFailure};
using Event=std::array<std::uint64_t,5>;
struct HeapEvent
{
    GuestAddress entry;
    std::array<std::uint64_t,7> args;
    bool operator==(const HeapEvent&) const = default;
};
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, CrtAllocationServices,
    crt_stream_state::NativeServices,
    crt_stream_pointer_unlock::NativeServices,
    crt_stream_index_unlock::NativeServices,
    crt_stream_locks::NativeServices,
    crt_stream_io::NativeServices,
    crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    family::DynamicServices, heap_reallocate::Services,
    HeapSegmentServices, heap_block_resize::NativeServices,
    heap_lock_exit::NativeServices, crt_free_context::LowerCalls,
    chain::VirtualCalls, heap_context::BoundaryServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    std::vector<HeapEvent> heap_events;
    unsigned writes=0, seeks=0, locks=0, unlocks=0, traps=0;
    unsigned verified_text_buffers=0, converted_errors=0;
    unsigned verified_wide_buffers=0, unicode_outputs=0;
    std::string expected_text;
    GuestAddress expected_payload=a8_heap_fixture::Payload;
    bool grow=false;
    const char* unexpected_heap=nullptr;
    unsigned virtual_calls=0, free_calls=0;
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
    std::uint32_t GetCurrentProcessType() override {return 1u;}
    void BugCheck(std::uint32_t,GuestAddress,GuestAddress,
        std::uint32_t,std::uint32_t) override {Unexpected();}
    void EnterCriticalSection(GuestAddress) override {}
    void LeaveCriticalSection(GuestAddress) override {}
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
    std::int32_t CommitRange(GuestAddress, GuestAddress,
        GuestAddress, GuestAddress) override {Unexpected();}
    void InitializeUncommittedRange(GuestAddress, GuestAddress,
        std::uint32_t) override {Unexpected();}
    std::uint64_t CompareMemoryUlong(GuestMemory&, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint64_t,
        heap_block_resize::FrameRegisters&) override {Unexpected();}
    void LeaveCriticalSection(GuestMemory&,
        heap_lock_exit::Registers&) override {Unexpected();}
    HeapAllocateServices& Allocation() override {return *this;}
    HeapFreeServices& Free() override {return *this;}
    HeapSegmentServices& Segment() override {return *this;}
    heap_block_resize::NativeServices& ResizeNative() override
    {return *this;}
    heap_lock_exit::NativeServices& LockExitNative() override
    {return *this;}
    void GetCurrentProcessType(GuestMemory&,
        heap_reallocate::Registers& state) override {state.r3=1u;}
    void BugCheck(GuestMemory&,heap_reallocate::Registers&) override
    {Unexpected();}
    void EnterCriticalSection(GuestMemory&,
        heap_reallocate::Registers&) override {Unexpected();}
    void FreeVirtualMemory(GuestMemory&,
        heap_reallocate::Registers&) override {Unexpected();}
    void CompareMemoryUlong(GuestMemory&,
        heap_reallocate::Registers&) override {Unexpected();}
    void RaiseException(GuestMemory&,
        heap_reallocate::Registers&) override {Unexpected();}
    void Call(GuestAddress target,GuestMemory& m,
        chain::Registers& state) override;
    void CallDirect(GuestAddress entry,GuestMemory& m,
        heap_context::Registers& state) override;
    void CallNative(GuestAddress entry,GuestMemory& m,
        heap_context::Registers& state) override;
};
Services* active=nullptr;

void Services::Call(GuestAddress target,GuestMemory& m,
    chain::Registers& state)
{
    if(target==VirtualTarget)
    {
        ++virtual_calls;
        if(Address(state.r[3])!=Object ||
            Address(state.r[4])!=expected_payload ||
            state.r[5]!=760u)
            throw std::runtime_error("A8 virtual call arguments");
        for(std::size_t i=0;i<expected_text.size();++i)
            if(m.ReadU16(Address(state.r[4]+2u*i))!=
                static_cast<std::uint8_t>(expected_text[i]))
                throw std::runtime_error("A8 formatted text");
        if(m.ReadU16(Address(state.r[4]+2u*expected_text.size()))!=0)
            throw std::runtime_error("A8 formatted terminator");
        events.push_back({20u,target,state.r[3],state.r[4],state.r[5]});
        state.r[3]=0x55667788u;
        return;
    }
    if(target==0x823ade28u)
    {
        ++free_calls;
        if(Address(state.r[3])!=a8_heap_fixture::Heap || state.r[4]!=0u ||
            Address(state.r[5])!=expected_payload)
            throw std::runtime_error("A8 free selected input");
        m.WriteU32(FreedSink,Address(state.r[5]));
        events.push_back({21u,target,state.r[3],state.r[4],state.r[5]});
        state.r[3]=1u;
        return;
    }
    throw std::runtime_error("unexpected A8 mutable guest boundary");
}

void Services::CallDirect(GuestAddress entry,GuestMemory& m,
    heap_context::Registers& state)
{
    heap_events.push_back({entry,{state.r[3],state.r[4],state.r[5],
        state.r[6],state.r[7],state.r[1],state.lr}});
    if(entry==0x827cc428u)
    {
        if(!grow || m.ReadU32(a8_heap_fixture::LargeHead)!=
                a8_heap_fixture::LargeHead)
        {
            unexpected_heap="unexpected heap grow";
            state.r[3]=0u;
            return;
        }
        const auto head=a8_heap_fixture::LargeHead;
        const auto node=GrowthBlock+8u;
        m.WriteU16(GrowthBlock,256u);
        m.WriteU16(GrowthBlock+2u,0u);
        m.WriteU8(GrowthBlock+4u,0u);
        m.WriteU8(GrowthBlock+5u,0x10u);
        m.WriteU32(node,head);m.WriteU32(node+4u,head);
        m.WriteU32(head,node);m.WriteU32(head+4u,node);
        m.WriteU32(a8_heap_fixture::Heap+48u,
            m.ReadU32(a8_heap_fixture::Heap+48u)+256u);
        state.r[3]=GrowthBlock;
        return;
    }
    if(entry==0x827cba60u)
    {
        InsertFreeBlocks(m,Address(state.r[3]),Address(state.r[4]),
            Address(state.r[5]));
        return;
    }
    if(entry==0x82b7bc40u)
    {
        state.r[3]=FillGuestMemory(m,Address(state.r[3]),
            Address(state.r[4]),Address(state.r[5]));
        return;
    }
    unexpected_heap="unexpected direct heap lower";
}

void Services::CallNative(GuestAddress entry,GuestMemory&,
    heap_context::Registers& state)
{
    heap_events.push_back({entry,{state.r[3],state.r[4],state.r[5],
        state.r[6],state.r[7],state.r[1],state.lr}});
    switch(entry)
    {
    case 0x830da07cu:state.r[3]=1u;return;
    case 0x830d9c6cu:
    case 0x830d9c7cu:return;
    default:unexpected_heap="unexpected native heap lower";return;
    }
}

family::Dependencies FormatterDependencies(Services& s)
{return {{{s,s,s,s,{s,s,s,s,s,s,s,s},s,s},s},{{s,s},s},s,s,s};}

chain::Dependencies ChainDependencies(Services& s)
{return {s,s,FormatterDependencies(s),s,s};}

chain::Registers FromPpc(const PPCContext& c)
{
    chain::Registers s{};
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

void ToPpc(PPCContext& c,const chain::Registers& s)
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

heap_context::Registers HeapFromPpc(const PPCContext& context)
{
    const auto upper=FromPpc(context);
    heap_context::Registers state{};
    state.r=upper.r;
    state.r[1]=upper.sp;
    state.lr=upper.lr;state.ctr=upper.ctr;
    state.xer_so=upper.xer_so;state.xer_ca=upper.xer_ca;
    state.cr0={upper.cr0.lt,upper.cr0.gt,upper.cr0.eq,upper.cr0.so};
    state.cr6={upper.cr6.lt,upper.cr6.gt,upper.cr6.eq,upper.cr6.so};
    return state;
}

void HeapToPpc(PPCContext& context,const heap_context::Registers& state)
{
    auto upper=FromPpc(context);
    upper.r=state.r;
    upper.sp=state.r[1];
    upper.r[1]=0u;
    upper.lr=state.lr;upper.ctr=state.ctr;
    upper.xer_so=state.xer_so;upper.xer_ca=state.xer_ca;
    upper.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.un};
    upper.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.un};
    ToPpc(context,upper);
}

bool Same(const chain::Registers& a,const chain::Registers& b)
{
    return a.sp==b.sp && a.lr==b.lr && a.ctr==b.ctr &&
        a.r==b.r && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca &&
        a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt &&
        a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
        a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt &&
        a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}

enum class Scenario {LargeExact,LargeSplit,Grow,Locked};
struct Case {Scenario scenario;const char* format;const char* expected;};
constexpr std::array<Case,4> Cases{{
    {Scenario::LargeExact,"A","A"},
    {Scenario::LargeSplit,"%d","42"},
    {Scenario::Grow,"B","B"},
    {Scenario::Locked,"L","L"}}};

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
    a8_heap_fixture::Seed(memory);
    // The actual heap allocator indexes 128 small-list sentinels even when
    // the caller's first 2048-byte request selects the large list.
    for(unsigned units=1u;units<128u;++units)
    {
        const auto head=a8_heap_fixture::LargeHead+units*8u;
        memory.WriteU32(head,head);
        memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0u;word<4u;++word)
        memory.WriteU32(a8_heap_fixture::Heap+(88u+word)*4u,0u);
    if(item.scenario==Scenario::LargeSplit)
    {
        constexpr std::uint32_t split_units=160u;
        memory.WriteU16(a8_heap_fixture::Block,
            static_cast<std::uint16_t>(split_units));
        memory.WriteU32(a8_heap_fixture::Heap+48u,split_units);
        const auto old_next=a8_heap_fixture::Block+
            a8_heap_fixture::RoundedBytes;
        for(unsigned i=0u;i<6u;++i)
            memory.WriteU8(old_next+i,0u);
        const auto next=a8_heap_fixture::Block+split_units*16u;
        memory.WriteU16(next,1u);
        memory.WriteU16(next+2u,
            static_cast<std::uint16_t>(split_units));
        memory.WriteU8(next+4u,0u);
        memory.WriteU8(next+5u,1u);
    }
    if(item.scenario==Scenario::Grow)
    {
        memory.WriteU32(a8_heap_fixture::LargeHead,
            a8_heap_fixture::LargeHead);
        memory.WriteU32(a8_heap_fixture::LargeHead+4u,
            a8_heap_fixture::LargeHead);
        memory.WriteU32(a8_heap_fixture::Heap+48u,0u);
    }
    if(item.scenario==Scenario::Locked)
        memory.WriteU32(a8_heap_fixture::Heap+24u,0u);
    memory.WriteU32(0x83214fc8u+24u,0x8231a2a0u);
    memory.WriteU32(0x83214fc8u+32u,0x82b7eef8u);
    memory.WriteU32(0x83214fc8u+36u,0x82b7ee58u);
    memory.WriteU32(Environment+256u,ErrorState);
    memory.WriteU32(0x83214d74u,0x7200u);
    memory.WriteU32(0x83214d78u,1u);
    memory.WriteU32(Count,16u);
    memory.WriteU32(Blocks,0x70000u);
    memory.WriteU32(Record,0x3500u);
    memory.WriteU8(Record+4u,1u);
    memory.WriteU32(Record+8u,0x70000u);
    memory.WriteU32(Stream,Buffer);
    memory.WriteU32(Stream+8u,Buffer);
    memory.WriteU32(Stream+12u,0x42u);
    memory.WriteU32(Stream+4u,0x7fffffffu);
    memory.WriteU32(Stream+16u,0u);
    memory.WriteU32(Stream+24u,0x7fffffffu);
    for(std::size_t i=0;i<std::strlen(item.format);++i)
        memory.WriteU16(Format+2u*static_cast<GuestAddress>(i),
            static_cast<std::uint8_t>(item.format[i]));
    memory.WriteU16(Format+2u*static_cast<GuestAddress>(
        std::strlen(item.format)),0);
    memory.WriteU32(Object,Vtable);
    memory.WriteU32(Vtable+4u,VirtualTarget);
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    PPCRegister* fields[]={&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,
        &c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,
        &c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,
        &c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,
        &c.r27,&c.r28,&c.r29,&c.r30,&c.r31};
    for(unsigned i=0;i<32u;++i)
        fields[i]->u64=0x1122334400000000ull+i;
    c.r1.u64=0x8877665500000000ull|Stack;
    c.r3.u64=0xaabbccdd00000000ull|Object;
    c.r4.u64=0x9988776600000000ull|Format;
    c.r5.u64=0x556677880000002aull;
    c.r13.u64=Environment;
    c.lr=0xabcdef0123456789ull;
    c.ctr.u64=0x123456789abcdef0ull;
    c.xer.so=1;c.xer.ca=1;
    c.cr0.lt=1;c.cr6.gt=1;
    return c;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item);Seed(recovered,item);
    Services expected(original,Mode::WideBuffered),
        actual(recovered,Mode::WideBuffered);
    expected.expected_text=item.expected;
    actual.expected_text=item.expected;
    const auto payload=item.scenario==Scenario::Grow?
        GrowthBlock+16u:a8_heap_fixture::Payload;
    expected.expected_payload=payload;
    actual.expected_payload=payload;
    expected.grow=item.scenario==Scenario::Grow;
    actual.grow=expected.grow;
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    __imp__sub_824790A8(raw,original.Bytes());
    active=nullptr;
    const auto heap_count=[&](GuestAddress entry)
    {
        return std::count_if(expected.heap_events.begin(),
            expected.heap_events.end(),[&](const HeapEvent& event)
            {return event.entry==entry;});
    };
    if(expected.unexpected_heap || expected.virtual_calls!=1u ||
        expected.free_calls!=1u ||
        expected.memory.ReadU32(FreedSink)!=payload ||
        (item.scenario==Scenario::Grow &&
            heap_count(0x827cc428u)!=1) ||
        (item.scenario==Scenario::Locked &&
            (heap_count(0x830d9c6cu)!=1 ||
                heap_count(0x830d9c7cu)!=1)) ||
        (item.scenario==Scenario::LargeSplit &&
            expected.memory.ReadU16(a8_heap_fixture::Block+
                a8_heap_fixture::RoundedBytes)!=31u))
    {
        std::fprintf(stderr,"EXPECT A8 scenario=%u virtual=%u free=%u sink=%08X\n",
            static_cast<unsigned>(item.scenario),expected.virtual_calls,
            expected.free_calls,expected.memory.ReadU32(FreedSink));
        throw std::runtime_error("independent A8 business path");
    }
    if(!full_chain::Apply(0x824790a8u,actual.memory,
            {ChainDependencies(actual),actual},state))
        throw std::runtime_error("A8 recovered entry missing");
    if(actual.unexpected_heap)
        throw std::runtime_error(actual.unexpected_heap);
    const auto observed=FromPpc(raw);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events ||
        expected.heap_events!=actual.heap_events ||
        expected.virtual_calls!=actual.virtual_calls ||
        expected.free_calls!=actual.free_calls)
    {
        std::fprintf(stderr,"FAIL A8 scenario=%u state=%u RAM=%u events=%zu/%zu "
            "virtual=%u/%u free=%u/%u r3=%llX/%llX SP=%llX/%llX "
            "LR=%llX/%llX CR0=%u%u%u%u/%u%u%u%u "
            "CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n",
            static_cast<unsigned>(item.scenario),same_state,same_ram,
            expected.events.size(),actual.events.size(),
            expected.virtual_calls,actual.virtual_calls,
            expected.free_calls,actual.free_calls,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,observed.cr0.so,
            state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so,
            observed.cr6.lt,observed.cr6.gt,observed.cr6.eq,observed.cr6.so,
            state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so,
            observed.xer_ca,state.xer_ca);
        for(unsigned i=0;i<32u;++i)
            if(observed.r[i]!=state.r[i])
                std::fprintf(stderr," r%u=%llX/%llX",i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n',stderr);
        unsigned shown=0;
        for(const auto region:Regions)
        {
            for(std::size_t offset=0;offset<region.size;++offset)
            {
                const auto address=region.base+
                    static_cast<GuestAddress>(offset);
                if(original.Bytes()[address]==recovered.Bytes()[address])
                    continue;
                std::fprintf(stderr," RAM %08X original=%02X recovered=%02X\n",
                    address,original.Bytes()[address],
                    recovered.Bytes()[address]);
                if(++shown==8u) break;
            }
            if(shown==8u) break;
        }
        return false;
    }
    return true;
}
} // namespace

void OriginalSave27(PPCContext& context,std::uint8_t*)
{
    auto memory=active->memory;
    for(unsigned i=27u;i<=31u;++i)
    {
        PPCRegister* fields[]={&context.r27,&context.r28,&context.r29,
            &context.r30,&context.r31};
        WriteU64(memory,Address(context.r1.u64-8u*(33u-i)),
            fields[i-27u]->u64);
    }
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}

void OriginalRestore27(PPCContext& context,std::uint8_t*)
{
    auto memory=active->memory;
    PPCRegister* fields[]={&context.r27,&context.r28,&context.r29,
        &context.r30,&context.r31};
    for(unsigned i=27u;i<=31u;++i)
        fields[i-27u]->u64=ReadU64(memory,
            Address(context.r1.u64-8u*(33u-i)));
    context.r12.u64=memory.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

void OriginalSaveFrom(PPCContext& context,unsigned first)
{
    auto memory=active->memory;
    auto state=FromPpc(context);
    for(unsigned i=first;i<=31u;++i)
        WriteU64(memory,Address(context.r1.u64-8u*(33u-i)),state.r[i]);
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}

void OriginalRestoreFrom(PPCContext& context,unsigned first)
{
    auto memory=active->memory;
    auto state=FromPpc(context);
    for(unsigned i=first;i<=31u;++i)
        state.r[i]=ReadU64(memory,
            Address(context.r1.u64-8u*(33u-i)));
    ToPpc(context,state);
    context.r12.u64=memory.ReadU32(Address(context.r1.u64-8u));
    context.lr=context.r12.u64;
}

void OriginalSave28(PPCContext& context,std::uint8_t*)
{OriginalSaveFrom(context,28u);}
void OriginalRestore28(PPCContext& context,std::uint8_t*)
{OriginalRestoreFrom(context,28u);}
void OriginalSave22(PPCContext& context,std::uint8_t*)
{OriginalSaveFrom(context,22u);}
void OriginalRestore22(PPCContext& context,std::uint8_t*)
{OriginalRestoreFrom(context,22u);}

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
    std::uint8_t* base)
{
    switch(address)
    {
    case 0x823acad8u:__imp__sub_823ACAD8(context,base);return;
    case 0x823acbd0u:__imp__sub_823ACBD0(context,base);return;
    case 0x823accb0u:__imp__sub_823ACCB0(context,base);return;
    case 0x823acc98u:__imp__sub_823ACC98(context,base);return;
    case 0x823ad544u:__imp__sub_823AD544(context,base);return;
    case 0x82b7d158u:__imp__sub_82B7D158(context,base);return;
    case 0x82b7d020u:__imp__sub_82B7D020(context,base);return;
    case 0x82319330u:__imp__sub_82319330(context,base);return;
    case 0x823addc0u:__imp__sub_823ADDC0(context,base);return;
    case 0x823ade28u:
    {
        auto state=FromPpc(context);
        active->Call(address,active->memory,state);
        ToPpc(context,state);
        return;
    }
    case 0x827cc428u:
    case 0x827cba60u:
    case 0x82b7bc40u:
    {
        auto state=HeapFromPpc(context);
        active->CallDirect(address,active->memory,state);
        HeapToPpc(context,state);
        return;
    }
    default:break;
    }
    auto state=FromPpc(context);
    auto deps=FormatterDependencies(*active);
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
        if(!lo::semantic::read_only_fields::Apply(address,call,
                active->memory))
            throw std::runtime_error("formatter character model missing");
        state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
        state.r[8]=call.r8;state.r[9]=call.r9;state.r[10]=call.r10;
        state.r[11]=call.r11;state.r[13]=call.r13;state.r[18]=call.r18;
    }
    else if(crt_float_formatting::Apply(address,active->memory,
                deps.floating,state) ||
        crt_format_dispatch::Apply(address,active->memory,
            deps.output.streams.locks,state))
    {}
    else
        throw std::runtime_error("A8 original direct callee unhandled");
    ToPpc(context,state);
}

void OriginalVirtual(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active->Call(Address(context.ctr.u64)&~GuestAddress{3},
        active->memory,state);
    ToPpc(context,state);
}

void OriginalIndirect(GuestAddress target,PPCContext& context,
    std::uint8_t* base)
{
    if(target==VirtualTarget)
        OriginalVirtual(context,base);
    else
        OriginalAccepted(target,context,base);
}

void OriginalHeapNative(GuestAddress address,PPCContext& context,
    std::uint8_t*)
{
    auto state=HeapFromPpc(context);
    active->CallNative(address,active->memory,state);
    HeapToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        std::printf("PASS legacy-config-format-full-heap-chain %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected full heap control flow with explicit grow/native boundaries; accepted deep formatter and mutable heap-free semantics; faults/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
