#define main Narrow61SharedFixtureMain
#include "crt_stream_scan_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_narrow_formatter61.h"
#include "lo_semantics/crt_narrow_callers61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_stream_counted_output.h"
#include <fstream>
#include <string>
namespace narrow61_oracle {
namespace model=crt_narrow_formatter61;
using Full=model::Registers;
constexpr GuestAddress Text=0x41000u,Args=0x42000u,Out=0x52000u;
constexpr GuestAddress RecursiveStream=0x83214b10u,FloatSlot=0x83214fc8u;
constexpr std::array<test::Region,9> NarrowRegions{{
 {0u,0x120000u},{0x820d3000u,0x1000u},{0x82157000u,0x1000u},
 {0x831e0000u,0x10000u},{0x83214000u,0x3000u},{0x83245000u,0x1000u},
 {0x832d3000u,0x2000u},{0x832ec000u,0x2000u},{0x83378000u,0x3000u}}};
enum class Route {Literal,IntegerTail,Recursive,Float};
struct Case {const char* name;Route route;GuestAddress entry;const char* output;};
constexpr Case Cases[]={{"buffer-literal-terminator",Route::Literal,0x82b7cb20u,"OK"},
 {"buffer-live-float-slot",Route::Float,0x82b7cb20u,"3.5"},
 {"stdout-real-recursive-cleanup",Route::Recursive,0x82b85300u,"7"}};
using Trace=std::array<std::uint64_t,10>;
struct Guest final:model::GuestServices {
 std::vector<Trace> events;std::string output;
 void CallIndirect(GuestAddress target,GuestMemory& m,Full& s) override {
  if(target!=0x2400u||s.lr!=0x82b82ed0u||s.r[6]!=static_cast<unsigned>('f')||
     ReadU64(m,Address(s.r[3]))!=0x400c000000000000ull)
   throw std::runtime_error("unexpected live narrow float slot/packed argument/LR");
  events.push_back({target,s.r[1],s.lr,s.r[3],s.r[4],s.r[5],s.r[6],s.r[7],s.ctr,s.r[10]});
  for(unsigned i=0;i<4u;++i)m.WriteU8(Address(s.r[4]+i),static_cast<std::uint8_t>("3.5"[i]));
  s.r[10]=0xaabbccdd11223344ull;s.fpr_bits[3]=0x4010000000000000ull;
  s.cr1.gt^=1u;s.cr7.eq^=1u;
 }
 void CallOutput(GuestMemory& m,Full& s) override {
  if(s.lr!=0x82b83328u)throw std::runtime_error("unexpected recursive output LR");
  events.push_back({0x823add70u,s.r[1],s.lr,s.r[3],s.r[4],s.r[5],s.r[6],s.r[7],s.ctr,s.r[10]});
  for(unsigned i=0;i<512u;++i){auto c=m.ReadU8(Address(s.r[3]+i));if(!c)break;output+=static_cast<char>(c);}
  s.r[3]=0x8877665500000001ull;s.r[10]=0xaabbccdd55667788ull;
  s.fpr_bits[3]=0x4014000000000000ull;s.cr1.gt^=1u;s.cr7.eq^=1u;
 }
};
scan_oracle::ServicesBundle* original=nullptr;Guest* original_guest=nullptr;
model::Dependencies Deps(scan_oracle::ServicesBundle& s,Guest& g){return {scan_oracle::Deps(s).accepted,g};}
void Seed(test::GuestWindow& w,Route route){
 scan_oracle::Seed(w,scan_oracle::Route::Literal);auto m=w.Memory();
 // Actual original image page contains the classifier, both jump tables,
 // sign/digit strings and padding literals; no synthesized dispatch bytes.
 std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",std::ios::binary);
 if(!image)throw std::runtime_error("missing private image fixture");
 image.seekg(0xd3000);std::array<char,0x1000> page{};
 image.read(page.data(),page.size());if(!image)throw std::runtime_error("short narrow formatter image page");
 for(unsigned i=0;i<page.size();++i)m.WriteU8(0x820d3000u+i,static_cast<std::uint8_t>(page[i]));
 const char* text=route==Route::Literal?"OK":route==Route::IntegerTail?"%4d":route==Route::Float?"%f":"%d";
 for(unsigned i=0;;++i){m.WriteU8(Text+i,static_cast<std::uint8_t>(text[i]));if(!text[i])break;}
 const auto stream=route==Route::Recursive?RecursiveStream:Stream;
 m.WriteU32(stream,Out);m.WriteU32(stream+4u,64u);m.WriteU32(stream+8u,Out);
 m.WriteU32(stream+12u,66u);m.WriteU32(FloatSlot+24u,0x2403u);
 WriteU64(m,Args,route==Route::Float?0x400c000000000000ull:7ull);
 m.WriteU32(0x83215358u+17u*8u,0x63000u);
 m.WriteU32(0x83215300u,0x70000u);m.WriteU32(0x70000u+188u,0x70100u);
 m.WriteU32(0x70100u,0x70200u);m.WriteU8(0x70200u,'.');
}
PPCContext Initial(const Case& item){
 auto c=scan_oracle::Initial(scan_oracle::Route::Literal);
 c.r1.u64=0x8877665500000000ull|Stack;c.lr=0x1234567887654321ull;
 c.r3.u64=0xaabbccdd00000000ull|(item.route==Route::Recursive?Text:Out);
 c.r4.u64=item.route==Route::Recursive?7u:0x1122334400000000ull|Text;
 c.r5.u64=item.route==Route::Float?0x400c000000000000ull:7u;
 return c;
}

void Check(const Case& item){
 test::GuestWindow before(NarrowRegions),after(NarrowRegions);Seed(before,item.route);Seed(after,item.route);
 scan_oracle::ServicesBundle expected(before),actual(after);Guest eg,ag;
 auto c=Initial(item);const auto initial_r31=c.r31.u64;auto s=crt_full_oracle::FromPpc(c);
 current=&expected.stream;active=&expected.stream;current_index=&expected.index;current_host=&expected.host;
 wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;
 scan_oracle::original_services=&expected;original=&expected;original_guest=&eg;
 if(item.entry==0x82b7cb20u)__imp__sub_82B7CB20(c,before.Bytes());else __imp__sub_82B85300(c,before.Bytes());
 original=nullptr;original_guest=nullptr;scan_oracle::original_services=nullptr;
 close_shared_oracle::active_extra=nullptr;wrapper_oracle::original_unlock=nullptr;
 current_host=nullptr;current_index=nullptr;active=nullptr;current=nullptr;
 if(!crt_narrow_callers61::Apply(item.entry,actual.stream.memory,Deps(actual,ag),s))throw std::runtime_error("missing narrow entry");
 auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||eg.events!=ag.events||eg.output!=ag.output||
    expected.stream.events!=actual.stream.events||expected.stream.traps!=actual.stream.traps||
    expected.index.events!=actual.index.events||expected.host.events!=actual.host.events||
    expected.unlock.events!=actual.unlock.events||expected.extra.events!=actual.extra.events){
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"%s Full[%u] %llx/%llx\n",item.name,i,
    static_cast<unsigned long long>(a[i]),static_cast<unsigned long long>(b[i]));
  for(const auto r:NarrowRegions)for(std::size_t i=0;i<r.size;++i)if(before.Bytes()[r.base+i]!=after.Bytes()[r.base+i]){
    std::fprintf(stderr,"%s RAM %08llx %02x/%02x\n",item.name,static_cast<unsigned long long>(r.base+i),
     before.Bytes()[r.base+i],after.Bytes()[r.base+i]);break;}
  throw std::runtime_error("narrow Full72/RAM/callback mismatch");
 }
 std::string output;
 if(item.route==Route::Recursive)output=eg.output;
 else for(unsigned i=0;i<std::char_traits<char>::length(item.output);++i)output+=static_cast<char>(before.Memory().ReadU8(Out+i));
 if(output!=item.output||c.r3.u32!=std::char_traits<char>::length(item.output)||
    c.r1.u64!=(0x8877665500000000ull|Stack)||c.lr!=0x87654321u||c.r31.u64!=initial_r31)
  throw std::runtime_error("missed independent narrow output/count/SP/LR/nonvolatile expectation");
 if((item.route==Route::Float||item.route==Route::Recursive)&&eg.events.size()!=1u)
  throw std::runtime_error("missed actual live narrow callback");
 if(c.r1.u64!=(0x8877665500000000ull|Stack)||c.lr!=0x87654321u)
  throw std::runtime_error("caller high SP and restored LR missed");
 if(item.route==Route::Recursive) {
  if(expected.index.events.size()!=1u||expected.index.events[0][2]!=0x82b85410u||
     expected.index.events[0][3]!=0x63000u||expected.stream.locks!=1u)
   throw std::runtime_error("stdout actual saved cleanup LR/indexed unlock missed");
 } else if(before.Memory().ReadU8(Out+std::string(item.output).size())!=0u)
  throw std::runtime_error("buffer actual terminator missed");

}
}
void OriginalNarrow61Accepted(GuestAddress entry,PPCContext& c,std::uint8_t*){
 using namespace narrow61_oracle;
 auto s=crt_full_oracle::FromPpc(c);const auto d=Deps(*original,*original_guest);
 if(entry==0x82b86be0u)crt_reader_chain61::ApplySupport_B86BE0(current->memory,d.accepted,s);
 else if(entry==0x823add70u)d.guest.CallOutput(current->memory,s);
 else {
  auto lower=crt_context_adapter::ToStream(s);const auto& streams=d.accepted.close.pipeline.close.accepted;
  bool found=crt_stream_counted_output::Apply(entry,current->memory,streams,lower);
  if(!found)found=crt_stream_operations::Apply(entry,current->memory,streams,lower);
  if(!found)found=crt_stream_operations::ApplyAcceptedCallee(entry,current->memory,streams,lower);
  if(!found)throw std::runtime_error("missing accepted narrow oracle lower");
  crt_context_adapter::FromStream(s,lower);
 }
 crt_full_oracle::ToPpc(c,s);
}
void OriginalNarrow61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);
 narrow61_oracle::original_guest->CallIndirect(target,current->memory,s);crt_full_oracle::ToPpc(c,s);
}
void OriginalNarrowCallerLower(GuestAddress entry,PPCContext& c,std::uint8_t*) {
 auto state=crt_full_oracle::FromPpc(c);
 if(!crt_narrow_callers61::ApplyAcceptedLower(entry,current->memory,
     narrow61_oracle::Deps(*narrow61_oracle::original,*narrow61_oracle::original_guest),state))
  throw std::runtime_error("missing actual narrow caller lower");
 crt_full_oracle::ToPpc(c,state);
}
int main(){
 for(const auto& item:narrow61_oracle::Cases)try{narrow61_oracle::Check(item);}
 catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
 std::puts("PASS crt-narrow-callers61 3 focused actual PPC cases");
 std::puts("LIMIT accepted selected lower ABI and explicit mutable float/output guest calls; fault/MMIO/native/runtime unvalidated");return 0;
}
