#include "lo_semantics/legacy_descriptor_rank_attachment.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
namespace {
using namespace lo::semantic::gpu;
namespace family=legacy_descriptor_rank_attachment;
constexpr GuestAddress Stack=0x80000u,Owner=0x10000u,Name=0x20000u;
constexpr GuestAddress Node=0x30000u,Descriptor=0x40020u,Attachment=0x50020u;
constexpr GuestAddress AttachmentOwner=0x54000u,Metadata=0x58000u;
constexpr GuestAddress Pool=0x60000u,Previous=0x70000u,Target=0x74000u;
constexpr GuestAddress SelectorTable=0x821712a0u,JumpTable=0x82174e48u;
constexpr std::array<test::Region,3> Regions{{{0u,0x90000u},
 {0x82171000u,0x1000u},{0x82174000u,0x1000u}}};
constexpr GuestAddress Second=0x56020u;
struct Case {const char* name; bool first_owned,second_owned;unsigned pools;};
constexpr std::array Cases{Case{"reuse-both",false,false,1u},Case{"copy-first",true,false,2u},Case{"copy-second",false,true,2u},Case{"copy-both",true,true,3u}};
unsigned pool_calls=0;
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

constexpr std::array<std::uint8_t, 97> JumpBytes{{
    0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x00,
    0x34, 0xe0, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0x4c, 0x4c,
    0x4c, 0xe0, 0xe0, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x4c, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0x64, 0x64, 0xe8, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe8, 0x74, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0,
    0xe0
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
        a.cached_fp_control == b.cached_fp_control;
}


PPCContext Initial(const Case& item) {
 (void)item;PPCContext c{};auto fields=Fields(c);
 for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800000000ull|Stack;c.r3.u64=Owner;c.r4.u64=Name;c.r5.u64=Attachment;c.r6.u64=Second;
 c.lr=0xabcdef0123456789ull;c.ctr.u64=0x5566778899aabbccull;c.cr0.gt=1;c.cr6.lt=1;c.xer.so=1;c.xer.ca=1;
 c.f0.u64=std::bit_cast<std::uint64_t>(5.0);c.f12.u64=std::bit_cast<std::uint64_t>(7.0);c.f13.u64=std::bit_cast<std::uint64_t>(9.0);c.fpscr.csr=0x1f80u;return c;
}
void Seed(GuestMemory& m,const Case& item) {
 for(unsigned i=0;i<SelectorBytes.size();++i)m.WriteU8(SelectorTable+i,SelectorBytes[i]);
 for(unsigned i=0;i<JumpBytes.size();++i)m.WriteU8(JumpTable+i,JumpBytes[i]);
 m.WriteU32(Owner+4u,Owner);m.WriteU32(Owner+16u,Previous);m.WriteU32(16u,Previous);m.WriteU32(Previous,0u);m.WriteU32(Owner+40u,0u);
 m.WriteU32(Owner+912u,Pool+4096u);m.WriteU32(Owner+916u,Pool);m.WriteU32(Name+24u,Node);m.WriteU32(Node,0u);
 for(auto a:{Attachment,Second}){m.WriteU32(a,(5u<<25u)|5u);m.WriteU32(a+4u,0u);m.WriteU32(a+8u,0x44556677u);m.WriteU32(a+12u,AttachmentOwner);}
 m.WriteU32(Attachment+16u,item.first_owned?Previous:0u);m.WriteU32(Second+16u,item.second_owned?Previous:0u);
 m.WriteU32(AttachmentOwner+4u,0u);m.WriteU32(0x50000u,Metadata);m.WriteU32(Metadata+148u,Owner);
}
struct Services final:family::Services {
 void AllocateFromPool(GuestMemory&,crt_stream_operations::Registers&) override
 {throw std::runtime_error("unexpected exhausted pool");}
 void SetHostFpControl(std::uint32_t csr) override {simde_mm_setcsr(csr);}
};
void Check(const Case& item) {
 std::fprintf(stderr,"clone %s seed\n",item.name);std::fflush(stderr);
 test::GuestWindow original(Regions),recovered(Regions);
 auto left=original.Memory(),right=recovered.Memory();Seed(left,item);Seed(right,item);
 auto context=Initial(item);auto state=FromPpc(context);Services services;pool_calls=0;
 const auto previous=simde_mm_getcsr();simde_mm_setcsr(0x1f80u);
 std::fprintf(stderr,"clone %s original\n",item.name);std::fflush(stderr);
 __imp__sub_82FF3730(context,original.Bytes());
 auto observed=FromPpc(context);const auto original_csr=simde_mm_getcsr();
 simde_mm_setcsr(0x1f80u);std::fprintf(stderr,"clone %s recovered\n",item.name);std::fflush(stderr);
 if(!family::Apply(0x82ff3730u,right,services,state))throw std::runtime_error("semantic entry missing");
 const auto recovered_csr=simde_mm_getcsr();simde_mm_setcsr(previous);
 if(pool_calls!=item.pools)throw std::runtime_error("original clone pool path");
 const auto result=static_cast<GuestAddress>(observed.integer.r[3]);
 if(left.ReadU32(result+40u)==0u||left.ReadU32(result+44u)==0u||left.ReadU32(Name+24u)!=result+36u||((left.ReadU32(result+8u)>>14u)&7u)!=5u){
  std::fprintf(stderr,"result=%08X first=%08X second=%08X ring=%08X header=%08X firstword=%08X\n",result,left.ReadU32(result+40u),left.ReadU32(result+44u),left.ReadU32(Name+24u),left.ReadU32(result+8u),left.ReadU32(left.ReadU32(result+40u)));
  throw std::runtime_error("dual attachment/rank/ring path");
 }
 if(!Same(observed,state)||original_csr!=recovered_csr||!original.EqualCommitted(recovered)) {
  for(unsigned i=0;i<32u;++i)if(observed.integer.r[i]!=state.integer.r[i])
   std::fprintf(stderr,"r%u %016llX/%016llX\n",i,(unsigned long long)observed.integer.r[i],(unsigned long long)state.integer.r[i]);
  std::fprintf(stderr,"LR %llX/%llX SP %llX/%llX CTR %llX/%llX\n",
   (unsigned long long)observed.integer.lr,(unsigned long long)state.integer.lr,
   (unsigned long long)observed.integer.sp,(unsigned long long)state.integer.sp,
   (unsigned long long)observed.integer.ctr,(unsigned long long)state.integer.ctr);
  for(auto region:Regions)for(std::size_t off=0;off<region.size;++off) {
   auto address=std::size_t(region.base)+off;
   if(original.Bytes()[address]!=recovered.Bytes()[address]) {
    std::fprintf(stderr,"RAM %08zX %02X/%02X\n",address,original.Bytes()[address],recovered.Bytes()[address]);
    throw std::runtime_error(item.name);
   }
  }
  throw std::runtime_error(item.name);
 }
}
}
void OriginalBody82FB6B28(PPCContext& c,std::uint8_t* b) {__imp__sub_82FB6B28(c,b);}
void OriginalBody82FBDA20(PPCContext& c,std::uint8_t* b) {__imp__sub_82FBDA20(c,b);}
void OriginalBody83056B90(PPCContext& c,std::uint8_t* b) {__imp__sub_83056B90(c,b);}
void OriginalBody82FAC238(PPCContext& c,std::uint8_t* b) {__imp__sub_82FAC238(c,b);}
void OriginalBody82FB36B0(PPCContext& c,std::uint8_t* b) {++pool_calls;__imp__sub_82FB36B0(c,b);}
void OriginalBody83056568(PPCContext& c,std::uint8_t* b) {__imp__sub_83056568(c,b);}
void OriginalBody82B7BC40(PPCContext& c,std::uint8_t* b) {__imp__sub_82B7BC40(c,b);}
void OriginalSave25(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=25u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),fields[i]->u64);
 m.WriteU32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore25(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=25u;i<=31u;++i)fields[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));
 c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;
}
void OriginalSave27(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=27u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),fields[i]->u64);
 m.WriteU32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore27(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=27u;i<=31u;++i)fields[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));
 c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;
}
void OriginalSave28(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=28u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),fields[i]->u64);
 m.WriteU32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore28(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=28u;i<=31u;++i)fields[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));
 c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;
}
void OriginalSave29(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=29u;i<=31u;++i)recovery_abi::WriteU64(m,c.r1.u32-8u*(33u-i),fields[i]->u64);
 m.WriteU32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore29(PPCContext& c,std::uint8_t* b) {
 auto m=GuestMemory(0u,std::span<std::uint8_t>(b,test::GuestWindow::Space));auto fields=Fields(c);
 for(unsigned i=29u;i<=31u;++i)fields[i]->u64=recovery_abi::ReadU64(m,c.r1.u32-8u*(33u-i));
 c.r12.u64=m.ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;
}
void OriginalNativeAllocate(PPCContext&,std::uint8_t*) {
 std::fputs("unexpected original exhausted pool\n",stderr);std::abort();
}
int main(){try {for(const auto& item:Cases)Check(item);
 std::printf("PASS legacy-descriptor-rank-attachment %zu actual PPC cases\n",Cases.size());
 std::puts("LIMIT selected ordinary RAM and descriptor layouts; pool exhaustion/native implementation, other PPCContext state, faults/MMIO/concurrency/runtime open");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
