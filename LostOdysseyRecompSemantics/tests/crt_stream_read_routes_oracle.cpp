#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_read_routes.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
namespace read_family = crt_stream_read_routes;
using Full = read_family::Registers;
enum class ReadMode { MinusTwo, Invalid, Empty, Binary, TextCrLf, ModeTwo };
constexpr std::array Cases{ReadMode::MinusTwo, ReadMode::Invalid,
    ReadMode::Empty, ReadMode::Binary, ReadMode::TextCrLf,
    ReadMode::ModeTwo};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,
        &c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,
        &c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,
        &c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};
}
std::array<PPCRegister*, 32> FprFields(PPCContext& c)
{
    return {&c.f0,&c.f1,&c.f2,&c.f3,&c.f4,&c.f5,&c.f6,&c.f7,
        &c.f8,&c.f9,&c.f10,&c.f11,&c.f12,&c.f13,&c.f14,&c.f15,
        &c.f16,&c.f17,&c.f18,&c.f19,&c.f20,&c.f21,&c.f22,&c.f23,
        &c.f24,&c.f25,&c.f26,&c.f27,&c.f28,&c.f29,&c.f30,&c.f31};
}
Full FullFromPpc(PPCContext& c)
{
    Full s{}; const auto r=Fields(c), f=FprFields(c);
    for(unsigned i=0;i<32u;++i)
    { s.r[i]=r[i]->u64; s.fpr_bits[i]=f[i]->u64; }
    s.lr=c.lr;s.ctr=c.ctr.u64;s.cached_fp_control=c.fpscr.csr;
    s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
    s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return s;
}
void FullToPpc(PPCContext& c,const Full& s)
{
    const auto r=Fields(c), f=FprFields(c);
    for(unsigned i=0;i<32u;++i)
    {r[i]->u64=s.r[i];f[i]->u64=s.fpr_bits[i];}
    c.lr=s.lr;c.ctr.u64=s.ctr;c.fpscr.csr=s.cached_fp_control;
    c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
    c.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,{s.cr0.so}};
    c.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,{s.cr6.so}};
}
std::array<std::uint64_t,70> Snapshot(const Full& s)
{
    std::array<std::uint64_t,70> out{};
    for(unsigned i=0;i<32u;++i)
    {out[i]=s.r[i];out[i+32u]=s.fpr_bits[i];}
    out[64]=s.lr;out[65]=s.ctr;out[66]=s.cached_fp_control;
    out[67]=s.xer_so|(std::uint64_t(s.xer_ca)<<8u);
    const auto pack=[](crt_async_status_transfer::Condition c)
    {return std::uint64_t(c.lt)|(std::uint64_t(c.gt)<<8u)|
        (std::uint64_t(c.eq)<<16u)|(std::uint64_t(c.so)<<24u);};
    out[68]=pack(s.cr0);out[69]=pack(s.cr6);
    return out;
}
using ReadEvent=std::array<std::uint64_t,9>;
struct ReadNative final : crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    crt_free_context::LowerCalls
{
    ReadMode mode;
    std::vector<ReadEvent> events;
    explicit ReadNative(ReadMode selected):mode(selected){}
    void CallIndirect(GuestAddress target,GuestMemory& memory,
        Full& state) override
    {
        if(target!=0x2400u||state.lr!=0x82be2ed8u)
            throw std::runtime_error("unexpected async target");
        events.push_back({target,state.r[1],state.lr,state.r[3],
            state.r[4],state.r[5],state.r[7],state.r[8],state.r[9]});
        const char* payload=mode==ReadMode::TextCrLf?"A\r\nB":"ABCD";
        for(unsigned i=0;i<4u;++i)
            memory.WriteU8(Address(state.r[8]+i),
                static_cast<std::uint8_t>(payload[i]));
        memory.WriteU32(Address(state.r[1]+84u),4u);
        state.r[3]=0u;
        state.r[10]=0x1122334455667788ull;
        state.fpr_bits[3]=0x4008000000000000ull;
    }
    void NtWaitForSingleObjectEx(GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected async wait");}
    void NtStatusToDosError(GuestMemory&,Full&) override
    {throw std::runtime_error("unexpected status conversion");}
    void RtlMultiByteToUnicodeN(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    {throw std::runtime_error("unexpected non-UTF8 conversion");}
    void RtlNtStatusToDosError(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    {throw std::runtime_error("unexpected UTF8 status conversion");}
    void CallDirect(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected heap guest path");}
    void CallNative(GuestAddress,GuestMemory&,
        heap_allocation_context::Registers&) override
    {throw std::runtime_error("unselected heap import");}
    void Call(GuestAddress,GuestMemory&,
        crt_free_context::Registers&) override
    {throw std::runtime_error("unselected free lower");}
};
ReadNative* active_read=nullptr;

void SeedRead(GuestWindow& window,ReadMode mode)
{
    Seed(window,mode==ReadMode::TextCrLf?Mode::Text:Mode::Binary);
    auto memory=window.Memory();
    memory.WriteU32(0x34010u,0x2401u);
    if(mode==ReadMode::ModeTwo) memory.WriteU8(Record+40u,4u);
}
PPCContext MakeRead(ReadMode mode)
{
    PPCContext c{};
    const auto r=Fields(c),f=FprFields(c);
    for(unsigned i=0;i<32u;++i)
    {r[i]->u64=0x1234000000000000ull+i*0x100000001ull;
     f[i]->u64=0x4000000000000000ull+i;}
    c.r1.u64=0x1234567800000000ull|Stack;
    c.r3.u64=mode==ReadMode::MinusTwo?UINT64_MAX-1u:
        mode==ReadMode::Invalid?0x1234567800000040ull:
        0x1234567800000005ull;
    c.r4.u64=0x4567000000000000ull|0x40000u;
    c.r5.u64=mode==ReadMode::Empty?0u:
        mode==ReadMode::ModeTwo?6u:4u;
    c.r13.u64=Environment;
    c.lr=0x1111222233334444ull;c.ctr.u64=0x5555666677778888ull;
    c.fpscr.csr=0x9fc0u;c.xer.so=1;c.xer.ca=1;
    c.cr0={0,1,0,{1}};c.cr6={1,0,0,{1}};
    return c;
}
bool Check(ReadMode mode,unsigned ordinal)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedRead(original,mode);SeedRead(recovered,mode);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    ReadNative native_expected(mode),native_actual(mode);
    auto context=MakeRead(mode);auto state=FullFromPpc(context);
    active=&expected;active_read=&native_expected;
    __imp__sub_82B85448(context,original.Bytes());
    active_read=nullptr;active=nullptr;
    read_family::Dependencies deps{Dependencies(actual),native_actual,
        native_actual,native_actual,native_actual};
    if(!read_family::Apply(0x82b85448u,actual.memory,deps,state))
        throw std::runtime_error("missing read entry");
    const auto before=Snapshot(FullFromPpc(context));
    const auto after=Snapshot(state);
    bool same=before==after && original.EqualCommitted(recovered) &&
        expected.events==actual.events &&
        native_expected.events==native_actual.events;
    for(unsigned i=0;i<before.size();++i)
        if(before[i]!=after[i])
            std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",ordinal,i,
                static_cast<unsigned long long>(before[i]),
                static_cast<unsigned long long>(after[i]));
    if(!original.EqualCommitted(recovered))
        for(const auto region:Regions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",ordinal,
                    static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
    const bool covered=native_expected.events.size()==
        (mode==ReadMode::Binary||mode==ReadMode::TextCrLf||
        mode==ReadMode::ModeTwo?1u:0u) &&
        ((mode!=ReadMode::MinusTwo&&mode!=ReadMode::Invalid)||
         context.r3.u64==UINT64_MAX) &&
        (mode!=ReadMode::Empty||context.r3.u64==0u) &&
        (mode!=ReadMode::Binary||context.r3.u64==4u) &&
        (mode!=ReadMode::TextCrLf||
         (context.r3.u64==3u&&
          expected.memory.ReadU8(0x40000u)=='A'&&
          expected.memory.ReadU8(0x40001u)=='\n'&&
          expected.memory.ReadU8(0x40002u)=='B')) &&
        (mode!=ReadMode::ModeTwo||
         (context.r3.u64==4u&&
          Address(native_expected.events[0][7])==0x40000u&&
          native_expected.events[0][8]==6u&&
          native_expected.events[0][6]==
              native_expected.events[0][1]+80u));
    if(!covered) std::fprintf(stderr,
        "case %u missed read path r3=%llx callbacks=%zu cb_r7=%llx cb_r8=%llx cb_r9=%llx flags=%02x mode=%02x\n",
        ordinal,static_cast<unsigned long long>(context.r3.u64),
        native_expected.events.size(),
        static_cast<unsigned long long>(native_expected.events.empty()?0u:
            native_expected.events[0][6]),
        static_cast<unsigned long long>(native_expected.events.empty()?0u:
            native_expected.events[0][7]),
        static_cast<unsigned long long>(native_expected.events.empty()?0u:
            native_expected.events[0][8]),
        expected.memory.ReadU8(Record+4u),
        expected.memory.ReadU8(Record+40u));
    return same&&covered;
}
} // namespace

void OriginalSave18(PPCContext& c,std::uint8_t*)
{
    const auto r=Fields(c);
    for(unsigned i=18u;i<=31u;++i)
        WriteU64(active->memory,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalRestore18(PPCContext& c,std::uint8_t*)
{
    const auto r=Fields(c);
    for(unsigned i=18u;i<=31u;++i)
        r[i]->u64=ReadU64(active->memory,
            Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=active->memory.ReadU32(Address(c.r1.u64-8u));
    c.lr=c.r12.u64;
}
void OriginalSave28(PPCContext& c,std::uint8_t*)
{
    const auto r=Fields(c);
    for(unsigned i=28u;i<=31u;++i)
        WriteU64(active->memory,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalRestore28(PPCContext& c,std::uint8_t*)
{
    const auto r=Fields(c);
    for(unsigned i=28u;i<=31u;++i)
        r[i]->u64=ReadU64(active->memory,
            Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=active->memory.ReadU32(Address(c.r1.u64-8u));
    c.lr=c.r12.u64;
}
void OriginalStream(GuestAddress entry,PPCContext& c,std::uint8_t*)
{
    auto s=FromPpc(c);
    const bool found=entry==0x82b85ea8u?
        family::Apply(entry,active->memory,Dependencies(*active),s):
        family::ApplyAcceptedCallee(entry,active->memory,
            Dependencies(*active),s);
    if(!found) throw std::runtime_error("missing accepted read lower");
    ToPpc(c,s);
}
void OriginalIndirect(std::uint32_t target,PPCContext& c,std::uint8_t*)
{auto s=FullFromPpc(c);active_read->CallIndirect(target,active->memory,s);
 FullToPpc(c,s);}
void OriginalWait(PPCContext& c,std::uint8_t*)
{auto s=FullFromPpc(c);active_read->NtWaitForSingleObjectEx(active->memory,s);
 FullToPpc(c,s);}
void OriginalStatus(PPCContext& c,std::uint8_t*)
{auto s=FullFromPpc(c);active_read->NtStatusToDosError(active->memory,s);
 FullToPpc(c,s);}
void OriginalUnselected(PPCContext&,std::uint8_t*)
{throw std::runtime_error("unselected read allocation/free/UTF8 branch");}

int main()
{
    try
    {
        for(unsigned i=0;i<Cases.size();++i)
            if(!Check(Cases[i],i)) return 1;
        std::printf("PASS crt-stream-read-routes %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT allocation/free/UTF8 branch execution, heap guest helpers, async indirect target and native import internals remain open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
