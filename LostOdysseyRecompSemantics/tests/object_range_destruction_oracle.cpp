// Appended after the pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/object_range_destruction.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family=object_range_destruction;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack=0x80000u,Objects=0x30000u;
constexpr GuestAddress VTableA=0x50000u,VTableB=0x50100u;
constexpr GuestAddress VTableC=0x50200u;
constexpr std::array<Region,1> Regions{{{0u,0x90000u}}};

enum class Mode {Empty,Two,ZeroSkip};
struct Case {GuestAddress entry;std::uint32_t stride;Mode mode;
    GuestAddress callback_return;};
#ifdef LO_OBJECT_RANGE_EXTENSION_ONLY
constexpr std::array<Case,4> Cases{{
    {0x82b90340u,20u,Mode::Empty,0x82b90380u},
    {0x82b90340u,20u,Mode::Two,0x82b90380u},
    {0x82b92068u,48u,Mode::Empty,0x82b920a8u},
    {0x82b92068u,48u,Mode::Two,0x82b920a8u}}};
#else
constexpr std::array<Case,9> Cases{{
    {0x82b8bde0u,36u,Mode::Empty,0x82b8be20u},
    {0x82b8bde0u,36u,Mode::Two,0x82b8be20u},
    {0x82b8be48u,88u,Mode::Empty,0x82b8be88u},
    {0x82b8be48u,88u,Mode::Two,0x82b8be88u},
    {0x82b8dbb0u,40u,Mode::Empty,0x82b8dbf0u},
    {0x82b8dbb0u,40u,Mode::Two,0x82b8dbf0u},
    {0x82b8dc18u,32u,Mode::Empty,0x82b8dc58u},
    {0x82b8dc18u,32u,Mode::Two,0x82b8dc58u},
    {0x82b8bde0u,36u,Mode::ZeroSkip,0x82b8be20u}}};
#endif

using Event=std::array<std::uint64_t,7>;
struct Services final:family::DynamicServices
{
    GuestMemory memory;
    GuestAddress second;
    std::vector<Event> events;
    Services(GuestWindow& window,GuestAddress next)
        :memory(window.Memory()),second(next){}
    void CallGuestDestructor(GuestAddress target,GuestMemory& guest,
        family::Registers& state) override
    {
        events.push_back({target,Address(state.r[3]),state.r[4],
            state.sp,state.lr,state.ctr,
            guest.ReadU32(Address(state.r[3])+12u)});
        const auto ordinal=static_cast<std::uint32_t>(events.size());
        guest.WriteU32(Address(state.r[3])+12u,0xabc00000u+ordinal);
        if(ordinal==1u)
            guest.WriteU32(second,VTableC);
        state.r[5]=0xfeed000000000000ull+ordinal;
        state.r[10]=0xface000000000000ull+ordinal;
        state.r[11]=0xbed0000000000000ull+ordinal;
        state.ctr=0xbabe000000000000ull+ordinal;
        state.lr=0xcafe000000000000ull+ordinal;
        state.cr0={std::uint8_t(ordinal==2u),
            std::uint8_t(ordinal==1u),0,0};
        state.cr6={1,0,0,0};
        state.xer_so=std::uint8_t(ordinal==2u);
        state.xer_ca=std::uint8_t(ordinal==1u);
    }
};
Services* active=nullptr;

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0;index<32u;++index)
        state.r[index]=index==1u?0u:fields[index]->u64;
    state.sp=context.r1.u64;state.lr=context.lr;
    state.ctr=context.ctr.u64;
    state.xer_so=context.xer.so;state.xer_ca=context.xer.ca;
    state.cr0={std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt),std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6={std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt),std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}
