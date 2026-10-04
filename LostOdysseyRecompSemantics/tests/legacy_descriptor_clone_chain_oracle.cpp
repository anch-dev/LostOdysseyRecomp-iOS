#include "lo_semantics/legacy_descriptor_clone_chain.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
namespace {
using namespace lo::semantic::gpu;
namespace family=legacy_descriptor_clone_chain;
constexpr GuestAddress Stack=0x80000u,Owner=0x10000u,Name=0x20000u;
constexpr GuestAddress Node=0x30000u,Descriptor=0x40020u,Attachment=0x50020u;
constexpr GuestAddress AttachmentOwner=0x54000u,Metadata=0x58000u;
constexpr GuestAddress Pool=0x60000u,Previous=0x70000u,Target=0x74000u;
constexpr GuestAddress SelectorTable=0x821712a0u,JumpTable=0x82174e48u;
constexpr std::array<test::Region,3> Regions{{{0u,0x90000u},
 {0x82171000u,0x1000u},{0x82174000u,0x1000u}}};
struct Case {const char* name;GuestAddress entry;unsigned kind;
 bool attached,linked;unsigned pools;};
constexpr std::array Cases{
 Case{"copy-five-words",0x82fb6b28u,0u,false,false,1u},
 Case{"merge-bit-conflict",0x82fbd450u,2u,false,false,0u},
 Case{"merge-existing-bit4",0x82fbd450u,4u,false,false,0u},
 Case{"attach-reuse",0x82fbda20u,0u,false,false,0u},
 Case{"attach-copy-owned",0x82fbda20u,0u,true,false,1u},
 Case{"allocate-unlinked",0x83056b90u,58u,false,false,1u},
 Case{"allocate-linked",0x83056b90u,59u,false,true,1u},
 Case{"constructor58",0x83057c38u,58u,false,false,1u},
 Case{"constructor59",0x83057ce0u,59u,false,false,1u},
 Case{"constructor60",0x83057d88u,60u,false,false,1u},
 Case{"constructor61",0x83057e30u,61u,false,false,1u},
 Case{"constructor-copy-owned",0x83057c38u,58u,true,false,2u},
 Case{"clone58",0x83058ec8u,58u,false,false,1u},
 Case{"clone59-owned",0x83058ec8u,59u,true,false,2u},
 Case{"clone60",0x83058ec8u,60u,false,false,2u},
 Case{"clone61",0x83058ec8u,61u,false,false,2u},
 Case{"clone59-linked-attachment",0x83058ec8u,59u,false,true,2u},
 Case{"clone-unrecognized-skip",0x83058ec8u,7u,false,true,0u}
};
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
 PPCContext c{};auto fields=Fields(c);
 for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800000000ull|Stack;c.r3.u64=Owner;
 c.r4.u64=Name;c.r5.u64=Attachment;c.r6.u64=item.kind;
 c.r7.u64=1u;c.r8.u64=1u;
 if(item.entry==0x82fb6b28u){c.r4.u64=Attachment;c.r5.u64=AttachmentOwner;}
 if(item.entry==0x82fbd450u){c.r3.u64=Attachment;c.r4.u64=item.kind;}
 if(item.entry==0x82fbda20u){c.r3.u64=Target;c.r4.u64=Attachment;}
 if(item.entry==0x83056b90u)c.r5.u64=item.linked?Name+24u:0u;
 if(item.entry==0x83058ec8u){c.r3.u64=Descriptor;c.r4.u64=Owner;}
 c.lr=0xabcdef0123456789ull;c.ctr.u64=0x5566778899aabbccull;
 c.cr0.gt=1;c.cr6.lt=1;c.xer.so=1;c.xer.ca=1;
 c.f0.u64=std::bit_cast<std::uint64_t>(5.0);
 c.f12.u64=std::bit_cast<std::uint64_t>(7.0);
 c.f13.u64=std::bit_cast<std::uint64_t>(9.0);c.fpscr.csr=0x1f80u;return c;
}
void Seed(GuestMemory& m,const Case& item) {
 for(unsigned i=0;i<SelectorBytes.size();++i)m.WriteU8(SelectorTable+i,SelectorBytes[i]);
 for(unsigned i=0;i<JumpBytes.size();++i)m.WriteU8(JumpTable+i,JumpBytes[i]);
 m.WriteU32(Owner+4u,Owner);m.WriteU32(Owner+16u,Previous);
 m.WriteU32(16u,Previous);m.WriteU32(Previous,0u);m.WriteU32(Owner+40u,0u);
 m.WriteU32(Owner+912u,Pool+4096u);m.WriteU32(Owner+916u,Pool);
 m.WriteU32(Name+24u,Node);m.WriteU32(Node,0u);
 m.WriteU32(Descriptor,0u);m.WriteU32(Descriptor+8u,item.kind<<7u);
 m.WriteU32(Descriptor+24u,Name);m.WriteU32(Descriptor+40u,Attachment);
 m.WriteU32(Attachment,0x11223305u);m.WriteU32(Attachment+4u,0u);
 m.WriteU32(Attachment+8u,0x44556677u);m.WriteU32(Attachment+12u,AttachmentOwner);
 m.WriteU32(Attachment+16u,item.attached?Previous:0u);
 m.WriteU32(AttachmentOwner+4u,0u);m.WriteU32(0x50000u,Metadata);
 m.WriteU32(Metadata+148u,Owner);m.WriteU32(Target,0u);
 if(item.entry==0x83058ec8u && item.linked) {
  m.WriteU32(Descriptor,Attachment);
  if(item.kind==7u)m.WriteU32(Attachment,0x0e000000u);
 }
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
 switch(item.entry) {
 case 0x82fb6b28u:__imp__sub_82FB6B28(context,original.Bytes());break;
 case 0x82fbd450u:__imp__sub_82FBD450(context,original.Bytes());break;
 case 0x82fbda20u:__imp__sub_82FBDA20(context,original.Bytes());break;
 case 0x83056b90u:__imp__sub_83056B90(context,original.Bytes());break;
 case 0x83057c38u:__imp__sub_83057C38(context,original.Bytes());break;
 case 0x83057ce0u:__imp__sub_83057CE0(context,original.Bytes());break;
 case 0x83057d88u:__imp__sub_83057D88(context,original.Bytes());break;
 case 0x83057e30u:__imp__sub_83057E30(context,original.Bytes());break;
 case 0x83058ec8u:__imp__sub_83058EC8(context,original.Bytes());break;
 default:throw std::runtime_error("entry missing");}
 auto observed=FromPpc(context);const auto original_csr=simde_mm_getcsr();
 simde_mm_setcsr(0x1f80u);std::fprintf(stderr,"clone %s recovered\n",item.name);std::fflush(stderr);
 if(!family::Apply(item.entry,right,services,state))throw std::runtime_error("semantic entry missing");
 const auto recovered_csr=simde_mm_getcsr();simde_mm_setcsr(previous);
 if(pool_calls!=item.pools)throw std::runtime_error("original clone pool path");
 if(item.entry==0x83058ec8u && item.kind==7u && observed.integer.r[3]!=0u)
  throw std::runtime_error("unrecognized clone result");
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
void OriginalBody82FBD450(PPCContext& c,std::uint8_t* b) {__imp__sub_82FBD450(c,b);}
void OriginalBody82FBDA20(PPCContext& c,std::uint8_t* b) {__imp__sub_82FBDA20(c,b);}
void OriginalBody83056B90(PPCContext& c,std::uint8_t* b) {__imp__sub_83056B90(c,b);}
void OriginalBody83057C38(PPCContext& c,std::uint8_t* b) {__imp__sub_83057C38(c,b);}
void OriginalBody83057CE0(PPCContext& c,std::uint8_t* b) {__imp__sub_83057CE0(c,b);}
void OriginalBody83057D88(PPCContext& c,std::uint8_t* b) {__imp__sub_83057D88(c,b);}
void OriginalBody83057E30(PPCContext& c,std::uint8_t* b) {__imp__sub_83057E30(c,b);}
void OriginalBody83058EC8(PPCContext& c,std::uint8_t* b) {__imp__sub_83058EC8(c,b);}
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
 std::printf("PASS legacy-descriptor-clone-chain %zu actual PPC cases\n",Cases.size());
 std::puts("LIMIT selected ordinary RAM and descriptor layouts; pool exhaustion/native implementation, other PPCContext state, faults/MMIO/concurrency/runtime open");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
