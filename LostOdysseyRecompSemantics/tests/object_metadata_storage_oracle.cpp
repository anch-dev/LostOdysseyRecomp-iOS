#include "lo_semantics/object_metadata_storage.h"
#include "semantic_oracle_support.h"
#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace
{
using namespace lo::semantic::gpu;
namespace family=object_metadata_storage;
constexpr GuestAddress Descriptor=0x10000u,Value=0x11000u,Data=0x40000u,NewData=0x60000u,Manager=0x20000u,Vtable=0x21000u,ManagerSlot=0x8330b608u;
constexpr test::Region Regions[]={{0,0x100000u},{0x8330b000u,0x1000u}};
struct Case {const char* name;GuestAddress entry;std::uint32_t pointer,count,capacity;std::uint64_t r4;bool lazy,alias,fail;std::uint64_t product;};
constexpr Case Cases[]={
 {"append-spare",0x825f41e8u,Data,1,4,Value,false,false,false,0},
 {"append-lazy-alias",0x825f41e8u,Data,4,4,Descriptor+4u,true,true,false,152},
 {"grow-high-word",0x822c42d8u,Data,1,4,0xffffffff00000001ull,false,false,false,0},
 {"realloc-empty",0x8229f678u,0,0,0,4,false,false,false,0},
 {"realloc-zero-cap-failure",0x8229f678u,Data,0,0,4,false,false,true,0},
 {"realloc-full-product",0x8229f678u,0,0,0x7fffffffu,0x7fffffffu,false,false,false,0x3fffffff00000001ull}};
using Event=std::array<std::uint64_t,8>;using Events=std::vector<Event>;
const Case* active{};test::GuestWindow* original_window{};Events original_events;
std::array<PPCRegister*,32> Fields(PPCContext& c)
{return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};}
family::Registers FromPpc(PPCContext& c)
{
 family::Registers s{};auto f=Fields(c);for(unsigned i=0;i<32;++i)s.r[i]=f[i]->u64;s.lr=c.lr;s.ctr=c.ctr.u64;s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
 s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};s.f0_bits=c.f0.u64;s.f1_bits=c.f1.u64;s.f13_bits=c.f13.u64;s.f30_bits=c.f30.u64;s.f31_bits=c.f31.u64;return s;
}
void ToPpc(PPCContext& c,const family::Registers& s)
{
 auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=s.r[i];c.lr=s.lr;c.ctr.u64=s.ctr;c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
 c.cr6.lt=s.cr6.lt;c.cr6.gt=s.cr6.gt;c.cr6.eq=s.cr6.eq;c.cr6.so=s.cr6.un;c.f0.u64=s.f0_bits;c.f1.u64=s.f1_bits;c.f13.u64=s.f13_bits;c.f30.u64=s.f30_bits;c.f31.u64=s.f31_bits;
}
bool Same(const family::Registers& a,const family::Registers& b)
{return a.r==b.r && a.lr==b.lr && a.ctr==b.ctr && a.cr6==b.cr6 && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca && a.f0_bits==b.f0_bits && a.f1_bits==b.f1_bits && a.f13_bits==b.f13_bits && a.f30_bits==b.f30_bits && a.f31_bits==b.f31_bits && a.cached_fp_control==b.cached_fp_control;}
void Initialize(GuestMemory& m,family::Registers& s,Events& events)
{
 events.push_back({1,s.r[3],s.r[4],s.r[5],s.r[6],s.r[1],s.lr,0});m.WriteU32(ManagerSlot,Manager);s.r[3]=0x99999999u;s.r[10]=0xaabbccdd01010101ull;s.r[13]=0xaabbccdd0000f000ull;
}
void Allocate(GuestAddress target,GuestMemory& m,family::Registers& s,Events& events)
{
 events.push_back({2,s.r[3],s.r[4],s.r[5],s.r[6],s.r[1],s.lr,target});
 if(target!=0x70000u || s.r[3]!=Manager || s.r[4]!=active->pointer || s.r[5]!=active->product)throw std::runtime_error("allocator original independent argument expectation");
 if(active->alias)m.WriteU32(Descriptor+4u,9u);
 s.r[3]=active->fail?0:0x1234567800060000ull;s.r[10]=0x1122334400001234ull;s.cr6={0,0,1,1};s.xer_ca=1;s.f1_bits=0x7ff8000000000042ull;
}
class Services final:public family::PpcBoundaryServices
{
public:Events events;
 void InitializeManager827C5F38(GuestMemory& m,family::Registers& s)override{Initialize(m,s,events);}
 void CallAllocator(GuestAddress t,GuestMemory& m,family::Registers& s)override{Allocate(t,m,s,events);}
};
void Seed(GuestMemory& m,const Case& item)
{
 m.WriteU32(Descriptor,item.pointer);m.WriteU32(Descriptor+4u,item.count);m.WriteU32(Descriptor+8u,item.capacity);m.WriteU32(Value,0xaabbccddu);
 m.WriteU32(ManagerSlot,item.lazy?0u:Manager);m.WriteU32(Manager,Vtable);m.WriteU32(Vtable+8u,0x70003u);
}
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xcd);recovered.Fill(0xcd);auto left=original.Memory(),right=recovered.Memory();Seed(left,item);Seed(right,item);
 PPCContext c{};auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=0x9988776600000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r3.u64=0x1122334400010000ull;c.r4.u64=item.r4;c.r5.u64=0xffffffff80000007ull;c.r6.u64=8;c.lr=0x88776655abcdef01ull;c.ctr.u64=0xaabbccdd55556666ull;c.xer.so=1;c.xer.ca=0;c.cr6.gt=1;c.cr6.so=1;
 c.f1.u64=0x3ff0000000000000ull;c.f0.u64=0x8000000000000000ull;c.f13.u64=0x4000000000000000ull;c.f30.u64=0x4008000000000000ull;c.f31.u64=0x4010000000000000ull;
 auto state=FromPpc(c);active=&item;original_window=&original;original_events.clear();
 switch(item.entry){case 0x825f41e8u:__imp__sub_825F41E8(c,original.Bytes());break;case 0x822c42d8u:__imp__sub_822C42D8(c,original.Bytes());break;default:__imp__sub_8229F678(c,original.Bytes());break;}
 const unsigned expected_calls=(item.entry==0x825f41e8u && !item.alias) || item.entry==0x822c42d8u || (!item.pointer&&!item.capacity)?0u:(item.lazy?2u:1u);
 if(original_events.size()!=expected_calls)throw std::runtime_error("storage original path");
 if(item.entry==0x825f41e8u)
 {
  const auto count=item.alias?9u:item.count+1u;const auto pointer=item.alias?NewData:item.pointer;const auto value=item.alias?9u:0xaabbccddu;
  if(c.r3.u64!=count-1u || left.ReadU32(Descriptor+4u)!=count || left.ReadU32(pointer+item.count*4u)!=value)throw std::runtime_error("append original independent result");
 }
 else if(item.entry==0x822c42d8u){if(c.r3.u64!=1u || left.ReadU32(Descriptor+4u)!=2u)throw std::runtime_error("grow original high-word expectation");}
 else if(item.capacity || item.pointer){if(left.ReadU32(Descriptor)!=(item.fail?0u:NewData) || c.r3.u64!=(item.fail?0ull:0x1234567800060000ull))throw std::runtime_error("reallocate original pointer result");}
 Services services;if(!family::Apply(item.entry,right,services,state) || !Same(FromPpc(c),state) || !original.EqualCommitted(recovered) || services.events!=original_events)
 {const auto observed=FromPpc(c);std::fprintf(stderr,"FAIL %s RAM=%u events=%zu/%zu\n",item.name,original.EqualCommitted(recovered),original_events.size(),services.events.size());for(unsigned i=0;i<32;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u=%llX/%llX\n",i,static_cast<unsigned long long>(observed.r[i]),static_cast<unsigned long long>(state.r[i]));throw std::runtime_error("metadata storage actual PPC mismatch");}
}
}
void OriginalStorageSave27(PPCContext& c,std::uint8_t* base)
{auto f=Fields(c);for(unsigned i=27;i<=31;++i)PPC_STORE_U64(c.r1.u32-8u*(33u-i),f[i]->u64);PPC_STORE_U32(c.r1.u32-8u,c.r12.u32);}
void OriginalStorageRestore27(PPCContext& c,std::uint8_t* base)
{auto f=Fields(c);for(unsigned i=27;i<=31;++i)f[i]->u64=PPC_LOAD_U64(c.r1.u32-8u*(33u-i));c.r12.u64=PPC_LOAD_U32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalStorageInitialize(PPCContext& c,std::uint8_t*)
{auto state=FromPpc(c);auto memory=original_window->Memory();Initialize(memory,state,original_events);ToPpc(c,state);}
void OriginalStorageAllocator(std::uint32_t t,PPCContext& c,std::uint8_t*)
{auto state=FromPpc(c);auto memory=original_window->Memory();Allocate(t,memory,state,original_events);ToPpc(c,state);}
int main()
{try{for(const auto& item:Cases)Check(item);std::printf("PASS object-metadata-storage 3 entries %zu actual PPC cases\n",std::size(Cases));std::puts("LIMIT manager initializer 827C5F38, allocator guest internals, other inputs, faults/MMIO/concurrency/runtime open");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
