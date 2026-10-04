#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_close_error.h"
#include "lo_semantics/crt_status_error.h"

namespace
{
namespace close_error = crt_stream_close_error;

enum class CloseMode
{
    DirectNegative, DirectRelease, Normal, NativeFailure,
    PairedSkip, InvalidHandle
};
struct CloseCase
{
    const char* name;
    GuestAddress address;
    CloseMode mode;
};
constexpr std::array CloseCases{
    CloseCase{"direct-negative-status",0x82be1b80u,CloseMode::DirectNegative},
    CloseCase{"direct-release-live-handle",0x82b86190u,CloseMode::DirectRelease},
    CloseCase{"close-high-half-success",0x82b87a38u,CloseMode::Normal},
    CloseCase{"close-negative-status",0x82b87a38u,CloseMode::NativeFailure},
    CloseCase{"close-paired-descriptor",0x82b87a38u,CloseMode::PairedSkip},
    CloseCase{"close-out-of-range",0x82b87a38u,CloseMode::InvalidHandle}};

using CloseEvent=std::array<std::uint64_t,5>;
struct GuestCalls final : close_error::GuestServices
{
    CloseMode mode;
    std::vector<CloseEvent> events;
    explicit GuestCalls(CloseMode selected):mode(selected){}
    void CallIndirect(GuestMemory&,GuestAddress target,
        close_error::Registers& state) override
    {
        if(target!=0x2600u)throw std::runtime_error("unexpected close guest target");
        events.push_back({target,state.sp,state.lr,state.ctr,state.r[3]});
        state.r[11]=0xfedcba9800000011ull;
        state.r[3]=(mode==CloseMode::NativeFailure||
            mode==CloseMode::DirectNegative)?0xffffffffc0000017ull:
            0x1234567800000000ull;
    }
};

GuestCalls* active_guest=nullptr;

void SeedClose(GuestWindow& window,const CloseCase& item)
{
    const auto native_failure=item.mode==CloseMode::NativeFailure||
        item.mode==CloseMode::DirectNegative;
    Seed(window,native_failure?Mode::NativeFailureFive:Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(0x34004u,0x2601u);
    if(item.mode==CloseMode::PairedSkip)
    {
        memory.WriteU32(0x30040u,0x3500u);
        memory.WriteU8(0x30044u,1u);
        memory.WriteU32(0x30080u,0x3500u);
        memory.WriteU8(0x30084u,1u);
    }
}

PPCContext InitialClose(const CloseCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef0123456789ull;
    context.r13.u64=Environment;
    context.r3.u64=item.mode==CloseMode::PairedSkip?1u:
        item.mode==CloseMode::InvalidHandle?17u:
        0x1122334400000005ull;
    context.r29.u64=0xfedcba989abcdef0ull;
    context.r30.u64=0x8877665544332211ull;
    context.r31.u64=0x7766554433221100ull;
    context.xer.so=1;
    context.xer.ca=1;
    return context;
}

void CheckClose(const CloseCase& item)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedClose(original,item);SeedClose(recovered,item);
    const auto service_mode=(item.mode==CloseMode::NativeFailure||
        item.mode==CloseMode::DirectNegative)?Mode::NativeFailureFive:Mode::Binary;
    Services expected(original,service_mode),actual(recovered,service_mode);
    GuestCalls original_guest(item.mode),recovered_guest(item.mode);
    PPCContext context=InitialClose(item);
    auto state=FromPpc(context);
    active=&expected;active_guest=&original_guest;
    switch(item.address)
    {
    case 0x82b87a38u:__imp__sub_82B87A38(context,original.Bytes());break;
    case 0x82be1b80u:__imp__sub_82BE1B80(context,original.Bytes());break;
    case 0x82b86190u:__imp__sub_82B86190(context,original.Bytes());break;
    default:throw std::runtime_error("unknown close entry");
    }
    active_guest=nullptr;active=nullptr;
    if(!close_error::Apply(item.address,actual.memory,
            {Dependencies(actual),recovered_guest},state))
        throw std::runtime_error("missing close semantic entry");
    if(!Same(FromPpc(context),state)||
        original_guest.events!=recovered_guest.events||
        expected.events!=actual.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("selected context, events or RAM mismatch");

    const auto memory=expected.memory;
    const auto calls=original_guest.events.size();
    switch(item.mode)
    {
    case CloseMode::DirectNegative:
        if(calls!=1u||context.r3.u64!=0u||expected.converted_errors!=1u||
            memory.ReadU32(ErrorState+352u)!=5u)
            throw std::runtime_error("negative status conversion path");
        break;
    case CloseMode::DirectRelease:
        if(calls!=0u||context.r3.u64!=0u||memory.ReadU32(Record)!=UINT32_MAX)
            throw std::runtime_error("direct descriptor release path");
        break;
    case CloseMode::Normal:
        if(calls!=1u||context.r3.u64!=0u||
            memory.ReadU32(Record)!=UINT32_MAX||memory.ReadU8(Record+4u)!=0u)
            throw std::runtime_error("successful close path");
        break;
    case CloseMode::NativeFailure:
        if(calls!=1u||context.r3.u64!=UINT64_MAX||
            expected.converted_errors!=1u||memory.ReadU8(Record+4u)!=0u)
            throw std::runtime_error("failed close error path");
        break;
    case CloseMode::PairedSkip:
        if(calls!=0u||context.r3.u64!=0u||
            memory.ReadU32(0x30040u)!=UINT32_MAX||
            memory.ReadU8(0x30044u)!=0u||memory.ReadU8(0x30084u)!=1u)
            throw std::runtime_error("paired close skip path");
        break;
    case CloseMode::InvalidHandle:
        if(calls!=0u||context.r3.u64!=0u||expected.traps==0u)
            throw std::runtime_error("invalid handle cleanup path");
        break;
    }
}
} // namespace

void OriginalStatus(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    crt_status_error::Registers lower{};
    lower.sp=state.sp;lower.lr=state.lr;
    lower.r3=state.r[3];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r13=state.r[13];
    lower.xer_so=state.xer_so;
    lower.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    if(!crt_status_error::Apply(0x827ca628u,active->memory,*active,lower))
        throw std::runtime_error("missing accepted status conversion");
    state.sp=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[13]=lower.r13;
    state.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
    ToPpc(context,state);
}

void OriginalCloseIndirect(std::uint32_t target,PPCContext& context,
    std::uint8_t*)
{
    auto state=FromPpc(context);
    active_guest->CallIndirect(active->memory,target,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:CloseCases)
        {
            try{CheckClose(item);}
            catch(const std::exception& error)
            {
                std::fprintf(stderr,"%s: %s\n",item.name,error.what());
                return 1;
            }
        }
        std::printf("PASS crt-stream-close-error %zu actual PPC cases\n",
            CloseCases.size());
        std::puts("LIMIT mutable guest dispatch selected context; faults, MMIO, concurrency and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
