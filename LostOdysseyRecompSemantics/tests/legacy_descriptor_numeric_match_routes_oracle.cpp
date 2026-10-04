#include "lo_semantics/legacy_descriptor_numeric_match_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_numeric_match_routes;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x8000u},
    {0x8210a000u, 0x2000u}
}};
constexpr GuestAddress Record = 0x10020u, Owner = 0x30000u, Output = 0x40000u, Sink = 0x50000u;
struct Case {const char* name;bool getter;unsigned kind,flags,index;std::uint32_t word;bool force_float;};
constexpr std::array Cases{
 Case{"extract-float-clamp",true,0u,1u,0u,0x40600000u,false},
 Case{"extract-float-flip",true,0u,4u,0u,0x40600000u,false},
 Case{"extract-signed-copy",true,1u,2u,1u,0xfffffffdu,false},
 Case{"extract-unsigned-copy",true,2u,2u,0u,7u,false},
 Case{"extract-force-float",true,2u,0u,0u,0x40600000u,true}
};
unsigned diagnostic_calls=0,numeric_calls=0,copy_calls=0,flip_calls=0,category_calls=0,special_calls=0;
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
    for (unsigned index = 0; index < 32u; ++index)
        state.numeric.classifier.integer.r[index] =
            index == 1u ? 0u : fields[index]->u64;
    auto& integer = state.numeric.classifier.integer;
    integer.sp = c.r1.u64; integer.lr = c.lr; integer.ctr = c.ctr.u64;
    integer.xer_so = c.xer.so; integer.xer_ca = c.xer.ca;
    integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.numeric.classifier.f0_bits = c.f0.u64;
    state.numeric.classifier.f1_bits = c.f1.u64;
    state.numeric.classifier.cached_fp_control = c.fpscr.csr;
    state.numeric.f31_bits = c.f31.u64;
    state.f2_bits = c.f2.u64;
    return state;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{
    return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so;
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.numeric.classifier.integer;
    const auto& y = b.numeric.classifier.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.numeric.classifier.f0_bits == b.numeric.classifier.f0_bits &&
        a.numeric.classifier.f1_bits == b.numeric.classifier.f1_bits &&
        a.numeric.classifier.cached_fp_control ==
            b.numeric.classifier.cached_fp_control &&
        a.numeric.f31_bits == b.numeric.f31_bits &&
        a.f2_bits == b.f2_bits;
}
PPCContext Initial(const Case& item, GuestAddress)
{
    PPCContext c{}; const auto fields=Fields(c);
    for(unsigned i=0; i<32u; ++i) fields[i]->u64=0x1122334400000000ull+i;
    c.r1.u64=0x1234567800000000ull|Stack;
    c.r3.u64=0x1234567800000000ull|(item.getter ? Record : Output);
    c.r4.u64=0x5566778800000000ull|(item.getter ? item.index : Record);
    c.r5.u64=0x99aabbcc00000000ull|item.flags;
    c.r6.u64=item.flags;
    c.lr=0xabcdef0123456789ull;c.ctr.u64=0x5555666677778888ull;
    c.cr0.gt=1;c.cr6.lt=1;c.xer.so=1;c.xer.ca=1;
    c.f0.u64=0xc00a000000000000ull;c.f1.u64=0x4020000000000000ull;
    c.f2.u64=0x8000000000000000ull;c.f31.u64=0x400a000000000000ull;
    c.fpscr.csr=0x9fc0u;return c;
}
void Seed(GuestMemory& m,const Case& item)
{
    // Exact image_disc1.bin data, each file offset is VA minus 0x82000000.
    m.WriteU32(0x82000e50u,0u);m.WriteU32(0x82007784u,0x3f800000u);
    WriteU64(m,0x82000f28u,0x3ff0000000000000ull);
    WriteU64(m,0x82000fe8u,0u);WriteU64(m,0x8210aac8u,0x41f0000000000000ull);
    m.WriteU32(0x10000u,0x20000u);m.WriteU32(0x20000u+148u,Owner);
    m.WriteU32(Owner+40u,item.force_float ? 0x4000u : 0u);
    m.WriteU32(Record+16u,item.kind << (14u+2u*item.index));
    m.WriteU32(Record+40u+4u*item.index,item.word);
}
void DiagnosticAction(GuestMemory& m,family::Registers& s,unsigned ordinal)
{
    auto& g=s.numeric.classifier.integer;
    m.WriteU32(Sink+ordinal*8u,static_cast<std::uint32_t>(g.r[3]));
    m.WriteU32(Sink+ordinal*8u+4u,static_cast<std::uint32_t>(g.r[4]));
    g.r[4]=0;g.r[11]=Record;g.r[9]=0xaabbccddu;
    g.cr0={0,1,0,g.xer_so};g.xer_ca=0;
}
struct Services final : family::Services
{
    unsigned calls=0,diagnostics=0;
    void SetHostFpControl(std::uint32_t control) override
    {++calls;PPCFPSCRRegister{}.setcsr(control);}
    void Diagnostic(GuestMemory& m,family::Registers& s) override
    {DiagnosticAction(m,s,diagnostics++);}
};
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};

