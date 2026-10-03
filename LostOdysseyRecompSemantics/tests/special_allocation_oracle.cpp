// Appended after pinned original PPC bodies and actual ABI helpers.
#include "lo_semantics/special_allocation.h"
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
constexpr GuestAddress StackTop=0x3f000,Allocator=0x10000;
constexpr GuestAddress ActiveData=0x12000,FreeData=0x16000;
constexpr GuestAddress ActiveReplacement=0x20000,FreeReplacement=0x24000;
constexpr GuestAddress Manager=0x35000,VTable=0x35100;
constexpr GuestAddress Global=0x8330b608,GlobalPage=Global&~0xfffu;
constexpr GuestAddress ResizeMethod=0x8234567b,AllocateMethod=0x8234569d;
constexpr GuestAddress NewBlock=0x30000;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
enum class Kind{Allocate,Free};
struct Case{
    Kind kind=Kind::Allocate;const char* name="";
    std::uint32_t request=256,active_count=2,active_capacity=4;
    std::uint32_t free_count=2,free_capacity=4,seed=0xacc011;
    bool null_recycled=false,null_manager=false;
    bool null_allocate=false,null_resize=false,mutate_frame=false,mutate_count=false;
    std::uint32_t free_index=0;
};
struct Event{
    char kind;
    std::array<std::uint32_t,5> args{};
    std::array<std::uint32_t,6> before{},after{};
    bool operator==(const Event&)const=default;
};
struct Run{
    std::uint8_t* bytes;const Case& c;
    unsigned remove_calls=0,copy_calls=0;
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
    std::array<std::uint32_t,6> State()const{
        return {Read32(Allocator),Read32(Allocator+4),Read32(Allocator+8),
                Read32(Allocator+12),Read32(Allocator+16),Read32(Allocator+20)};
    }
    void Snapshot(){
        auto& snapshot=snapshots.emplace_back();
        snapshot.insert(snapshot.end(),bytes,bytes+LowCompare);
        snapshot.insert(snapshot.end(),bytes+GlobalPage,bytes+GlobalPage+0x1000);
    }
    void Begin(char kind,std::array<std::uint32_t,5> args){
        Snapshot();events.push_back({kind,args,State(),{}});
    }
    void End(){events.back().after=State();Snapshot();}
    void Initialize(){Begin('I',{});Write32(Global,Manager);End();}
    GuestAddress Allocate(GuestAddress method,GuestAddress manager,
                           std::uint32_t bytes,std::uint32_t flags){
        Begin('A',{method,manager,bytes,flags,0});
        Require(method==(AllocateMethod&~3u)&&manager==Manager,"actual allocate method/manager");
        End();return c.null_allocate?0:NewBlock;
    }
    GuestAddress Resize(GuestAddress method,GuestAddress manager,GuestAddress old,
                         std::uint32_t bytes_requested,std::uint32_t argument){
        Begin('R',{method,manager,old,bytes_requested,argument});
        Require(method==(ResizeMethod&~3u)&&manager==Manager,"actual resize method/manager");
        if(c.mutate_frame&&c.kind==Kind::Allocate){
            Write32(StackTop-176+80,NewBlock+0x40);
            Write32(StackTop-176+84,0x98765432);
        }
        if(c.mutate_count)Write32(Allocator+4,Read32(Allocator+4)+2);
        const bool active=old==ActiveData||old==ActiveReplacement;
        const auto replacement=active?ActiveReplacement:FreeReplacement;
        if(old!=0&&!c.null_resize&&bytes_requested!=0){
            const std::size_t previous=std::size_t(active?c.active_capacity:c.free_capacity)*8;
            const auto count=(std::min)(previous,std::size_t(bytes_requested));
            Require(count<=0x10000,"bounded synthetic allocator copy");
            std::memcpy(bytes+replacement,bytes+old,count);
        }
        End();return c.null_resize?0:replacement;
    }
};
Run* active=nullptr;
struct Services final:SpecialAllocationServices{
    Run& run;explicit Services(Run& value):run(value){}
    void InitializeManager()override{run.Initialize();}
    GuestAddress ResizeStorage(GuestAddress method,GuestAddress manager,
        GuestAddress old,std::uint32_t bytes,std::uint32_t argument)override{
        return run.Resize(method,manager,old,bytes,argument);
    }
    GuestAddress AllocateStorage(GuestAddress method,GuestAddress manager,
        std::uint32_t bytes,std::uint32_t flags)override{
        return run.Allocate(method,manager,bytes,flags);
    }
};
struct GuestWindow{
    std::uint8_t* base;
    GuestWindow(){
        base=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,GuestSpace,MEM_RESERVE,PAGE_NOACCESS));
        Require(base!=nullptr,"reserve sparse 4 GiB guest window");
        Require(VirtualAlloc(base,LowCommit,MEM_COMMIT,PAGE_READWRITE)==base,"commit low guest/stack");
        Require(VirtualAlloc(base+GlobalPage,0x1000,MEM_COMMIT,PAGE_READWRITE)==base+GlobalPage,
                "commit original global manager page");
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
    run.Write32(VTable+16,AllocateMethod);
    run.Write32(Allocator,ActiveData);run.Write32(Allocator+4,run.c.active_count);
    run.Write32(Allocator+8,run.c.active_capacity);
    run.Write32(Allocator+12,FreeData);run.Write32(Allocator+16,run.c.free_count);
    run.Write32(Allocator+20,run.c.free_capacity);
    run.Write32(Allocator+24,0xfffffffdu);run.Write32(Allocator+28,0xfffffffeu);
    for(unsigned i=0;i<run.c.active_count&&i<256;++i){
        run.Write32(ActiveData+i*8,0x28000+i*0x100);
        run.Write32(ActiveData+i*8+4,(i+1)*32);
    }
    for(unsigned i=0;i<run.c.free_count&&i<256;++i){
        run.Write32(FreeData+i*8,run.c.null_recycled&&i+1==run.c.free_count?0:
                    0x2c000+i*0x100);
        run.Write32(FreeData+i*8+4,(i+1)*128);
    }
}
void Compare(const Case& c,unsigned ordinal){
    GuestWindow a,b;Run original{a.base,c},recovered{b.base,c};
    Setup(original);std::memcpy(b.base,a.base,LowCommit);
    std::memcpy(b.base+GlobalPage,a.base+GlobalPage,0x1000);
    PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;
    std::array<PPCRegister*,10> regs={&ctx.r22,&ctx.r23,&ctx.r24,&ctx.r25,&ctx.r26,
        &ctx.r27,&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
    for(unsigned i=0;i<regs.size();++i)regs[i]->u64=0x1122334455667700ull+i;
    ctx.r3.u64=Allocator;ctx.r4.u64=c.kind==Kind::Allocate?c.request:
        (0x28000+c.free_index*0x100);
    try{
        active=&original;
        if(c.kind==Kind::Allocate)oracle_AllocateSpecialBlock(ctx,a.base);
        else oracle_FreeSpecialBlock(ctx,a.base);
        active=nullptr;
        Require(ctx.r1.u64==StackTop&&ctx.lr==CallerLR,"original r1/LR restored");
        for(unsigned i=0;i<regs.size();++i)
            Require(regs[i]->u64==0x1122334455667700ull+i,"original nonvolatile GPR restored");
        GuestMemory guest(0,{b.base,GuestSpace});Services services(recovered);
        if(c.kind==Kind::Allocate){
            const auto result=AllocateSpecialBlock(guest,services,Allocator,c.request,StackTop-176);
            Require(ctx.r3.u32==result,"allocated pointer return");
        }else FreeSpecialBlock(guest,services,Allocator,0x28000+c.free_index*0x100,
                              StackTop-160);
        bool same=original.events==recovered.events&&original.snapshots==recovered.snapshots&&
            std::memcmp(a.base,b.base,LowCompare)==0&&
            std::memcmp(a.base+GlobalPage,b.base+GlobalPage,0x1000)==0;
        if(c.kind==Kind::Allocate)
            same=same&&std::memcmp(a.base+StackTop-176+80,b.base+StackTop-176+80,8)==0;
        const bool expected_remove=c.kind==Kind::Free?
            (c.free_index<c.active_count):
            (c.request!=0&&c.request%128==0&&c.request<=c.free_count*128);
        Require(original.remove_calls==unsigned(expected_remove),
                "original special path composes array removal");
        if(expected_remove){
            Require(original.copy_calls==1,"original removal composes forward CopyGuestMemory");
            const GuestAddress remove_frame=StackTop-(c.kind==Kind::Allocate?176:160)-128;
            const GuestAddress spill=remove_frame-8;
            // ResizeArray's real __savegprlr_27 writes its LR over the high
            // half of this same guest slot after Move/Copy returns. Its low
            // half remains the Move/Copy destination in either path.
            same=same&&std::memcmp(a.base+spill+4,b.base+spill+4,4)==0;
            if(original.Read32(spill)==0x82298b9cu)
                Require(std::ranges::any_of(original.events,
                    [](const Event& event){return event.kind=='R';}),
                    "nested Resize helper overwrites Copy spill high word");
            else same=same&&std::memcmp(a.base+spill,b.base+spill,8)==0;
        }
        if(!same){
            std::size_t first=0;while(first<LowCompare&&a.base[first]==b.base[first])++first;
            std::fprintf(stderr,"case %u %s kind=%u return_r3=%08x events=%zu/%zu first=%08zx\n",
                ordinal,c.name,unsigned(c.kind),ctx.r3.u32,original.events.size(),
                recovered.events.size(),first);
            for(std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if(!(original.events[i]==recovered.events[i])){
                    auto& x=original.events[i];auto& y=recovered.events[i];
                    std::fprintf(stderr,"event %zu %c/%c args %08x/%08x %08x/%08x %08x/%08x %08x/%08x\n",
                        i,x.kind,y.kind,x.args[0],y.args[0],x.args[1],y.args[1],
                        x.args[2],y.args[2],x.args[3],y.args[3]);
                }
            throw std::runtime_error("PPC/recovered special allocation mismatch");
        }
    }catch(const std::exception& error){
        active=nullptr;std::fprintf(stderr,"case %u %s: %s\n",ordinal,c.name,error.what());throw;
    }
}
} // namespace

PPC_FUNC(sub_827C5F38){active->Initialize();}
PPC_FUNC(sub_82298AF8){++active->remove_calls;oracle_RemoveArrayRange(ctx,base);}
PPC_FUNC(sub_8229F678){oracle_ResizeArray(ctx,base);}
PPC_FUNC(sub_82B7C470){oracle_MoveGuestMemory(ctx,base);}
PPC_FUNC(sub_82B7A0B0){++active->copy_calls;oracle_CopyGuestMemory(ctx,base);}
void oracle_AllocationIndirect(PPCContext& ctx,std::uint8_t*,std::uint32_t target){
    if(target==(ResizeMethod&~3u))
        ctx.r3.u64=active->Resize(target,ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32);
    else if(target==(AllocateMethod&~3u))
        ctx.r3.u64=active->Allocate(target,ctx.r3.u32,ctx.r4.u32,ctx.r5.u32);
    else throw std::runtime_error("unexpected indirect manager method");
}

int main(){
    try{
        std::vector<Case> cases;
        auto add=[&](Case c,const char* name){c.name=name;c.seed+=std::uint32_t(cases.size())*0x9e37u;cases.push_back(c);};
        Case c;
        for(auto size:{0u,1u,127u,128u,129u,0x7fffffffu,0xffffffffu}){
            c={};c.free_count=0;c.request=size;add(c,"allocate fallback rounding boundary");
        }
        c={};add(c,"reuse last matching free entry");
        c.request=128;add(c,"reuse first matching free entry");
        c={};c.request=64;add(c,"no free entry matches");
        c={};c.null_recycled=true;add(c,"null recycled entry falls back to allocate");
        c={};c.active_count=1;c.active_capacity=1;add(c,"reuse grows active entry storage");
        c={};c.free_capacity=128;add(c,"reuse shrinks free entry storage");
        c={};c.free_count=0;c.null_manager=true;add(c,"allocate initializes manager");
        c={};c.free_count=0;c.null_allocate=true;add(c,"allocator returns null");
        c={};c.free_count=0;c.active_count=0;c.active_capacity=0;c.null_resize=true;
        add(c,"active resize returns null with zero insertion index");
        c={};c.free_count=0;c.active_count=0;c.active_capacity=0;c.mutate_frame=true;
        add(c,"resize mutates allocation frame locals");
        c={};c.free_count=0;c.active_count=0;c.active_capacity=0;c.mutate_count=true;
        add(c,"resize mutates active count");
        c={};c.kind=Kind::Free;c.active_count=0;add(c,"free empty active table");
        c={};c.kind=Kind::Free;c.free_index=10;add(c,"free absent address");
        c={};c.kind=Kind::Free;c.free_index=0;add(c,"free first active entry");
        c={};c.kind=Kind::Free;c.free_index=1;add(c,"free last active entry");
        c={};c.kind=Kind::Free;c.free_count=2;c.free_capacity=2;add(c,"free grows free entry storage");
        c={};c.kind=Kind::Free;c.active_capacity=128;add(c,"free shrinks active entry storage");
        c={};c.kind=Kind::Free;c.free_count=0;c.free_capacity=0;c.null_manager=true;
        add(c,"free initializes manager for new free table");
        c={};c.kind=Kind::Free;c.free_count=0;c.free_capacity=0;c.null_resize=true;
        add(c,"free table resize returns null");
        std::array<unsigned,2> counts{};
        for(unsigned i=0;i<cases.size();++i){Compare(cases[i],i);++counts[unsigned(cases[i].kind)];}
        std::printf("PASS 827C9A40 %u\nPASS 827C9C60 %u\n",counts[0],counts[1]);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
