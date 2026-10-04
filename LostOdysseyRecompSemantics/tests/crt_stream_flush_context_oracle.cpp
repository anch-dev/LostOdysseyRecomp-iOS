// Appended after the pinned flush, wrapper, status and pointer-unlock bodies.
#include "lo_semantics/crt_stream_flush_context.h"
#include "crt_stream_oracle_fixture.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace flush=lo::semantic::gpu::crt_stream_flush_context;
using FlushEvent=std::array<std::uint64_t,4>;

struct FlushService final : flush::NativeServices
{
    bool fail;
    std::vector<FlushEvent> events;
    explicit FlushService(bool failed):fail(failed){}
    void NtFlushBuffersFile(GuestMemory&,flush::Registers& s) override
    {
        events.push_back({s.sp,s.lr,s.r[3],s.r[4]});
        s.r[3]=fail?0xffffffffc0000017ull:0u;
    }
};

FlushService* flush_active=nullptr;

enum class Path {MinusTwo,OutOfRange,Success,NativeFailure,
    InvalidAfterLock,Wrapper};
constexpr std::array Paths{Path::MinusTwo,Path::OutOfRange,
    Path::Success,Path::NativeFailure,Path::InvalidAfterLock,
    Path::Wrapper};

Mode ServiceMode(Path path)
{
    if(path==Path::NativeFailure)return Mode::NativeFailureFive;
    if(path==Path::InvalidAfterLock)return Mode::InvalidAfterLock;
    return Mode::LockedWrite;
}

PPCContext Initial(Path path,GuestMemory& memory)
{
    PPCContext c{};
    PPCRegister* fields[]={&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,
        &c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,
        &c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,
        &c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,
        &c.r30,&c.r31};
    for(unsigned index=0;index<32;++index)
        fields[index]->u64=0x1122334400000000ull+index;
    c.r1.u64=0x8877665500000000ull|Stack;
    c.r13.u64=0xaabbccdd00000000ull|Environment;
    c.r3.u64=0xaabbccdd00000005ull;
    if(path==Path::MinusTwo)c.r3.u64=0xaabbccddfffffffeull;
    if(path==Path::OutOfRange)c.r3.u64=0xaabbccdd00000010ull;
    c.lr=0xabcdef0181234567ull;
    c.ctr.u64=0x5566778899aabbccull;
    c.xer.so=1;c.xer.ca=1;c.cr0.lt=1;c.cr6.gt=1;
    if(path==Path::Wrapper)
    {
        c.r1.u64-=144u;
        c.r12.u64=0x8877665500000000ull|Stack;
        c.r27.u64=0xaabbccdd00000005ull;
        memory.WriteU32(Stack+20u,5u);
    }
    return c;
}

bool Independent(Path path,const PPCContext& c,const Services& s,
    const FlushService& flush_service)
{
    switch(path)
    {
    case Path::MinusTwo:
        return c.r3.u64==UINT64_MAX && s.locks==0 && s.unlocks==0 &&
            s.traps==0 && flush_service.events.empty() &&
            s.memory.ReadU32(0x83215210u)==9u;
    case Path::OutOfRange:
        return c.r3.u64==UINT64_MAX && s.locks==0 && s.unlocks==0 &&
            s.traps==1 && flush_service.events.empty() &&
            s.memory.ReadU32(0x83215210u)==9u;
    case Path::Success:
        return c.r3.u32==0u && s.locks==1 && s.unlocks==1 &&
            flush_service.events.size()==1u;
    case Path::NativeFailure:
        return c.r3.u32==UINT32_MAX && s.locks==1 && s.unlocks==1 &&
            flush_service.events.size()==1u && s.converted_errors==1 &&
            s.memory.ReadU32(ErrorState+352u)==5u &&
            s.memory.ReadU32(0x83215210u)==9u;
    case Path::InvalidAfterLock:
        return c.r3.u32==UINT32_MAX && s.locks==1 && s.unlocks==1 &&
            flush_service.events.empty() &&
            s.memory.ReadU32(0x83215210u)==9u;
    case Path::Wrapper:
        return s.unlocks==1 && s.locks==0 &&
            flush_service.events.empty() && c.r3.u32==Record+12u;
    }
    return false;
}

