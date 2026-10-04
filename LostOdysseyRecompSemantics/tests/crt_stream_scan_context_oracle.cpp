#pragma push_macro("main")
#undef main
#define main ScanSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_stream_scan_context.h"
#include "lo_semantics/crt_stream_byte_read_context.h"
#include "lo_semantics/crt_stream_pushback_context.h"

namespace scan_oracle
{
namespace family = crt_stream_scan_context;
using Full = family::Registers;
constexpr GuestAddress Format = 0x41000u;
constexpr GuestAddress Arguments = 0x42000u;
constexpr GuestAddress Destination = 0x43000u;
constexpr GuestAddress ScanObject = 0x83214fc8u;
constexpr GuestAddress DispatchTable = 0x82157d58u;
constexpr GuestAddress IndirectTarget = 0x2400u;
constexpr std::array<test::Region, 8> ScanRegions{{
    {0u, 0x120000u}, {0x82157000u, 0x1000u},
    {0x831e0000u, 0x10000u}, {0x83214000u, 0x3000u},
    {0x83245000u, 0x1000u}, {0x832d3000u, 0x2000u},
    {0x832ec000u, 0x2000u}, {0x83378000u, 0x3000u}}};
// VA 82157D58, read directly from image_disc1.bin at offset 157D58.
constexpr std::array<std::uint8_t, 50> DispatchBytes{{
    0,0,4,0x9c,7,0xdc,7,0xdc,7,0xdc,0x0b,0xb4,3,0x3c,0x0b,0xb4,
    0x0b,0xb4,0x0b,0xb4,0x0b,0xb4,7,0x88,4,0x9c,4,0x98,0x0b,0xb4,
    0x0b,0xb4,0,0x10,0x0b,0xb4,4,0x9c,0x0b,0xb4,0x0b,0xb4,3,0x40,
    0x0b,0xb4,0x0b,0xb4,0,0x20}};

enum class Route {NullFormat, Literal, LiteralMiss, RefillLiteral,
    FloatIndirect};
struct ScanCase {const char* name; Route route;};
constexpr std::array Cases{
    ScanCase{"null-format", Route::NullFormat},
    ScanCase{"buffered-literal-match", Route::Literal},
    ScanCase{"buffered-literal-miss", Route::LiteralMiss},
    ScanCase{"refill-literal-match", Route::RefillLiteral},
    ScanCase{"float-indirect", Route::FloatIndirect}};
using Event = std::array<std::uint64_t, 9>;

struct ReadNative final : crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    crt_free_context::LowerCalls
{
    std::vector<Event> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory,
        Full& state) override
    {
        if (target != IndirectTarget || state.lr != 0x82be2ed8u)
            throw std::runtime_error("unexpected scan refill native target");
        events.push_back({target,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.r[8],state.r[9],state.ctr});
        for (unsigned i=0; i<4u; ++i)
            memory.WriteU8(Address(state.r[8]+i),
                static_cast<std::uint8_t>("ABCD"[i]));
        memory.WriteU32(Address(state.r[1]+84u),4u);
        state.r[3]=0u;
        state.r[10]=0x1122334455667788ull;
        state.fpr_bits[3]=0x4008000000000000ull;
        state.cr1.gt^=1u;
        state.cr7.eq^=1u;
    }
    void NtWaitForSingleObjectEx(GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected scan async wait");}
    void NtStatusToDosError(GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected scan status conversion");}
    void RtlMultiByteToUnicodeN(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    {throw std::runtime_error("unexpected scan conversion");}
    void RtlNtStatusToDosError(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    {throw std::runtime_error("unexpected scan conversion status");}
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unexpected scan heap guest call");}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unexpected scan heap native call");}
    void Call(GuestAddress,GuestMemory&,crt_free_context::Registers&) override
    {throw std::runtime_error("unexpected scan free lower call");}
};

using GrowthEvent = std::array<std::uint64_t, 4>;
struct GrowthHeap final : heap_allocation_context::BoundaryServices
{
    std::vector<GrowthEvent> events;
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unexpected scan growth heap guest call");}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unexpected scan growth heap native call");}
};
struct GrowthHandler final : crt_record_allocation_context::HandlerServices
{
    std::vector<GrowthEvent> events;
    void CallNewHandler(GuestAddress,GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected scan growth handler");}
};
struct GrowthGuest final : crt_reallocation_context::GuestServices
{
    std::vector<GrowthEvent> events;
    void CallLower(GuestAddress,GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected scan growth realloc lower");}
};
struct GrowthQuery final : heap_block_query_context::NativeServices
{
    std::vector<GrowthEvent> events;
    void CallNative(GuestAddress,GuestMemory&,
        heap_block_query_context::Registers&) override
    {throw std::runtime_error("unexpected scan growth query native");}
};
struct Indirect final : family::GuestServices
{
    std::vector<Event> events;
    void CallIndirect(GuestAddress target,GuestMemory& memory,
        Full& state) override
    {
        if (target!=IndirectTarget || state.lr!=0x82df5b84u)
            throw std::runtime_error("unexpected scan object target");
        events.push_back({target,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.r[6],state.ctr,
            state.fpr_bits[3]});
        memory.WriteU32(Address(state.r[4]),7u);
        state.r[3]=0x8877665500000001ull;
        state.r[10]=0xaabbccdd00000042ull;
        state.fpr_bits[3]=0x4008000000000000ull;
        state.cr1.gt^=1u;
        state.cr7.eq^=1u;
    }
};

struct ServicesBundle
{
    Services stream;
    IndexService index;
    Host host{Scenario::BinarySuccess};
    wrapper_oracle::WrapperUnlock unlock;
    close_shared_oracle::SharedExtra extra;
    ReadNative native;
    GrowthHeap heap;
    GrowthHandler handler;
    GrowthGuest guest;
    GrowthQuery query;
    Indirect indirect;
    explicit ServicesBundle(GuestWindow& window):
        stream(window,Mode::LockedWrite){}
};

family::Dependencies Deps(ServicesBundle& bundle)
{
    auto shared=close_shared_oracle::Deps(bundle.stream,bundle.index,
        bundle.host,bundle.unlock,bundle.extra);
    const auto& open=shared.open.open;
    crt_stream_open_pipeline::Dependencies pipeline{
        open.stream,open.open,open.position,
        {open.read.stream,bundle.native,bundle.native,bundle.native,bundle.native},
        open.close,open.failure};
    crt_stream_close_shared_lower::Dependencies accepted{
        shared.close,{pipeline,shared.open.unlock},shared.native};
    crt_record_allocation_context::Dependencies allocation{
        Dependencies(bundle.stream),bundle.heap,bundle.handler};
    crt_reallocation_context::Dependencies reallocation{
        allocation,bundle.guest};
    crt_stream_resize_context::Dependencies resize{reallocation,bundle.query};
    return {accepted,{allocation,resize},bundle.indirect};
}

void Seed(GuestWindow& window,Route route)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    for(std::size_t i=0;i<DispatchBytes.size();++i)
        memory.WriteU8(DispatchTable+static_cast<GuestAddress>(i),
            DispatchBytes[i]);
    memory.WriteU32(ScanObject+28u,IndirectTarget|1u);
    memory.WriteU32(Arguments+4u,Destination);
    memory.WriteU32(Stream+12u,9u);
    memory.WriteU32(Stream+8u,Buffer);
    memory.WriteU32(Stream+16u,5u);
    memory.WriteU32(Stream+24u,4u);
    memory.WriteU32(Stream,Buffer);
    memory.WriteU32(Stream+4u,route==Route::RefillLiteral?0u:2u);
    memory.WriteU32(0x34010u,0x2401u);
    const char* format=route==Route::FloatIndirect?"%f":"A";
    for(unsigned i=0;format[i]!='\0';++i)
        memory.WriteU8(Format+i,static_cast<std::uint8_t>(format[i]));
    memory.WriteU8(Format+(route==Route::FloatIndirect?2u:1u),0u);
    memory.WriteU8(Buffer,route==Route::LiteralMiss?'B':
        route==Route::FloatIndirect?'7':'A');
    memory.WriteU8(Buffer+1u,route==Route::FloatIndirect?'X':'B');
    if(route==Route::FloatIndirect)
    {
        // Actual 823588A0 tests the 0x4 digit bit in this runtime ctype
        // table. The default zero page would classify '7' as nonnumeric.
        memory.WriteU32(0x832152e8u,0x71000u);
        memory.WriteU16(0x71000u+2u*static_cast<GuestAddress>('7'),4u);
        // The actual 57B0 floating branch reads the locale decimal byte
        // through the pointer chain rooted at 83215300.
        memory.WriteU32(0x83215300u,0x70000u);
        memory.WriteU32(0x70000u+188u,0x70100u);
        memory.WriteU32(0x70100u,0x70200u);
        memory.WriteU8(0x70200u,'.');
    }
}

PPCContext Initial(Route route)
{
    auto context=close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    context.r1.u64=0x8877665500000000ull|Stack;
    context.lr=0x1234567887654321ull;
    context.ctr.u64=0x5566778899aabbccull;
    context.f3.u64=0x3ff0000000000000ull;
    context.cr1.gt=1u;context.cr7.lt=1u;
    context.r3.u64=0x8877665500000000ull|Stream;
    context.r4.u64=route==Route::NullFormat?0u:
        0x7766554400000000ull|Format;
    context.r5.u64=0x6655443300000000ull|Arguments;
    context.r6.u64=0x5544332200000000ull|Arguments;
    return context;
}

ServicesBundle* original_services=nullptr;

void Check(const ScanCase& item)
{
    GuestWindow original(ScanRegions),recovered(ScanRegions);
    Seed(original,item.route);Seed(recovered,item.route);
    ServicesBundle expected(original),actual(recovered);
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected.stream;active=&expected.stream;
    current_index=&expected.index;current_host=&expected.host;
    wrapper_oracle::original_unlock=&expected.unlock;
    close_shared_oracle::active_extra=&expected.extra;
    original_services=&expected;
    __imp__sub_82DF4AF8(context,original.Bytes());
    original_services=nullptr;
    close_shared_oracle::active_extra=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    current_host=nullptr;current_index=nullptr;active=nullptr;current=nullptr;
    if(!family::Apply(0x82df4af8u,actual.stream.memory,Deps(actual),state))
        throw std::runtime_error("missing recovered scan body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.stream.events!=actual.stream.events||
        expected.stream.traps!=actual.stream.traps||
        expected.index.events!=actual.index.events||
        expected.host.events!=actual.host.events||
        expected.unlock.events!=actual.unlock.events||
        expected.extra.events!=actual.extra.events||
        expected.native.events!=actual.native.events||
        expected.indirect.events!=actual.indirect.events)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"%s state[%u] %llx/%llx\n",item.name,
                    i,static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:ScanRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"%s RAM %08llx %02x/%02x\n",
                    item.name,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("scan Full72/RAM/callback mismatch");
    }
    const auto memory=expected.stream.memory;
    if(item.route==Route::NullFormat)
    {
        if(context.r3.s32!=-1||memory.ReadU32(0x83215210u)!=22u)
            throw std::runtime_error("null scan format not rejected");
    }
    else if(item.route==Route::Literal)
    {
        if(context.r3.u32!=0u||memory.ReadU32(Stream)!=Buffer+1u||
            memory.ReadU32(Stream+4u)!=1u)
            throw std::runtime_error("buffered literal not consumed");
    }
    else if(item.route==Route::RefillLiteral)
    {
        if(context.r3.u32!=0u||expected.native.events.size()!=1u||
            memory.ReadU32(Stream)!=Buffer+1u||
            memory.ReadU32(Stream+4u)!=3u)
            throw std::runtime_error("literal did not use byte-read refill");
    }
    else if(item.route==Route::FloatIndirect)
    {
        if(context.r3.u32!=1u||expected.indirect.events.size()!=1u||
            expected.indirect.events[0][2]!=0x82df5b84u||
            memory.ReadU32(Destination)!=7u)
        {
            std::fprintf(stderr,"%s original r3=%08x indirect=%zu "
                "dest=%08x stream=%08x count=%08x\n",item.name,
                context.r3.u32,expected.indirect.events.size(),
                memory.ReadU32(Destination),memory.ReadU32(Stream),
                memory.ReadU32(Stream+4u));
            throw std::runtime_error("floating scan did not call object+28");
        }
    }
    else if(context.r3.u32!=0u||expected.indirect.events.size()!=0u)
        throw std::runtime_error("literal mismatch path absent");
}
} // namespace scan_oracle