void Check(const Case& item){
 test::GuestWindow original(Regions),recovered(Regions);auto left=original.Memory(),right=recovered.Memory();
 Seed(left,item);Seed(right,item);auto context=Initial(item,0u);auto state=FromPpc(context);Services services;
 diagnostic_calls=numeric_calls=copy_calls=flip_calls=category_calls=special_calls=0;
 const auto saved=simde_mm_getcsr();simde_mm_setcsr(0x9fc0u);
 __imp__sub_83053EA0(context,original.Bytes());const auto observed=FromPpc(context);const auto original_csr=simde_mm_getcsr();
 simde_mm_setcsr(0x9fc0u);if(!family::Apply(0x83053ea0u,right,services,state))throw std::runtime_error("extract missing");
 const auto recovered_csr=simde_mm_getcsr();simde_mm_setcsr(saved);
 if(numeric_calls!=(item.flags==1u?1u:0u)||copy_calls!=(item.flags==2u?1u:0u)||flip_calls!=(item.flags==4u?1u:0u)||diagnostic_calls!=0u)
  throw std::runtime_error("original extraction path");
 if(!Same(observed,state)||original_csr!=recovered_csr||!original.EqualCommitted(recovered))throw std::runtime_error(item.name);
}
void CheckMatch(unsigned mode){
 test::GuestWindow original(Regions),recovered(Regions);auto left=original.Memory(),right=recovered.Memory();
 constexpr GuestAddress A=0x60000u,B=0x61000u,P=0x62000u,Q=0x63000u;
 const bool same_parent=mode==0u||mode>=3u;
 const std::uint32_t header=0x02000000u|(1u<<25u)|(3u<<5u)|1u;
 for(auto* m:{&left,&right}){
  m->WriteU32(A+12u,P);m->WriteU32(B+12u,same_parent?P:Q);
  m->WriteU32(P+8u,mode==2u?0u:15232u);m->WriteU32(Q+8u,15232u);
  m->WriteU32(A,header);m->WriteU32(B,header^(mode==3u?1u:mode==4u?0x02000000u:0u));
 }
 auto context=Initial(Cases[0],0u);context.r3.u64=A;context.r4.u64=B;auto state=FromPpc(context);Services services;
 __imp__sub_82FAC128(context,original.Bytes());auto observed=FromPpc(context);
 if(!family::Apply(0x82fac128u,right,services,state))throw std::runtime_error("match missing");
 const auto expected=mode==0u||mode==1u?1u:0u;
 if(observed.numeric.classifier.integer.r[3]!=expected)throw std::runtime_error("match expected");
 if(!Same(observed,state)||!original.EqualCommitted(recovered))throw std::runtime_error("match state/RAM");
}
}
void OriginalNumeric(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053c30u && c.lr != 0x83053ca4u)
        throw std::runtime_error("actual 83053360 call LR");
    __imp__sub_83053360(c, base);
    ++numeric_calls;
}
void OriginalCopySign(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053c44u && c.lr != 0x83053cb8u)
        throw std::runtime_error("actual 82B7DEF0 call LR");
    __imp__sub_82B7DEF0(c, base);
    ++copy_calls;
}
void OriginalFlipSign(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053cdcu)
        throw std::runtime_error("actual 82B7DF28 call LR");
    __imp__sub_82B7DF28(c, base);
    ++flip_calls;
}
void OriginalCategory(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053378u)
        throw std::runtime_error("actual 82B7DFC0 call LR");
    __imp__sub_82B7DFC0(c, base);
    ++category_calls;
}
void OriginalSpecial(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x82b7dfe4u)
        throw std::runtime_error("actual 82B82340 call LR");
    __imp__sub_82B82340(c, base);
    ++special_calls;
}

void OriginalSave28(PPCContext& c,std::uint8_t* base)
{
    const auto fields=Fields(c);
    for(unsigned i=28u;i<=31u;++i) PPC_STORE_U64(c.r1.u32-40u+(i-28u)*8u,fields[i]->u64);
    PPC_STORE_U32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore28(PPCContext& c,std::uint8_t* base)
{
    const auto fields=Fields(c);
    for(unsigned i=28u;i<=31u;++i) fields[i]->u64=PPC_LOAD_U64(c.r1.u32-40u+(i-28u)*8u);
    c.r12.u64=PPC_LOAD_U32(c.r1.u32-8u);c.lr=c.r12.u64;
}
void OriginalDiagnostic(PPCContext& c,std::uint8_t* base)
{
    auto s=FromPpc(c);GuestMemory m(0,std::span<std::uint8_t>(base,std::size_t{1}<<32));DiagnosticAction(m,s,diagnostic_calls++);
    const auto fields=Fields(c);const auto& g=s.numeric.classifier.integer;
    for(unsigned i=0;i<32u;++i) fields[i]->u64=i==1u ? g.sp : g.r[i];
    c.lr=g.lr;c.ctr.u64=g.ctr;c.xer.ca=g.xer_ca;c.xer.so=g.xer_so;
    c.cr0.lt=g.cr0.lt;c.cr0.gt=g.cr0.gt;c.cr0.eq=g.cr0.eq;c.cr0.so=g.cr0.so;
    c.cr6.lt=g.cr6.lt;c.cr6.gt=g.cr6.gt;c.cr6.eq=g.cr6.eq;c.cr6.so=g.cr6.so;
}
int main(){try{for(const auto& item:Cases)Check(item);for(unsigned mode=0;mode<5u;++mode)CheckMatch(mode);
 std::puts("PASS legacy-descriptor-numeric-match-routes 10 actual PPC cases");
 std::puts("LIMIT selected finite extraction and descriptor equivalence; diagnostic implementation, other layouts/context, exceptional numeric conversions, faults/MMIO/runtime open");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
