#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/crt_stream_close_caller.h"

namespace
{
namespace close_caller = crt_stream_close_caller;

enum class CallerMode
{SpecialMinusTwo,OutOfRange,Inactive,LockedSuccess,LockedFailure,DroppedAfterLock};
struct CallerCase{const char* name;CallerMode mode;};
constexpr std::array CallerCases{
    CallerCase{"minus-two-error",CallerMode::SpecialMinusTwo},
    CallerCase{"out-of-range-invalid",CallerMode::OutOfRange},
    CallerCase{"inactive-record-invalid",CallerMode::Inactive},
    CallerCase{"locked-close-success",CallerMode::LockedSuccess},
    CallerCase{"locked-close-native-failure",CallerMode::LockedFailure},
    CallerCase{"flag-dropped-after-lock",CallerMode::DroppedAfterLock}};

using GuestEvent=std::array<std::uint64_t,5>;
struct GuestCalls final : crt_stream_close_error::GuestServices
{
    CallerMode mode;
    std::vector<GuestEvent> events;
    explicit GuestCalls(CallerMode selected):mode(selected){}
    void CallIndirect(GuestMemory&,GuestAddress target,
        close_caller::Registers& state) override
    {
        if(target!=0x2600u)throw std::runtime_error("unexpected close target");
        events.push_back({target,state.sp,state.lr,state.ctr,state.r[3]});
        state.r[11]=0xfedcba9800000011ull;
        state.r[3]=mode==CallerMode::LockedFailure?
            0xffffffffc0000017ull:0x1234567800000000ull;
    }
};

GuestCalls* active_guest=nullptr;

void SeedCaller(GuestWindow& window,const CallerCase& item)
{
    const auto mode=item.mode==CallerMode::LockedFailure?
        Mode::NativeFailureFive:item.mode==CallerMode::DroppedAfterLock?
        Mode::InvalidAfterLock:Mode::Binary;
    Seed(window,mode);
    auto memory=window.Memory();
    memory.WriteU32(0x34004u,0x2601u);
    if(item.mode==CallerMode::Inactive)
        memory.WriteU8(Record+4u,0u);
}

PPCContext InitialCaller(const CallerCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef0123456789ull;
    context.r13.u64=Environment;
    context.r3.u64=item.mode==CallerMode::SpecialMinusTwo?UINT64_MAX-1u:
        item.mode==CallerMode::OutOfRange?17u:
        0x1122334400000005ull;
    context.r27.u64=0x2711223344556677ull;
    context.r28.u64=0x2811223344556677ull;
    context.r29.u64=0x2911223344556677ull;
    context.r30.u64=0x3011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.xer.so=1;context.xer.ca=1;
    return context;
}

void CheckCaller(const CallerCase& item)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedCaller(original,item);SeedCaller(recovered,item);
    const auto mode=item.mode==CallerMode::LockedFailure?
        Mode::NativeFailureFive:item.mode==CallerMode::DroppedAfterLock?
        Mode::InvalidAfterLock:Mode::Binary;
    Services expected(original,mode),actual(recovered,mode);
    GuestCalls original_guest(item.mode),recovered_guest(item.mode);
    PPCContext context=InitialCaller(item);
    auto state=FromPpc(context);
    active=&expected;active_guest=&original_guest;
    __imp__sub_82B87B18(context,original.Bytes());
    active_guest=nullptr;active=nullptr;
    if(!close_caller::Apply(0x82b87b18u,actual.memory,
            {Dependencies(actual),recovered_guest},state))
        throw std::runtime_error("missing locked close entry");
    if(!Same(FromPpc(context),state)||
        original_guest.events!=recovered_guest.events||
        expected.events!=actual.events||
        expected.locks!=actual.locks||expected.unlocks!=actual.unlocks||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("selected context, events or RAM mismatch");

    const auto calls=original_guest.events.size();
    const auto memory=expected.memory;
    switch(item.mode)
    {
    case CallerMode::SpecialMinusTwo:
        if(context.r3.u64!=UINT64_MAX||calls!=0u||expected.locks!=0u||
            memory.ReadU32(0x83215210u)!=9u)
            throw std::runtime_error("special invalid handle path");
        break;
    case CallerMode::OutOfRange:
    case CallerMode::Inactive:
        if(context.r3.u64!=UINT64_MAX||calls!=0u||expected.locks!=0u||
            expected.traps!=1u)
            throw std::runtime_error("invalid handle parameter path");
        break;
    case CallerMode::LockedSuccess:
        if(context.r3.u64!=0u||calls!=1u||expected.locks!=1u||
            expected.unlocks!=1u||memory.ReadU32(Record)!=UINT32_MAX||
            memory.ReadU8(Record+4u)!=0u)
            throw std::runtime_error("locked close composed path");
        break;
    case CallerMode::LockedFailure:
        if(context.r3.u64!=UINT32_MAX||calls!=1u||expected.locks!=1u||
            expected.unlocks!=1u||expected.converted_errors!=1u)
            throw std::runtime_error("locked close status path");
        break;
    case CallerMode::DroppedAfterLock:
        if(context.r3.u64!=UINT32_MAX||calls!=0u||expected.locks!=1u||
            expected.unlocks!=1u||memory.ReadU32(0x83215210u)!=9u)
            throw std::runtime_error("lock-time flag recheck path");
        break;
    }
}
} // namespace

void OriginalPointerLeaf(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=state.sp;lower.lr=state.lr;
    lower.r3=state.r[3];lower.r9=state.r[9];
    lower.r10=state.r[10];lower.r11=state.r[11];
    lower.r12=state.r[12];lower.r30=state.r[30];
    lower.r31=state.r[31];lower.xer_ca=state.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,active->memory,
            *active,lower))
        throw std::runtime_error("missing accepted pointer leaf");
    state.sp=lower.sp;state.lr=lower.lr;
    state.r[3]=lower.r3;state.r[9]=lower.r9;
    state.r[10]=lower.r10;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[30]=lower.r30;
    state.r[31]=lower.r31;state.xer_ca=lower.xer_ca;
    ToPpc(context,state);
}

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
        for(const auto& item:CallerCases)
        {
            try{CheckCaller(item);}
            catch(const std::exception& error)
            {
                std::fprintf(stderr,"%s: %s\n",item.name,error.what());
                return 1;
            }
        }
        std::printf("PASS crt-stream-close-caller %zu actual PPC cases\n",
            CallerCases.size());
        std::puts("LIMIT composed close and accepted lock/error leaves; mutable guest dispatch and native lock boundary remain selected");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