void OriginalScanAccepted(GuestAddress entry,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    auto dependencies=scan_oracle::Deps(*scan_oracle::original_services);
    bool handled=false;
    switch(entry)
    {
    case 0x82b81360u:
        handled=crt_stream_byte_read_context::Apply(entry,current->memory,
            dependencies.accepted,state);break;
    case 0x82b87cf8u:
        handled=crt_stream_pushback_context::Apply(entry,current->memory,
            {dependencies.growth.resize.reallocation},state);break;
    case 0x82df4a58u:
        handled=crt_stream_buffer_growth_callers::Apply(entry,current->memory,
            dependencies.growth,state);break;
    case 0x82b7fd78u:case 0x82b7fec0u:case 0x823addc0u:
        handled=crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
            current->memory,dependencies.accepted,state);break;
    default:throw std::runtime_error("unselected original scan callee");
    }
    if(!handled)throw std::runtime_error("missing original scan callee");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalScanIndirect(GuestAddress target,PPCContext& context,
    std::uint8_t*)
{
    auto state=crt_full_oracle::FromPpc(context);
    scan_oracle::original_services->indirect.CallIndirect(target,
        current->memory,state);
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for(const auto& item:scan_oracle::Cases)
    {
        try{scan_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("PASS crt-stream-scan-context 5 focused actual PPC cases");
    std::puts("LIMIT selected accepted lower ABI and mutable object+28 guest boundary; fault/MMIO/concurrency/runtime unvalidated");
    return 0;
}
