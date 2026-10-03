// Appended after verbatim generated PPC bodies and actual ABI helpers.
#include "lo_semantics/allocation_array.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::uint64_t GuestSpace=std::uint64_t{1}<<32;
constexpr std::size_t LowCommit=0x40000,LowCompare=0x3e000;
constexpr GuestAddress StackTop=0x3f000,Array=0x10000,Data=0x12000;
constexpr GuestAddress Replacement=0x21000,Manager=0x35000,VTable=0x35100;
constexpr GuestAddress Global=0x8330b608,GlobalPage=Global&~0xfffu;
constexpr GuestAddress ResizeMethod=0x8234567b;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
enum class Kind{Resize,Remove};
struct Case{
    Kind kind=Kind::Remove;
    const char* name="";
    std::uint32_t length=8,capacity=12,stride=8,first=2,count=2,argument=0x4a;
    bool null_data=false,null_manager=false,null_result=false;
    bool mutate_callback=false;
    std::uint32_t seed=0x900d;
};
struct Event{
    char kind;
    std::array<std::uint32_t,5> args{};
    std::array<std::uint32_t,3> before{},after{};
    bool operator==(const Event&)const=default;
};
struct Run{
    std::uint8_t* bytes;const Case& c;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress address)const{
        return (std::uint32_t(bytes[address])<<24)|(std::uint32_t(bytes[address+1])<<16)|
            (std::uint32_t(bytes[address+2])<<8)|bytes[address+3];
    }
    void Write32(GuestAddress address,std::uint32_t value){
        bytes[address]=std::uint8_t(value>>24);bytes[address+1]=std::uint8_t(value>>16);
        bytes[address+2]=std::uint8_t(value>>8);bytes[address+3]=std::uint8_t(value);
    }
    void Snapshot(){
        auto& snapshot=snapshots.emplace_back();
        snapshot.insert(snapshot.end(),bytes,bytes+LowCompare);
        snapshot.insert(snapshot.end(),bytes+GlobalPage,bytes+GlobalPage+0x1000);
    }
    std::array<std::uint32_t,3> State()const{
        return {Read32(Array),Read32(Array+4),Read32(Array+8)};
    }
    void Begin(char kind,std::array<std::uint32_t,5> args){
        Snapshot();events.push_back({kind,args,State(),{}});
    }
    void End(){events.back().after=State();Snapshot();}
    void Initialize(){
        Begin('I',{});Write32(Global,Manager);End();
    }
    GuestAddress Resize(GuestAddress method,GuestAddress manager,GuestAddress old_storage,
                         std::uint32_t bytes_requested,std::uint32_t argument){
        Begin('R',{method,manager,old_storage,bytes_requested,argument});
        Require(method==(ResizeMethod&~3u)&&manager==Manager,
                "original vtable method and manager arguments");
        if(c.mutate_callback){
            Write32(Array+4,Read32(Array+4)^0x40u);
            Write32(Global,Manager);
        }
        if(!c.null_result && old_storage!=0 && bytes_requested!=0){
            const std::size_t old_bytes=std::size_t(c.capacity)*c.stride;
            const std::size_t copy=(std::min)(old_bytes,std::size_t(bytes_requested));
            Require(copy<=0x10000,"bounded synthetic reallocate copy");
            std::memcpy(bytes+Replacement,bytes+old_storage,copy);
        }
        End();return c.null_result?0:Replacement;
    }
};
Run* active=nullptr;
struct Services final:ArrayResizeServices{
    Run& run;explicit Services(Run& value):run(value){}
    void InitializeManager()override{run.Initialize();}
    GuestAddress ResizeStorage(GuestAddress method,GuestAddress manager,
        GuestAddress old_storage,std::uint32_t bytes,std::uint32_t argument)override{
        return run.Resize(method,manager,old_storage,bytes,argument);
    }
};
struct GuestWindow{
    std::uint8_t* base;
    GuestWindow(){
        base=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,GuestSpace,MEM_RESERVE,PAGE_NOACCESS));
        Require(base!=nullptr,"reserve sparse 4 GiB guest address space");
        Require(VirtualAlloc(base,LowCommit,MEM_COMMIT,PAGE_READWRITE)==base,"commit low guest/stack");
        Require(VirtualAlloc(base+GlobalPage,0x1000,MEM_COMMIT,PAGE_READWRITE)==base+GlobalPage,
                "commit original global allocator page");
    }
    ~GuestWindow(){if(base)VirtualFree(base,0,MEM_RELEASE);}
    GuestWindow(const GuestWindow&)=delete;GuestWindow& operator=(const GuestWindow&)=delete;
};
void Setup(Run& run){
    std::mt19937 rng(run.c.seed);
    for(std::size_t i=0;i<LowCommit;++i)run.bytes[i]=std::uint8_t(rng());
    for(std::size_t i=0;i<0x1000;++i)run.bytes[GlobalPage+i]=std::uint8_t(rng());
    run.Write32(Global,run.c.null_manager?0:Manager);
    run.Write32(Manager,VTable);run.Write32(VTable+8,ResizeMethod);
    run.Write32(Array,run.c.null_data?0:Data);
    run.Write32(Array+4,run.c.length);run.Write32(Array+8,run.c.capacity);
}
void Compare(const Case& c,unsigned ordinal){
    GuestWindow a,b;Run original{a.base,c},recovered{b.base,c};
    Setup(original);std::memcpy(b.base,a.base,LowCommit);
    std::memcpy(b.base+GlobalPage,a.base+GlobalPage,0x1000);
    PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;
    std::array<PPCRegister*,5> regs={&ctx.r27,&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
    for(unsigned i=0;i<regs.size();++i)regs[i]->u64=0x1122334455667700ull+i;
    ctx.r3.u64=Array;
    if(c.kind==Kind::Resize){ctx.r4.u64=c.stride;ctx.r5.u64=c.argument;}
    else {ctx.r4.u64=c.first;ctx.r5.u64=c.count;ctx.r6.u64=c.stride;ctx.r7.u64=c.argument;}
    active=&original;
    try{
        if(c.kind==Kind::Resize)oracle_ResizeArray(ctx,a.base);
        else oracle_RemoveArrayRange(ctx,a.base);
        active=nullptr;
        Require(ctx.r1.u64==StackTop&&ctx.lr==CallerLR,"original r1/LR restored");
        for(unsigned i=0;i<regs.size();++i)
            Require(regs[i]->u64==0x1122334455667700ull+i,"original nonvolatile GPR restored");
        GuestMemory guest(0,{b.base,GuestSpace});Services services(recovered);
        if(c.kind==Kind::Resize)ResizeArray(guest,services,Array,c.stride,c.argument);
        else RemoveArrayRange(guest,services,Array,c.first,c.count,c.stride,
                              c.argument,StackTop-128);
        const bool nested_resize=std::ranges::any_of(original.events,
            [](const Event& event){return event.kind=='R';});
        if(c.kind==Kind::Remove&&nested_resize)
            Require(original.Read32(StackTop-128-8)==0x82298b9cu,
                    "nested original Resize ABI helper overwrites Move spill high word");
        const std::size_t spill_bytes=nested_resize?4:8;
        const GuestAddress spill=StackTop-128-8+(nested_resize?4:0);
        if(original.events!=recovered.events||original.snapshots!=recovered.snapshots||
           std::memcmp(a.base,b.base,LowCompare)!=0||
           (c.kind==Kind::Remove &&
            std::memcmp(a.base+spill,b.base+spill,spill_bytes)!=0)||
           std::memcmp(a.base+GlobalPage,b.base+GlobalPage,0x1000)!=0){
            std::size_t first=0;while(first<LowCompare&&a.base[first]==b.base[first])++first;
            std::fprintf(stderr,"case %u %s kind=%u return_r3=%08x events=%zu/%zu first=%08zx\n",
                ordinal,c.name,unsigned(c.kind),ctx.r3.u32,original.events.size(),
                recovered.events.size(),first);
            if(c.kind==Kind::Remove){
                std::fprintf(stderr,"spill original/recovered:");
                for(unsigned k=0;k<8;++k)
                    std::fprintf(stderr," %02x/%02x",a.base[StackTop-136+k],b.base[StackTop-136+k]);
                std::fprintf(stderr,"\n");
            }
            for(std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if(!(original.events[i]==recovered.events[i])){
                    auto& x=original.events[i];auto& y=recovered.events[i];
                    std::fprintf(stderr,"event %zu %c/%c args %08x/%08x %08x/%08x %08x/%08x %08x/%08x %08x/%08x\n",
                        i,x.kind,y.kind,x.args[0],y.args[0],x.args[1],y.args[1],
                        x.args[2],y.args[2],x.args[3],y.args[3],x.args[4],y.args[4]);
                }
            throw std::runtime_error("PPC/recovered array mismatch");
        }
    }catch(const std::exception& error){
        active=nullptr;std::fprintf(stderr,"case %u %s: %s\n",ordinal,c.name,error.what());throw;
    }
}
} // namespace

PPC_FUNC(sub_827C5F38){active->Initialize();}
PPC_FUNC(sub_8229F678){oracle_ResizeArray(ctx,base);}
PPC_FUNC(sub_82B7C470){oracle_MoveGuestMemory(ctx,base);}
PPC_FUNC(sub_82B7A0B0){oracle_CopyGuestMemory(ctx,base);}
void oracle_ResizeIndirect(PPCContext& ctx,std::uint8_t*,std::uint32_t target){
    ctx.r3.u64=active->Resize(target,ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32);
}

int main(){
    try{
        std::vector<Case> cases;
        auto add=[&](Case c,const char* name){c.name=name;c.seed+=std::uint32_t(cases.size())*0x9e37u;cases.push_back(c);};
        Case c;c.kind=Kind::Resize;c.length=0;c.capacity=0;c.null_data=true;add(c,"empty array no resize");
        c.capacity=1;add(c,"null storage nonzero capacity");
        c.null_data=false;c.capacity=0;add(c,"existing storage zero capacity");
        c.capacity=8;c.stride=8;add(c,"resize existing storage");
        c.null_manager=true;add(c,"initialize missing manager");
        c.null_result=true;add(c,"resize callback returns null");
        c={};add(c,"remove middle overlapping bytes");
        for(auto stride:{1u,2u,4u,8u,16u,31u})
            for(auto first:{0u,1u,3u,7u}){
                c={};c.stride=stride;c.first=first;c.count=(std::min)(2u,8u-first);
                add(c,"remove with element stride and boundary index");
            }
        for(auto count:{0u,1u,4u,8u}){
            c={};c.first=0;c.count=count;add(c,"remove zero and full range");
        }
        for(auto capacity:{8u,12u,24u,64u,2048u,4096u}){
            c={};c.capacity=capacity;c.stride=8;add(c,"capacity shrink threshold");
        }
        c={};c.null_result=true;add(c,"shrink callback returns null");
        c={};c.null_manager=true;c.capacity=64;add(c,"shrink initializes manager");
        c={};c.mutate_callback=true;c.capacity=64;add(c,"callback mutates array count");
        std::array<unsigned,2> counts{};
        for(unsigned i=0;i<cases.size();++i){Compare(cases[i],i);++counts[unsigned(cases[i].kind)];}
        std::printf("PASS 8229F678 %u\nPASS 82298AF8 %u\n",counts[0],counts[1]);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
