#include "lo_semantics/legacy_float_digit_accumulator.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>
namespace
{
using namespace lo::semantic::gpu;
namespace family=legacy_float_digit_accumulator;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr test::Region Regions[]={{0,0x100000u}};
constexpr GuestAddress Input=0x10000u,Output=0x20000u,Stack=0x80000u;
struct Case { const char* name;std::array<std::uint8_t,20> digits;unsigned count,output_offset,sp_offset;std::array<std::uint32_t,3> expected; };
constexpr Case Cases[]={ {"one", {1}, 1, 0, 0, {0x3fff8000u,0x0u,0x0u}},
{"nine-word", {9}, 1, 4, 0, {0x40029000u,0x0u,0x0u}},
{"multi-byte", {1,2,3,4,5}, 5, 1, 0, {0x400cc0e4u,0x0u,0x0u}},
{"leading-zero", {0,0,1}, 3, 0, 3, {0x3fff8000u,0x0u,0x0u}},
{"word-carry", {4,2,9,4,9,6,7,2,9,6}, 10, 2, 0, {0x401f8000u,0x0u,0x0u}},
{"two-word-carry", {1,8,4,4,6,7,4,4,0,7,3,7,0,9,5,5,1,6,1,5}, 20, 0, 0, {0x403effffu,0xffffffffu,0xffff0000u}} };
std::array<PPCRegister*,32> Fields(PPCContext& c)
{
 return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
 family::Registers s{};auto fields=Fields(c);
 for(unsigned i=0;i<32;++i)s.r[i]=fields[i]->u64;
 s.lr=c.lr;s.ctr=c.ctr.u64;s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
 const PPCCRRegister* conditions[]={&c.cr0,&c.cr1,&c.cr2,&c.cr3,&c.cr4,&c.cr5,&c.cr6,&c.cr7};
 for(unsigned i=0;i<8;++i)s.cr[i]={conditions[i]->lt,conditions[i]->gt,conditions[i]->eq,conditions[i]->so};
 return s;
}
bool Same(const family::Registers& a,const family::Registers& b)
{ return a.r==b.r && a.cr==b.cr && a.lr==b.lr && a.ctr==b.ctr && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca; }
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xbd);recovered.Fill(0xbd);
 auto left=original.Memory(),right=recovered.Memory();
 for(unsigned i=0;i<item.count;++i){left.WriteU8(Input+i,item.digits[i]);right.WriteU8(Input+i,item.digits[i]);}
 PPCContext c{};auto fields=Fields(c);
 for(unsigned i=0;i<32;++i)fields[i]->u64=0x1234567800000000ull+i;
 c.r1.u64=0x1234567800000000ull| (Stack+item.sp_offset);
 c.r3.u64=0x5566778800000000ull|Input;
 c.r4.u64=0x1122334400000000ull|item.count;
 c.r5.u64=0x6677889900000000ull|(Output+item.output_offset);
 c.lr=0x99887766abcdef01ull;c.ctr.u64=0x8765432180000000ull;c.xer.so=1;c.xer.ca=0;
 c.cr0={1,0,0,{1}};c.cr1={0,1,0,{1}};c.cr6={1,0,0,{1}};c.cr7={0,0,1,{1}};
 auto state=FromPpc(c);
 __imp__sub_82297F38(c,original.Bytes());
 for(unsigned i=0;i<3;++i)if(left.ReadU32(Output+item.output_offset+4u*i)!=item.expected[i])
   throw std::runtime_error("digit accumulator independent expected record");
 if(!family::Apply(0x82297f38u,right,state) || !Same(FromPpc(c),state) || !original.EqualCommitted(recovered))
 {
  const auto observed=FromPpc(c);std::fprintf(stderr,"FAIL %s RAM=%u ctr=%llX/%llX\n",item.name,original.EqualCommitted(recovered),static_cast<unsigned long long>(observed.ctr),static_cast<unsigned long long>(state.ctr));
  for(unsigned i=0;i<32;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u=%llX/%llX\n",i,static_cast<unsigned long long>(observed.r[i]),static_cast<unsigned long long>(state.r[i]));
  for(unsigned i=0;i<8;++i)if(observed.cr[i]!=state.cr[i])std::fprintf(stderr,"cr%u=%u%u%u%u/%u%u%u%u\n",i,observed.cr[i].lt,observed.cr[i].gt,observed.cr[i].eq,observed.cr[i].so,state.cr[i].lt,state.cr[i].gt,state.cr[i].eq,state.cr[i].so);
  throw std::runtime_error("digit accumulator actual PPC mismatch");
 }
}
}
void OriginalSave27(PPCContext& c,std::uint8_t* base)
{
 auto fields=Fields(c);for(unsigned i=27;i<=31;++i)PPC_STORE_U64(c.r1.u32-8u*(33u-i),fields[i]->u64);
 PPC_STORE_U32(c.r1.u32-8u,c.r12.u32);
}
void OriginalRestore27(PPCContext& c,std::uint8_t* base)
{
 auto fields=Fields(c);for(unsigned i=27;i<=31;++i)fields[i]->u64=PPC_LOAD_U64(c.r1.u32-8u*(33u-i));
 c.r12.u64=PPC_LOAD_U32(c.r1.u32-8u);c.lr=c.r12.u64;
}
int main()
{
 try{for(const auto& item:Cases)Check(item);std::printf("PASS legacy-float-digit-accumulator %zu actual PPC cases\n",std::size(Cases));std::puts("LIMIT other inputs/aliasing, empty or zero-only nonterminating original inputs, faults, MMIO, concurrency and runtime remain open");return 0;}
 catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
