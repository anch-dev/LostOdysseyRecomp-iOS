#include "lo_semantics/legacy_descriptor_scalar_intern_callers.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace {
using namespace lo::semantic::gpu;
namespace family=legacy_descriptor_scalar_intern_callers;
constexpr GuestAddress Stack=0x80000u,Owner=0x10000u,Previous=0x20000u;
constexpr GuestAddress Output=0x30000u,Pool=0x60000u,Sink=0x70000u;
constexpr GuestAddress SelectorTable=0x821712a0u;
constexpr std::array<test::Region,4> Regions{{{0u,0x90000u},
 {0x82000000u,0x1000u},{0x82171000u,0x1000u},{0x83245000u,0x2000u}}};
struct Case {const char* name;GuestAddress entry;unsigned mode,count;};
constexpr std::array Cases{
 Case{"word-kind1",0x8305a6b0u,1u,1u},
 Case{"word-kind2",0x8305a6b0u,0u,1u},
 Case{"output-record-preserve-bit16",0x8305a720u,1u,1u},
 Case{"output-record-clear-bit16",0x8305a720u,0u,1u},
 Case{"vector-two",0x8305a778u,0u,2u},
 Case{"vector-four",0x8305a778u,0u,4u},
 Case{"typed-float",0x8305a7c0u,0u,1u},
 Case{"typed-signed",0x8305a7c0u,1u,1u},
 Case{"typed-wide",0x8305a7c0u,2u,1u},
 Case{"typed-diagnostic-mutates-F1",0x8305a7c0u,3u,1u}
};
unsigned lookup_calls=0,constructor_calls=0,diagnostic_calls=0;
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}

family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    auto& g = state.integer;
    for (unsigned index = 0; index < 32u; ++index)
        g.r[index] = index == 1u ? 0u : fields[index]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.f0_bits = c.f0.u64; state.f12_bits = c.f12.u64;
    state.f13_bits = c.f13.u64;
    state.f1_bits=c.f1.u64;state.f2_bits=c.f2.u64;state.f3_bits=c.f3.u64;state.f4_bits=c.f4.u64;
    state.cached_fp_control = c.fpscr.csr;
    return state;
}

bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }

bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer;
    const auto& y = b.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.f0_bits == b.f0_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control && a.f1_bits==b.f1_bits && a.f2_bits==b.f2_bits && a.f3_bits==b.f3_bits && a.f4_bits==b.f4_bits;
}


