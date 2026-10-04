#define main ErrorUpper61SharedFixtureMain
#include "crt_stream_scan_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_error_format_upper61.h"
#include "lo_semantics/crt_format_frame61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_stream_counted_output.h"
#include <fstream>
#include <string>
namespace error_upper61_oracle {
namespace model=crt_error_format_upper61;
using Full=model::Registers;
constexpr GuestAddress Text=0x41000u,Message=0x41100u,ErrorText=0x41200u,PublicStream=0x83214b30u;
constexpr std::array<test::Region,11> UpperRegions{{
 {0u,0x120000u},{0x820d3000u,0x1000u},{0x82157000u,0x1000u},
 {0x82216000u,0x2000u},{0x831e0000u,0x10000u},{0x83214000u,0x3000u},
 {0x8321f000u,0x1000u},{0x83245000u,0x1000u},{0x832d3000u,0x2000u},
 {0x832ec000u,0x2000u},{0x83378000u,0x3000u}}};
enum class Route {PublicPrefix,Float,NullFormat};
struct Case{const char* name;Route route;GuestAddress entry;};
constexpr Case Cases[]={{"error-prefix-real-chain",Route::PublicPrefix,0x82df4270u},
 {"stream-varargs-float-live-frame",Route::Float,0x82df28c8u},
 {"stream-null-format-high-spill",Route::NullFormat,0x82df28c8u}};
using Trace=std::array<std::uint64_t,8>;
struct Bulk final:crt_stream_bulk_close_routes::NativeServices {
 std::vector<Trace> events;
 void EnterCriticalSection(GuestMemory&,crt_stream_operations::Registers& s) override {
  events.push_back({1,s.sp,s.lr,s.r[3],s.r[10],s.r[31],s.ctr,s.r[12]});s.r[10]=0x1234567812345678ull;
 }
 void LeaveCriticalSection(GuestMemory&,crt_stream_operations::Registers& s) override {
  events.push_back({2,s.sp,s.lr,s.r[3],s.r[10],s.r[31],s.ctr,s.r[12]});s.r[10]=0x8877665512345678ull;
 }
};
struct Guest final:crt_narrow_formatter61::GuestServices {
 std::vector<Trace> events;std::string output;
 void CallIndirect(GuestAddress target,GuestMemory& m,Full& s) override {
  if(target!=0x2400u||s.lr!=0x82b82ed0u||s.r[6]!=static_cast<unsigned>('f')||
     ReadU64(m,Address(s.r[3]))!=0x400c000000000000ull)
   throw std::runtime_error("unexpected actual float slot/packed argument/LR");
  events.push_back({target,s.r[1],s.lr,s.r[3],s.r[4],s.r[5],s.r[6],s.r[10]});
  for(unsigned i=0;i<4u;++i)m.WriteU8(Address(s.r[4]+i),static_cast<std::uint8_t>("3.5"[i]));
  s.r[10]=0xaabbccdd11223344ull;s.fpr_bits[3]=0x4010000000000000ull;s.cr1.gt^=1u;s.cr7.eq^=1u;
 }
 void CallOutput(GuestMemory& m,Full& s) override {
  if(s.lr!=0x82b83328u)throw std::runtime_error("unexpected actual error output LR");
  events.push_back({0x823add70u,s.r[1],s.lr,s.r[3],s.r[4],s.r[5],s.r[6],s.r[10]});
  for(unsigned i=0;i<512u;++i){auto c=m.ReadU8(Address(s.r[3]+i));if(!c)break;output+=static_cast<char>(c);}
  s.r[3]=0x8877665500000001ull;s.r[10]=0xaabbccdd55667788ull;
  s.fpr_bits[3]=0x4014000000000000ull;s.cr1.gt^=1u;s.cr7.eq^=1u;
 }
};
scan_oracle::ServicesBundle* original=nullptr;Bulk* original_bulk=nullptr;Guest* original_guest=nullptr;
model::Dependencies Deps(scan_oracle::ServicesBundle& s,Bulk& b,Guest& g){
 auto d=scan_oracle::Deps(s).accepted;
 return {{{d.close.pipeline,b},d.open,d.native},g};
}
void CopyImage(GuestMemory& m,GuestAddress a,std::size_t count){
 std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",std::ios::binary);
 if(!image)throw std::runtime_error("missing private image fixture");
 image.seekg(a-0x82000000u);std::vector<char> bytes(count);image.read(bytes.data(),static_cast<std::streamsize>(count));
 if(!image)throw std::runtime_error("short actual formatter image bytes");
 for(std::size_t i=0;i<count;++i)m.WriteU8(a+static_cast<GuestAddress>(i),static_cast<std::uint8_t>(bytes[i]));
}
void Seed(test::GuestWindow& w,Route route){
 scan_oracle::Seed(w,scan_oracle::Route::Literal);auto m=w.Memory();
 CopyImage(m,0x820d3000u,0x1000u);CopyImage(m,0x82216000u,0x2000u);CopyImage(m,0x82157d54u,4u);
 const char* format=route==Route::Float?"%f":"%s";
 for(unsigned i=0;;++i){m.WriteU8(Text+i,static_cast<std::uint8_t>(format[i]));if(!format[i])break;}
 for(unsigned i=0;i<4u;++i)m.WriteU8(Message+i,static_cast<std::uint8_t>("ctx"[i]));
 for(unsigned i=0;i<6u;++i)m.WriteU8(ErrorText+i,static_cast<std::uint8_t>("error"[i]));
 m.WriteU32(0x8321ffb8u,1u);m.WriteU32(0x8321ff08u,ErrorText);m.WriteU32(0x8321ff0cu,ErrorText);
 m.WriteU32(0x83215210u,0u);
 for(const auto stream:{Stream,PublicStream}){
  m.WriteU32(stream,Buffer);m.WriteU32(stream+4u,64u);m.WriteU32(stream+8u,Buffer);m.WriteU32(stream+12u,66u);
  m.WriteU32(stream+16u,5u);
 }
 // Actual indexed critical-section slot 18 is initialized, avoiding unrelated allocation.
 m.WriteU32(0x83215358u+18u*8u,0x62000u);
 m.WriteU32(0x83214fc8u+24u,0x2403u);
 m.WriteU32(0x83215300u,0x70000u);m.WriteU32(0x70000u+188u,0x70100u);
 m.WriteU32(0x70100u,0x70200u);m.WriteU8(0x70200u,'.');
}
PPCContext Initial(const Case& item){
 auto c=scan_oracle::Initial(scan_oracle::Route::Literal);
 c.r3.u64=item.route==Route::PublicPrefix?(0xaabbccdd00000000ull|Message):(0xaabbccdd00000000ull|Stream);
 c.r4.u64=item.route==Route::NullFormat?0u:0x1122334400000000ull|Text;
 c.r5.u64=item.route==Route::Float?0x400c000000000000ull:0x9988776655443322ull;
 c.r6.u64=0x8877665544332211ull;c.r7.u64=0x1234567887654321ull;
 c.r8.u64=0x1122334455667788ull;c.r9.u64=0x2233445566778899ull;c.r10.u64=0x33445566778899aaull;
 return c;
}
void Check(const Case& item){
 test::GuestWindow before(UpperRegions),after(UpperRegions);Seed(before,item.route);Seed(after,item.route);
 scan_oracle::ServicesBundle expected(before),actual(after);Bulk eb,ab;Guest eg,ag;
 auto c=Initial(item);const auto initial=crt_full_oracle::FromPpc(c);auto s=initial;
 current=&expected.stream;active=&expected.stream;current_index=&expected.index;current_host=&expected.host;
 wrapper_oracle::original_unlock=&expected.unlock;close_shared_oracle::active_extra=&expected.extra;
 scan_oracle::original_services=&expected;original=&expected;original_bulk=&eb;original_guest=&eg;
 if(item.entry==0x82df4270u)__imp__sub_82DF4270(c,before.Bytes());else __imp__sub_82DF28C8(c,before.Bytes());
 original=nullptr;original_bulk=nullptr;original_guest=nullptr;scan_oracle::original_services=nullptr;
 close_shared_oracle::active_extra=nullptr;wrapper_oracle::original_unlock=nullptr;
 current_host=nullptr;current_index=nullptr;active=nullptr;current=nullptr;
 if(!model::Apply(item.entry,actual.stream.memory,Deps(actual,ab,ag),s))throw std::runtime_error("missing actual error upper");
 const auto a=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),b=crt_full_oracle::Snapshot(s);
 if(a!=b||!before.EqualCommitted(after)||eg.events!=ag.events||eg.output!=ag.output||eb.events!=ab.events||
    expected.stream.events!=actual.stream.events||expected.stream.traps!=actual.stream.traps||
    expected.index.events!=actual.index.events||expected.host.events!=actual.host.events||
    expected.unlock.events!=actual.unlock.events||expected.extra.events!=actual.extra.events){
  for(unsigned i=0;i<a.size();++i)if(a[i]!=b[i])std::fprintf(stderr,"%s Full[%u] %llx/%llx\n",item.name,i,
   static_cast<unsigned long long>(a[i]),static_cast<unsigned long long>(b[i]));
  for(const auto r:UpperRegions)for(std::size_t i=0;i<r.size;++i)if(before.Bytes()[r.base+i]!=after.Bytes()[r.base+i]){
   std::fprintf(stderr,"%s RAM %08llx %02x/%02x\n",item.name,static_cast<unsigned long long>(r.base+i),
    before.Bytes()[r.base+i],after.Bytes()[r.base+i]);break;}
  throw std::runtime_error("error upper Full72/RAM/live callback mismatch");
 }
 auto m=before.Memory();
 if(c.r1.u64!=initial.r[1]||c.lr!=0x87654321u||c.r30.u64!=initial.r[30]||c.r31.u64!=initial.r[31])
  throw std::runtime_error("missed upper SP/high64/nonvolatile/saved LR restoration");
 if(item.route==Route::PublicPrefix){
  if(eg.output!="ctx: error\n"||eg.events.size()!=3u||c.r3.u32!=6u||expected.stream.locks!=3u)
   throw std::runtime_error("missed actual prefix/error text/three formatting calls");
 }else{
  for(unsigned i=0;i<6u;++i)if(ReadU64(m,Stack+32u+8u*i)!=initial.r[5u+i])
   throw std::runtime_error("missed full64 live varargs spill");
  if(item.route==Route::Float){
   if(c.r3.u32!=3u||m.ReadU8(Buffer)!='3'||m.ReadU8(Buffer+1u)!='.'||m.ReadU8(Buffer+2u)!='5'||
      eg.events.size()!=1u||eb.events.size()!=2u||eb.events[0][2]!=0x82b7b760u||eb.events[1][2]!=0x82df2ae4u||
      m.ReadU32(Stack-160u-16u)!=0x82df2abcu||c.f3.u64!=0x4010000000000000ull)
    throw std::runtime_error("missed float output or actual lock/unlock cleanup LR and live FPR");
  }else if(c.r3.u32!=0xffffffffu||m.ReadU32(0x83215210u)!=22u||expected.stream.traps!=1u||
          !eg.events.empty()||!eb.events.empty())
   throw std::runtime_error("missed genuine null-format errno22 before stream lock");
 }
}
}
void OriginalErrorUpper61Accepted(GuestAddress entry,PPCContext& c,std::uint8_t*){
 using namespace error_upper61_oracle;auto s=crt_full_oracle::FromPpc(c);const auto d=Deps(*original,*original_bulk,*original_guest);
 if(entry==0x82df2ac8u)crt_format_frame61::ApplySupport_DF2AC8(current->memory,d.accepted,s);
 else{
  auto lower=crt_context_adapter::ToStream(s);bool found=false;
  if(entry==0x82b7b708u)found=crt_stream_bulk_close_routes::Apply(entry,current->memory,d.accepted.close,lower);
  else found=crt_format_stream::Apply(entry,current->memory,d.accepted.close.pipeline.format,lower);
  if(!found)throw std::runtime_error("missing actual accepted error upper lower");
  crt_context_adapter::FromStream(s,lower);
 }
 crt_full_oracle::ToPpc(c,s);
}
void OriginalNarrow61Accepted(GuestAddress entry,PPCContext& c,std::uint8_t*){
 using namespace error_upper61_oracle;auto s=crt_full_oracle::FromPpc(c);const auto d=Deps(*original,*original_bulk,*original_guest);
 if(entry==0x82b86be0u)crt_reader_chain61::ApplySupport_B86BE0(current->memory,d.accepted,s);
 else if(entry==0x823add70u)d.guest.CallOutput(current->memory,s);
 else{
  auto lower=crt_context_adapter::ToStream(s);const auto& streams=d.accepted.close.pipeline.close.accepted;
  bool found=crt_stream_counted_output::Apply(entry,current->memory,streams,lower);
  if(!found)found=crt_stream_operations::Apply(entry,current->memory,streams,lower);
  if(!found)found=crt_stream_operations::ApplyAcceptedCallee(entry,current->memory,streams,lower);
  if(!found)throw std::runtime_error("missing actual accepted narrow lower");
  crt_context_adapter::FromStream(s,lower);
 }
 crt_full_oracle::ToPpc(c,s);
}
void OriginalNarrow61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*){
 auto s=crt_full_oracle::FromPpc(c);error_upper61_oracle::original_guest->CallIndirect(target,current->memory,s);
 crt_full_oracle::ToPpc(c,s);
}
int main(){for(const auto& item:error_upper61_oracle::Cases)try{error_upper61_oracle::Check(item);}
 catch(const std::exception& e){std::fprintf(stderr,"%s: %s\n",item.name,e.what());return 1;}
 std::puts("PASS crt-error-format-upper61 3 focused actual PPC cases");
 std::puts("LIMIT accepted selected/native ABI plus explicit Full float/output slots; ordinary RAM only, faults/MMIO/runtime unvalidated");return 0;}