void ToPpc(PPCContext& context,const family::Registers& state)
{
    PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0;index<32u;++index)
        fields[index]->u64=state.r[index];
    context.r1.u64=state.sp;context.lr=state.lr;
    context.ctr.u64=state.ctr;
    context.xer.so=state.xer_so;context.xer.ca=state.xer_ca;
    context.cr0={bool(state.cr0.lt),bool(state.cr0.gt),
        bool(state.cr0.eq),{bool(state.cr0.so)}};
    context.cr6={bool(state.cr6.lt),bool(state.cr6.gt),
        bool(state.cr6.eq),{bool(state.cr6.so)}};
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
    auto memory=window.Memory();
    const auto second=Objects+item.stride;
    memory.WriteU32(Objects,VTableA);
    memory.WriteU32(second,VTableB);
    memory.WriteU32(VTableA+8u,0x60003u);
    memory.WriteU32(VTableB+8u,0x61003u);
    memory.WriteU32(VTableC+8u,0x62001u);
    memory.WriteU32(Objects+12u,0x11110001u);
    memory.WriteU32(second+12u,0x11110002u);
}
PPCContext Initial(const Case& item)
{
    PPCContext context{};
    PPCRegister* fields[]={&context.r0,&context.r1,&context.r2,
        &context.r3,&context.r4,&context.r5,&context.r6,&context.r7,
        &context.r8,&context.r9,&context.r10,&context.r11,&context.r12,
        &context.r13,&context.r14,&context.r15,&context.r16,&context.r17,
        &context.r18,&context.r19,&context.r20,&context.r21,&context.r22,
        &context.r23,&context.r24,&context.r25,&context.r26,&context.r27,
        &context.r28,&context.r29,&context.r30,&context.r31};
    for(unsigned index=0;index<32u;++index)
        fields[index]->u64=0x1122334400000000ull+index;
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef01u;
    context.ctr.u64=0x5555666677778888ull;
    context.r3.u64=item.mode==Mode::ZeroSkip?0u:Objects;
    context.r4.u64=item.mode==Mode::Empty?Objects:
        item.mode==Mode::ZeroSkip?36u:Objects+2u*item.stride;
    context.cr0.gt=1;context.cr6.lt=1;
    context.xer.so=1;context.xer.ca=1;
    return context;
}
bool ExpectedPath(const Case& item,const Services& services)
{
    const auto memory=services.memory;
    if(item.mode!=Mode::Two)
        return services.events.empty() &&
            memory.ReadU32(Objects+12u)==0x11110001u;
    const auto second=Objects+item.stride;
    const auto sp=0x1234567800000000ull|Stack;
    return services.events.size()==2u &&
        services.events[0]==Event{0x60000u,Objects,0u,sp-112u,
            item.callback_return,0x60003u,0x11110001u} &&
        services.events[1]==Event{0x62000u,second,0u,sp-112u,
            item.callback_return,0x62001u,0x11110002u} &&
        memory.ReadU32(second)==VTableC &&
        memory.ReadU32(Objects+12u)==0xabc00001u &&
        memory.ReadU32(second+12u)==0xabc00002u;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,item);Seed(recovered,item);
    Services expected(original,Objects+item.stride),
        actual(recovered,Objects+item.stride);
    auto raw=Initial(item);
    auto state=FromPpc(raw);
    active=&expected;
    std::fprintf(stderr,"range original entry=%08X mode=%u stride=%u\n",
        item.entry,static_cast<unsigned>(item.mode),item.stride);
    switch(item.entry)
    {
    case 0x82b8bde0u:__imp__sub_82B8BDE0(raw,original.Bytes());break;
    case 0x82b8be48u:__imp__sub_82B8BE48(raw,original.Bytes());break;
    case 0x82b8dbb0u:__imp__sub_82B8DBB0(raw,original.Bytes());break;
    case 0x82b8dc18u:__imp__sub_82B8DC18(raw,original.Bytes());break;
    case 0x82b90340u:__imp__sub_82B90340(raw,original.Bytes());break;
    case 0x82b92068u:__imp__sub_82B92068(raw,original.Bytes());break;
    default:throw std::runtime_error("unknown range entry");
    }
    active=nullptr;
    if(!ExpectedPath(item,expected))
    {
        std::fprintf(stderr,"range original fixture path entry=%08X mode=%u calls=%zu first-target=%llX\n",
            item.entry,static_cast<unsigned>(item.mode),
            expected.events.size(),
            static_cast<unsigned long long>(expected.events.empty()?0u:
                expected.events.front()[0]));
        throw std::runtime_error("range fixture missed original path");
    }
    if(!family::Apply(item.entry,actual.memory,actual,state))
        throw std::runtime_error("range recovered entry missing");
    const auto observed=FromPpc(raw);
    if(!Same(observed,state) || expected.events!=actual.events ||
        !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,"FAIL range %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX ctr=%llX/%llX RAM=%u events=%zu/%zu\n",
            item.entry,static_cast<unsigned>(item.mode),
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
        for(unsigned index=0;index<32u;++index)
            if(observed.r[index]!=state.r[index])
                std::fprintf(stderr," r%u=%llX/%llX",index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr,"\n");
        return false;
    }
    return true;
}
} // namespace

void OriginalIndirect(GuestAddress target,PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active->CallGuestDestructor(target,active->memory,state);
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:Cases)
            if(!Check(item)) return 1;
        std::printf("PASS object-range-destruction %zu focused original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ABI/RAM and dynamic destructor boundary; callee internals, faults, MMIO and runtime remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