void ToPpc(PPCContext& c,const family::Registers& s) {
 auto fields=Fields(c);for(unsigned i=0;i<32u;++i)fields[i]->u64=i==1u?s.integer.sp:s.integer.r[i];
 c.lr=s.integer.lr;c.ctr.u64=s.integer.ctr;c.xer.so=s.integer.xer_so;c.xer.ca=s.integer.xer_ca;
 c.cr0={s.integer.cr0.lt,s.integer.cr0.gt,s.integer.cr0.eq,s.integer.cr0.so};
 c.cr6={s.integer.cr6.lt,s.integer.cr6.gt,s.integer.cr6.eq,s.integer.cr6.so};
 c.f0.u64=s.f0_bits;c.f1.u64=s.f1_bits;c.f2.u64=s.f2_bits;c.f3.u64=s.f3_bits;c.f4.u64=s.f4_bits;
 c.f12.u64=s.f12_bits;c.f13.u64=s.f13_bits;c.fpscr.csr=s.cached_fp_control;
}
PPCContext Initial(const Case& item) {
 PPCContext c{};auto fields=Fields(c);for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800000000ull|Stack;c.r3.u64=0xabcdef0100000000ull|Owner;
 c.r4.u64=0x40600000u;c.r5.u64=item.mode;
 if(item.entry==0x8305a720u){c.r3.u64=Output;c.r4.u64=Owner;c.r5.u64=0x40600000u;c.r6.u64=item.mode;}
 if(item.entry==0x8305a778u)c.r4.u64=item.count;
 c.f0.u64=std::bit_cast<std::uint64_t>(9.0);
 c.f1.u64=std::bit_cast<std::uint64_t>(item.mode==0u?3.5:5.0);
 c.f2.u64=std::bit_cast<std::uint64_t>(1.0);c.f3.u64=std::bit_cast<std::uint64_t>(-2.0);
 c.f4.u64=std::bit_cast<std::uint64_t>(4.0);c.f12.u64=std::bit_cast<std::uint64_t>(7.0);
 c.f13.u64=std::bit_cast<std::uint64_t>(8.0);c.lr=0xabcdef0123456789ull;c.ctr.u64=0x5555666677778888ull;
 c.cr0.gt=1;c.cr6.lt=1;c.xer.so=1;c.xer.ca=1;c.fpscr.csr=0x9fc0u;return c;
}
void Seed(GuestMemory& m,const Case& item) {
 for(unsigned i=0;i<SelectorBytes.size();++i)m.WriteU8(SelectorTable+i,SelectorBytes[i]);
 m.WriteU32(0x82000e50u,0u);m.WriteU32(Owner+4u,Owner);m.WriteU32(Owner+16u,Previous);
 m.WriteU32(Previous,0u);m.WriteU32(16u,Previous);m.WriteU32(Owner+40u,0u);m.WriteU32(Owner+88u,0u);
 m.WriteU32(Owner+912u,Pool+4096u);m.WriteU32(Owner+916u,Pool);
 for(unsigned slot=15u;slot<22u;++slot)m.WriteU32(Owner+slot*4u,0u);
 m.WriteU32(Output,0u);m.WriteU32(Output+4u,item.mode==1u?0xffffffffu:0xfffeffffu);
}
void Diagnostic(GuestMemory& m,family::Registers& s) {
 m.WriteU32(Sink,static_cast<GuestAddress>(s.integer.r[4]));
 s.f1_bits=std::bit_cast<std::uint64_t>(7.0);s.integer.r[9]=0x77889900u;
 s.integer.cr0={0u,1u,0u,s.integer.xer_so};s.integer.xer_ca=0u;
}
struct Services final:legacy_descriptor_array_allocation::Services,
 legacy_descriptor_array_lookup::FreeTailServices,family::DiagnosticServices {
 unsigned diagnostics=0;
 void AllocateFromPool(GuestMemory&,crt_stream_operations::Registers&)override{throw std::runtime_error("pool exhausted");}
 void SetHostFpControl(std::uint32_t csr)override{simde_mm_setcsr(csr);}
 void Call(GuestAddress,GuestMemory&,crt_stream_operations::Registers&)override{throw std::runtime_error("unexpected free");}
 void Call(GuestMemory& m,family::Registers& s)override{++diagnostics;Diagnostic(m,s);}
};
void Check(const Case& item) {
 test::GuestWindow original(Regions),recovered(Regions);auto left=original.Memory(),right=recovered.Memory();
 Seed(left,item);Seed(right,item);auto context=Initial(item);auto state=FromPpc(context);Services services;
 lookup_calls=constructor_calls=diagnostic_calls=0;const auto previous=simde_mm_getcsr();simde_mm_setcsr(0x9fc0u);
 std::fprintf(stderr,"intern %s original\n",item.name);std::fflush(stderr);
 switch(item.entry) {
case 0x8305a6b0u:__imp__sub_8305A6B0(context,original.Bytes());break;
case 0x8305a720u:__imp__sub_8305A720(context,original.Bytes());break;
case 0x8305a778u:__imp__sub_8305A778(context,original.Bytes());break;
case 0x8305a7c0u:__imp__sub_8305A7C0(context,original.Bytes());break;
default:throw std::runtime_error("entry missing");}
 auto observed=FromPpc(context);const auto original_csr=simde_mm_getcsr();simde_mm_setcsr(0x9fc0u);
 std::fprintf(stderr,"intern %s recovered\n",item.name);std::fflush(stderr);
 if(!family::Apply(item.entry,right,{{services,services},services},state))throw std::runtime_error("implementation missing");
 const auto recovered_csr=simde_mm_getcsr();simde_mm_setcsr(previous);
 if(lookup_calls!=1u||constructor_calls!=1u||diagnostic_calls!=(item.mode==3u?1u:0u)||services.diagnostics!=diagnostic_calls)
  throw std::runtime_error("original caller path");
 if(item.entry==0x8305a720u && (observed.integer.r[3]!=Output||left.ReadU32(Output)!=Pool||left.ReadU32(Output+4u)!=(1u|(item.mode==1u?0x10000u:0u))))
  throw std::runtime_error("output-record independent assertion");
 if(!Same(observed,state)||original_csr!=recovered_csr||!original.EqualCommitted(recovered)) {
  for(unsigned i=0;i<32u;++i)if(observed.integer.r[i]!=state.integer.r[i])std::fprintf(stderr,"r%u %016llX/%016llX\n",i,(unsigned long long)observed.integer.r[i],(unsigned long long)state.integer.r[i]);
  std::fprintf(stderr,"host %08X/%08X FP0 %llX/%llX\n",original_csr,recovered_csr,(unsigned long long)observed.f0_bits,(unsigned long long)state.f0_bits);
  for(auto region:Regions)for(std::size_t off=0;off<region.size;++off){auto a=std::size_t(region.base)+off;if(original.Bytes()[a]!=recovered.Bytes()[a]){
   std::fprintf(stderr,"RAM %08zX %02X/%02X\n",a,original.Bytes()[a],recovered.Bytes()[a]);throw std::runtime_error(item.name);}}
  throw std::runtime_error(item.name);
 }
}
}
void OriginalBody8305A6B0(PPCContext& c,std::uint8_t* b) {__imp__sub_8305A6B0(c,b);}
void OriginalBody8305A720(PPCContext& c,std::uint8_t* b) {__imp__sub_8305A720(c,b);}
void OriginalBody8305A778(PPCContext& c,std::uint8_t* b) {__imp__sub_8305A778(c,b);}
void OriginalBody8305A7C0(PPCContext& c,std::uint8_t* b) {__imp__sub_8305A7C0(c,b);}
void OriginalBody82FAC238(PPCContext& c,std::uint8_t* b) {__imp__sub_82FAC238(c,b);}
void OriginalBody83058590(PPCContext& c,std::uint8_t* b) {++constructor_calls;__imp__sub_83058590(c,b);}
void OriginalBody82FB36B0(PPCContext& c,std::uint8_t* b) {__imp__sub_82FB36B0(c,b);}
void OriginalBody83056568(PPCContext& c,std::uint8_t* b) {__imp__sub_83056568(c,b);}
void OriginalBody82B7BC40(PPCContext& c,std::uint8_t* b) {__imp__sub_82B7BC40(c,b);}
void OriginalBody827C9DB0(PPCContext& c,std::uint8_t* b) {__imp__sub_827C9DB0(c,b);}
void OriginalBody827CA0E8(PPCContext& c,std::uint8_t* b) {__imp__sub_827CA0E8(c,b);}
void OriginalBody827CAD80(PPCContext& c,std::uint8_t* b) {__imp__sub_827CAD80(c,b);}
void OriginalBody823ACC98(PPCContext& c,std::uint8_t* b) {__imp__sub_823ACC98(c,b);}
void OriginalBody827C9EB8(PPCContext& c,std::uint8_t* b) {__imp__sub_827C9EB8(c,b);}
void OriginalBody83058AD8(PPCContext& c,std::uint8_t* b) {++lookup_calls;__imp__sub_83058AD8(c,b);}
void OriginalBody82FAC980(PPCContext& c,std::uint8_t* b) {__imp__sub_82FAC980(c,b);}
void OriginalSave24(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=24u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),f[i]->u64);m.WriteU32(c.r1.u32-8u,c.r12.u32);}
void OriginalRestore24(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=24u;i<=31u;++i)f[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalSave25(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=25u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),f[i]->u64);m.WriteU32(c.r1.u32-8u,c.r12.u32);}
void OriginalRestore25(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=25u;i<=31u;++i)f[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalSave27(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=27u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),f[i]->u64);m.WriteU32(c.r1.u32-8u,c.r12.u32);}
void OriginalRestore27(PPCContext& c,std::uint8_t* b) {auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto f=Fields(c);for(unsigned i=27u;i<=31u;++i)f[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalDiagnostic(PPCContext& c,std::uint8_t* b){++diagnostic_calls;auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto s=FromPpc(c);Diagnostic(m,s);ToPpc(c,s);}
void OriginalNative(PPCContext&,std::uint8_t*){std::fputs("unexpected original native\n",stderr);std::abort();}
int main(){try{for(const auto& item:Cases)Check(item);std::printf("PASS legacy-descriptor-scalar-intern-callers %zu actual PPC cases\n",Cases.size());std::puts("LIMIT selected scalar/vector/layout cases; diagnostic implementation, other context, exceptional conversions, faults/MMIO/concurrency/runtime open");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
