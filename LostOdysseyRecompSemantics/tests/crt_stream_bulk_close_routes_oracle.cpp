#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_bulk_close_routes.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace bulk = crt_stream_bulk_close_routes;
enum class Route { NullPointer, CloseInactive, EmptySweep,
    SkippedSweep, SweepCleanup, DirectLeave };
struct BulkCase {GuestAddress entry;Route route;};
constexpr std::array Cases{
    BulkCase{0x82b85d88u,Route::NullPointer},
    BulkCase{0x82b85d88u,Route::CloseInactive},
    BulkCase{0x82df60a8u,Route::EmptySweep},
    BulkCase{0x82df60a8u,Route::SkippedSweep},
    BulkCase{0x82df61d8u,Route::SweepCleanup},
    BulkCase{0x82b7b7c8u,Route::DirectLeave}};
using NativeEvent=std::array<std::uint64_t,5>;
struct Extra final : crt_formatting_support::NativeServices,
    crt_float_environment::NativeServices,
    crt_formatter::DynamicServices,
    crt_stream_close_error::GuestServices,
    crt_free_context::LowerCalls,
    crt_stream_index_unlock::NativeServices,
    bulk::NativeServices
{
    std::vector<NativeEvent> events;
    void InitializeUnicodeString(GuestMemory&,
        crt_formatting_support::Registers&) override
    {throw std::runtime_error("unexpected unicode initialize");}
    void UnicodeStringToAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override
    {throw std::runtime_error("unexpected unicode convert");}
    void FreeAnsiString(GuestMemory&,
        crt_formatting_support::Registers&) override
    {throw std::runtime_error("unexpected ansi free");}
    void InitAnsiString(GuestMemory&,GuestAddress,GuestAddress) override
    {throw std::runtime_error("unexpected ansi descriptor");}
    std::uint64_t WriteAnsi(GuestAddress,std::uint16_t) override
    {throw std::runtime_error("unexpected ansi output");}
    void CallDebugMonitor(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override
    {throw std::runtime_error("unexpected debug monitor");}
    void CallExceptionHandler(GuestAddress,GuestMemory&,
        crt_float_environment::Registers&) override
    {throw std::runtime_error("unexpected exception handler");}
    void BugCheck(GuestMemory&,
        crt_float_environment::Registers&) override
    {throw std::runtime_error("unexpected bugcheck");}
    void CallGuestFormatter(GuestAddress,GuestMemory&,
        crt_formatter::Registers&) override
    {throw std::runtime_error("unexpected dynamic formatter");}
    void CallIndirect(GuestMemory&,GuestAddress,
        crt_stream_close_error::Registers&) override
    {throw std::runtime_error("unexpected close dynamic target");}
    void Call(GuestAddress,GuestMemory&,
        crt_free_context::Registers&) override
    {throw std::runtime_error("unexpected free direct lower");}
    void EnterCriticalSection(GuestMemory&,bulk::Registers& state) override
    {
        events.push_back({1u,state.sp,state.lr,state.r[3],state.r[31]});
        state.r[10]=0x12345678000000aau;
    }
    void LeaveCriticalSection(GuestMemory&,bulk::Registers& state) override
    {
        events.push_back({2u,state.sp,state.lr,state.r[3],state.r[31]});
        state.r[10]=0x12345678000000bbu;
    }
    void LeaveCriticalSection(GuestMemory&,
        crt_stream_index_unlock::Registers& state) override
    {
        events.push_back({3u,state.sp,state.lr,state.r3,state.r31});
        state.r10=0x12345678000000ccu;
    }
};
Extra* active_extra=nullptr;

family::Dependencies StreamDependencies(Services& s,Extra& x)
{return {s,s,s,s,{s,s,s,s,s,s,x,s},s,s};}

bulk::Dependencies Dependencies(Services& s,Extra& x)
{
    crt_wide_stream_output::Dependencies output{StreamDependencies(s,x),x};
    crt_float_formatting::Dependencies floating{{s,s},x};
    crt_formatter::Dependencies formatter{output,floating,x,s,x};
    crt_format_stream::Dependencies format{formatter};
    crt_stream_close_error::Dependencies close{StreamDependencies(s,x),x};
    return {{close,format,x},x};
}
void SeedBulk(GuestWindow& window,Route route)
{
    Seed(window,Mode::Binary);
    auto m=window.Memory();
    m.WriteU32(0x83215360u,0x62000u); // already initialized indexed lock 1
    if(route==Route::CloseInactive)
        m.WriteU32(Record+12u,0u);
    if(route==Route::SkippedSweep)
    {
        m.WriteU32(0x83378e90u,0x39000u);
        m.WriteU32(0x83378e94u,1u);
        m.WriteU32(0x39000u,0x3a000u);
        m.WriteU32(0x3a000u+12u,0u);
    }
    if(route==Route::SweepCleanup)
    {
        m.WriteU32(0x83378e90u,0x39000u);
        m.WriteU32(0x83378e94u,1u);
        m.WriteU32(0x39000u,0x3a000u);
        m.WriteU32(0x3a000u+12u,0x8000u);
        m.WriteU32(0x832153d8u,0x62100u); // indexed lock 16
    }
}
PPCContext MakeBulk(const BulkCase& item)
{
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;
    c.lr=0x1111222233334444ull;
    c.ctr.u64=0x5555666677778888ull;
    c.r13.u64=Environment;
    c.r30.u64=0xaaaa00000000001eull;
    c.r31.u64=0xbbbb00000000001full;
    c.xer.so=1;c.xer.ca=1;
    c.cr0={0,1,0,{1}};c.cr6={1,0,0,{1}};
    c.r3.u64=item.route==Route::NullPointer?0u:
        item.route==Route::CloseInactive?Record:
        item.route==Route::DirectLeave?
            0x123400000003a000ull:0x7777000000000001ull;
    if(item.route==Route::SweepCleanup)
    {
        c.r1.u64=0x1234567800000000ull|(Stack-128u);
        c.r12.u64=0x1234567800000000ull|Stack;
        c.r29.u64=0u;
        c.r30.u64=0x83378e90u;
        c.r31.u64=0x1234567800000000ull|(Stack-128u);
    }
    return c;
}
bool Check(const BulkCase& item,unsigned ordinal)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedBulk(original,item.route);SeedBulk(recovered,item.route);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    Extra original_extra,actual_extra;
    auto c=MakeBulk(item);auto state=FromPpc(c);
    active=&expected;active_extra=&original_extra;
    switch(item.entry)
    {
    case 0x82b85d88u:__imp__sub_82B85D88(c,original.Bytes());break;
    case 0x82df60a8u:__imp__sub_82DF60A8(c,original.Bytes());break;
    case 0x82df61d8u:__imp__sub_82DF61D8(c,original.Bytes());break;
    case 0x82b7b7c8u:__imp__sub_82B7B7C8(c,original.Bytes());break;
    default:throw std::runtime_error("unknown bulk entry");
    }
    active_extra=nullptr;active=nullptr;
    if(!bulk::Apply(item.entry,actual.memory,
            Dependencies(actual,actual_extra),state))
        throw std::runtime_error("missing bulk entry");
    const auto before=FromPpc(c);
    bool same=Same(before,state)&&original.EqualCommitted(recovered)&&
        expected.events==actual.events&&
        original_extra.events==actual_extra.events;
    if(!Same(before,state))
        std::fprintf(stderr,"case %u selected state mismatch r3=%llx/%llx sp=%llx/%llx\n",
            ordinal,static_cast<unsigned long long>(before.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(before.sp),
            static_cast<unsigned long long>(state.sp));
    if(!original.EqualCommitted(recovered))
        for(const auto region:Regions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",ordinal,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
    bool covered=item.route==Route::NullPointer?
        c.r3.u64==UINT64_MAX:
        item.route==Route::CloseInactive?
            original_extra.events.size()==2u:
        item.route==Route::EmptySweep||item.route==Route::SkippedSweep?
            expected.locks==1u&&original_extra.events.size()==1u&&
                original_extra.events.front()[0]==3u:
        item.route==Route::SweepCleanup?
            original_extra.events.size()==1u&&
                original_extra.events.front()[0]==3u&&
                expected.memory.ReadU32(0x3a000u+12u)==0u:
        original_extra.events.size()==1u;
    if(!covered) std::fprintf(stderr,"case %u missed bulk route r3=%llx native=%zu lock=%u unlock=%u\n",
        ordinal,static_cast<unsigned long long>(c.r3.u64),
        original_extra.events.size(),expected.locks,expected.unlocks);
    return same&&covered;
}
} // namespace

void OriginalSave28(PPCContext& c,std::uint8_t*)
{
    auto& m=active->memory;
    const auto s=FromPpc(c);
    for(unsigned i=28u;i<=31u;++i)
        WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),s.r[i]);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalRestore28(PPCContext& c,std::uint8_t*)
{
    auto s=FromPpc(c);auto& m=active->memory;
    for(unsigned i=28u;i<=31u;++i)
        s.r[i]=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    s.r[12]=m.ReadU32(Address(c.r1.u64-8u));s.lr=s.r[12];ToPpc(c,s);
}
void OriginalNativeEnter(PPCContext& c,std::uint8_t*)
{auto s=FromPpc(c);active_extra->EnterCriticalSection(active->memory,s);ToPpc(c,s);}
void OriginalNativeLeave(PPCContext& c,std::uint8_t*)
{auto s=FromPpc(c);active_extra->LeaveCriticalSection(active->memory,s);ToPpc(c,s);}
void OriginalAccepted(GuestAddress address,PPCContext& c,std::uint8_t*)
{
    auto s=FromPpc(c);auto deps=Dependencies(*active,*active_extra);
    bool found=false;
    if(address==0x82b85cc8u)
        found=crt_stream_close_pipeline::Apply(address,active->memory,
            deps.pipeline,s);
    else if(address==0x82b7b778u||address==0x82b7b810u||
        address==0x82b7b838u)
        found=crt_format_stream::Apply(address,active->memory,
            deps.pipeline.format,s);
    else if(address==0x82b81b28u)
    {
        crt_stream_locks::Registers x{};
        x.sp=s.sp;x.lr=s.lr;x.ctr=s.ctr;
        x.r3=s.r[3];x.r4=s.r[4];x.r5=s.r[5];x.r6=s.r[6];
        x.r7=s.r[7];x.r8=s.r[8];x.r9=s.r[9];x.r10=s.r[10];
        x.r11=s.r[11];x.r12=s.r[12];x.r13=s.r[13];
        x.r28=s.r[28];x.r29=s.r[29];x.r30=s.r[30];x.r31=s.r[31];
        x.cr0={bool(s.cr0.lt),bool(s.cr0.gt),bool(s.cr0.eq),bool(s.cr0.so)};
        x.cr6={bool(s.cr6.lt),bool(s.cr6.gt),bool(s.cr6.eq),bool(s.cr6.so)};
        x.xer_ca=s.xer_ca;x.xer_so=s.xer_so;
        found=crt_stream_locks::Apply(address,active->memory,
            deps.pipeline.close.accepted.locks,x);
        s.sp=x.sp;s.lr=x.lr;s.ctr=x.ctr;
        s.r[3]=x.r3;s.r[4]=x.r4;s.r[5]=x.r5;s.r[6]=x.r6;
        s.r[7]=x.r7;s.r[8]=x.r8;s.r[9]=x.r9;s.r[10]=x.r10;
        s.r[11]=x.r11;s.r[12]=x.r12;s.r[13]=x.r13;
        s.r[28]=x.r28;s.r[29]=x.r29;s.r[30]=x.r30;s.r[31]=x.r31;
        s.cr0={std::uint8_t(x.cr0.lt),std::uint8_t(x.cr0.gt),
            std::uint8_t(x.cr0.eq),std::uint8_t(x.cr0.so)};
        s.cr6={std::uint8_t(x.cr6.lt),std::uint8_t(x.cr6.gt),
            std::uint8_t(x.cr6.eq),std::uint8_t(x.cr6.so)};
        s.xer_ca=x.xer_ca;s.xer_so=x.xer_so;
    }
    else if(address==0x82b819c8u)
    {
        crt_stream_index_unlock::Registers x{s.sp,s.lr,s.r[3],s.r[10],
            s.r[11],s.r[12],s.r[29],s.r[31]};
        found=crt_stream_index_unlock::Apply(address,active->memory,
            deps.pipeline.close.accepted.locks.index_unlock,x);
        s.sp=x.sp;s.lr=x.lr;s.r[3]=x.r3;s.r[10]=x.r10;
        s.r[11]=x.r11;s.r[12]=x.r12;s.r[29]=x.r29;s.r[31]=x.r31;
    }
    else found=family::ApplyAcceptedCallee(address,active->memory,
        deps.pipeline.close.accepted,s);
    if(!found) throw std::runtime_error("missing accepted bulk lower");
    ToPpc(c,s);
}
int main()
{
    try
    {
        for(unsigned i=0;i<Cases.size();++i)
            if(!Check(Cases[i],i)) return 1;
        std::printf("PASS crt-stream-bulk-close-routes %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT dynamic close target, unselected heap/direct guest and native lock internals, faults/MMIO and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