void Check(Path path)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,ServiceMode(path));Seed(recovered,ServiceMode(path));
    auto original_memory=original.Memory();
    auto recovered_memory=recovered.Memory();
    auto raw=Initial(path,original_memory);
    recovered_memory.WriteU32(Stack+20u,
        original_memory.ReadU32(Stack+20u));
    auto state=FromPpc(raw);
    Services expected(original,ServiceMode(path));
    Services actual(recovered,ServiceMode(path));
    FlushService expected_flush(path==Path::NativeFailure);
    FlushService actual_flush(path==Path::NativeFailure);
    active=&expected;flush_active=&expected_flush;
    if(path==Path::Wrapper)
        __imp__sub_82B820C4(raw,original.Bytes());
    else __imp__sub_82B81F78(raw,original.Bytes());
    active=nullptr;flush_active=nullptr;
    if(!Independent(path,raw,expected,expected_flush))
    {
        std::fprintf(stderr,"EXPECT flush path=%u r3=%llX locks=%u "
            "unlocks=%u traps=%u native=%zu errno=%08X last=%08X\n",
            static_cast<unsigned>(path),
            static_cast<unsigned long long>(raw.r3.u64),expected.locks,
            expected.unlocks,expected.traps,expected_flush.events.size(),
            expected.memory.ReadU32(0x83215210u),
            expected.memory.ReadU32(ErrorState+352u));
        throw std::runtime_error("independent stream flush path");
    }
    const GuestAddress entry=path==Path::Wrapper?
        0x82b820c4u:0x82b81f78u;
    if(!flush::Apply(entry,actual.memory,Dependencies(actual),
        actual_flush,state))
        throw std::runtime_error("missing stream flush entry");
    const auto observed=FromPpc(raw);
    const bool same_state=Same(observed,state);
    const bool same_ram=original.EqualCommitted(recovered);
    if(!same_state || !same_ram || expected.events!=actual.events ||
        expected_flush.events!=actual_flush.events)
    {
        std::fprintf(stderr,"FAIL flush path=%u state=%u RAM=%u "
            "accepted-events=%zu/%zu native-events=%zu/%zu\n",
            static_cast<unsigned>(path),same_state,same_ram,
            expected.events.size(),actual.events.size(),
            expected_flush.events.size(),actual_flush.events.size());
        for(unsigned index=0;index<32;++index)
            if(observed.r[index]!=state.r[index])
                std::fprintf(stderr," r%u=%llX/%llX\n",index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr," SP=%llX/%llX LR=%llX/%llX "
            "CTR=%llX/%llX CR0=%u%u%u%u/%u%u%u%u "
            "CR6=%u%u%u%u/%u%u%u%u SO=%u/%u CA=%u/%u\n",
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,observed.cr0.so,
            state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so,
            observed.cr6.lt,observed.cr6.gt,observed.cr6.eq,observed.cr6.so,
            state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so,
            observed.xer_so,state.xer_so,observed.xer_ca,state.xer_ca);
        throw std::runtime_error("stream flush selected context differs");
    }
}
} // namespace

void OriginalFlushImport(PPCContext& c,std::uint8_t*)
{
    auto state=FromPpc(c);
    flush_active->NtFlushBuffersFile(active->memory,state);
    ToPpc(c,state);
}

void OriginalStatusNative(PPCContext& c,std::uint8_t*)
{
    crt_status_error::Registers lower{};
    lower.sp=c.r1.u64;lower.lr=c.lr;lower.r3=c.r3.u64;
    lower.r11=c.r11.u64;lower.r12=c.r12.u64;lower.r13=c.r13.u64;
    lower.xer_so=c.xer.so;
    lower.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    active->NtStatusToDosError(active->memory,lower);
    c.r3.u64=lower.r3;
}

void OriginalLeaveNative(PPCContext& c,std::uint8_t*)
{
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=c.r1.u64;lower.lr=c.lr;lower.r3=c.r3.u64;
    lower.r9=c.r9.u64;lower.r10=c.r10.u64;
    lower.r11=c.r11.u64;lower.r12=c.r12.u64;
    lower.r30=c.r30.u64;lower.r31=c.r31.u64;
    lower.xer_ca=c.xer.ca;
    active->LeaveCriticalSection(active->memory,lower);
}

int main()
{
    try
    {
        for(const auto path:Paths) Check(path);
        std::printf("PASS crt-stream-flush-context %zu original PPC cases\n",
            Paths.size());
        std::puts("LIMIT three new full PPC bodies plus original selected status/pointer leaves; accepted CRT lower semantics, native flush, unselected CR, faults, MMIO, concurrency and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
