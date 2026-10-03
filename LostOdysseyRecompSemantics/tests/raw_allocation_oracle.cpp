// Appended after the original 823ACBD0/823ACC98 PPC bodies and ABI helpers.
#include "lo_semantics/raw_allocation.h"
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
constexpr GuestAddress StackTop=0x3f000,Heap=0x10000;
constexpr GuestAddress HeapGlobal=0x83245708,RetryGlobal=0x832d3aec;
constexpr GuestAddress HeapPage=HeapGlobal&~0xfffu,RetryPage=RetryGlobal&~0xfffu;
constexpr GuestAddress ErrorA=0x20000,ErrorB=0x20100;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
struct Case{
    const char* name="";std::uint64_t requested=0x100;
    std::uint64_t first_result=0x1122334400012345ull,second_result=0;
    bool missing_heap=false,restore_heap=false,retry_enabled=false;
    bool change_error_address=false,callback_changes_flag=false;
    std::int32_t retry_status=0;
    std::string expected_trace="A";
    std::uint32_t seed=0x4ec0;
};
struct Event{
    char name;std::array<std::uint64_t,4> args{};
    bool operator==(const Event&)const=default;
};
struct Run{
    std::uint8_t* bytes;const Case& c;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    unsigned allocations=0,error_lookups=0,retries=0;
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
        snapshot.insert(snapshot.end(),bytes+HeapPage,bytes+HeapPage+0x1000);
        snapshot.insert(snapshot.end(),bytes+RetryPage,bytes+RetryPage+0x1000);
    }
    void Record(char name,std::array<std::uint64_t,4> args){
        Snapshot();events.push_back({name,args});Snapshot();
    }
    std::uint64_t Allocate(GuestAddress heap,std::uint32_t flags,std::uint64_t size){
        const auto result=allocations++==0?c.first_result:c.second_result;
        Snapshot();events.push_back({'A',{heap,flags,size,result}});
        if(c.callback_changes_flag)Write32(RetryGlobal,0);
        Snapshot();return result;
    }
    void Missing(){
        Snapshot();events.push_back({'M',{}});
        if(c.restore_heap)Write32(HeapGlobal,Heap);
        Snapshot();
    }
    void Report(std::uint32_t code){Record('P',{code,0,0,0});}
    void Terminate(std::uint32_t code){Record('T',{code,0,0,0});}
    std::int32_t Retry(std::uint64_t size){
        const auto result=retries++==0?c.retry_status:0;
        Record('R',{size,std::uint32_t(result),0,0});return result;
    }
    GuestAddress Error(){
        const GuestAddress address=c.change_error_address&&error_lookups++>0?ErrorB:ErrorA;
        Record('E',{address,0,0,0});return address;
    }
};
Run* active=nullptr;
struct Services final:RawAllocationServices{
    Run& run;explicit Services(Run& value):run(value){}
    std::uint64_t AllocateHeap(GuestAddress heap,std::uint32_t flags,
                               std::uint64_t bytes)override{
        return run.Allocate(heap,flags,bytes);
    }
    void EnterMissingHeapPath()override{run.Missing();}
    void ReportMissingHeap(std::uint32_t code)override{run.Report(code);}
    void TerminateMissingHeap(std::uint32_t code)override{run.Terminate(code);}
    std::int32_t RetryAllocation(std::uint64_t bytes)override{return run.Retry(bytes);}
    GuestAddress GetErrorAddress()override{return run.Error();}
};
struct GuestWindow{
    std::uint8_t* base;
    GuestWindow(){
        base=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,GuestSpace,MEM_RESERVE,PAGE_NOACCESS));
        Require(base!=nullptr,"reserve sparse 4 GiB guest space");
        Require(VirtualAlloc(base,LowCommit,MEM_COMMIT,PAGE_READWRITE)==base,"commit low guest/stack");
        Require(VirtualAlloc(base+HeapPage,0x1000,MEM_COMMIT,PAGE_READWRITE)==base+HeapPage,
                "commit original process heap global page");
        Require(VirtualAlloc(base+RetryPage,0x1000,MEM_COMMIT,PAGE_READWRITE)==base+RetryPage,
                "commit original retry policy global page");
    }
    ~GuestWindow(){if(base)VirtualFree(base,0,MEM_RELEASE);}
    GuestWindow(const GuestWindow&)=delete;GuestWindow& operator=(const GuestWindow&)=delete;
};
void Setup(Run& run){
    std::mt19937 rng(run.c.seed);
    for(std::size_t i=0;i<LowCommit;++i)run.bytes[i]=std::uint8_t(rng());
    for(std::size_t i=0;i<0x1000;++i){
        run.bytes[HeapPage+i]=std::uint8_t(rng());
        run.bytes[RetryPage+i]=std::uint8_t(rng());
    }
    run.Write32(HeapGlobal,run.c.missing_heap?0:Heap);
    run.Write32(RetryGlobal,run.c.retry_enabled?1:0);
    run.Write32(ErrorA,0xfeedcafe);run.Write32(ErrorB,0xdeadbeef);
}
std::string Trace(const Run& run){
    std::string result;for(const auto& event:run.events)result+=event.name;return result;
}
void Compare(const Case& c,unsigned ordinal){
    GuestWindow a,b;Run original{a.base,c},recovered{b.base,c};
    Setup(original);std::memcpy(b.base,a.base,LowCommit);
    std::memcpy(b.base+HeapPage,a.base+HeapPage,0x1000);
    std::memcpy(b.base+RetryPage,a.base+RetryPage,0x1000);
    PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;ctx.r3.u64=c.requested;
    std::array<PPCRegister*,4> regs={&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
    for(unsigned i=0;i<regs.size();++i)regs[i]->u64=0x1122334455667700ull+i;
    try{
        active=&original;oracle_AllocateRawMemory(ctx,a.base);active=nullptr;
        Require(ctx.r1.u64==StackTop&&ctx.lr==CallerLR,"original r1/LR restored");
        for(unsigned i=0;i<regs.size();++i)
            Require(regs[i]->u64==0x1122334455667700ull+i,"original nonvolatile GPRs");
        Require(Trace(original)==c.expected_trace,"expected original PPC callback path");
        GuestMemory guest(0,{b.base,GuestSpace});Services services(recovered);
        const auto result=AllocateRawMemory(guest,services,c.requested);
        if(ctx.r3.u64!=result||original.events!=recovered.events||
           original.snapshots!=recovered.snapshots||
           std::memcmp(a.base,b.base,LowCompare)!=0||
           std::memcmp(a.base+HeapPage,b.base+HeapPage,0x1000)!=0||
           std::memcmp(a.base+RetryPage,b.base+RetryPage,0x1000)!=0){
            std::size_t first=0;while(first<LowCompare&&a.base[first]==b.base[first])++first;
            std::fprintf(stderr,"case %u %s return=%016llx/%016llx trace=%s/%s events=%zu/%zu first=%08zx\n",
                ordinal,c.name,(unsigned long long)ctx.r3.u64,(unsigned long long)result,
                Trace(original).c_str(),Trace(recovered).c_str(),
                original.events.size(),recovered.events.size(),first);
            throw std::runtime_error("PPC/recovered raw allocation mismatch");
        }
    }catch(const std::exception& error){
        active=nullptr;std::fprintf(stderr,"case %u %s: %s\n",ordinal,c.name,error.what());throw;
    }
}
} // namespace

PPC_FUNC(sub_823ACC98){oracle_GetProcessHeap(ctx,base);}
PPC_FUNC(sub_823ACCB0){ctx.r3.u64=active->Allocate(ctx.r3.u32,ctx.r4.u32,ctx.r5.u64);}
PPC_FUNC(sub_82B7FCE0){active->Missing();}
PPC_FUNC(sub_82B7FC98){active->Report(ctx.r3.u32);}
PPC_FUNC(sub_82B7BF20){active->Terminate(ctx.r3.u32);}
PPC_FUNC(sub_82B7FE68){ctx.r3.s64=active->Retry(ctx.r3.u64);}
PPC_FUNC(sub_82B7FD78){ctx.r3.u64=active->Error();}

int main(){
    try{
        std::vector<Case> cases;
        auto add=[&](Case c,const char* name){c.name=name;c.seed+=std::uint32_t(cases.size())*0x9e37u;cases.push_back(c);};
        Case c;add(c,"ordinary allocation success");
        c.requested=0;c.expected_trace="A";add(c,"zero normalized to one");
        c.requested=0x100000000ull;c.expected_trace="A";add(c,"high size bits and zero low normalized");
        c.requested=0x1234567800000100ull;add(c,"high size bits forwarded");
        c.requested=0xfffff000u;add(c,"maximum accepted low32 size");
        c.requested=0xabcdef01fffff000ull;add(c,"maximum accepted with high bits");
        for(auto requested:{0xfffff001ull,0xffffffffull,0xabcdef01fffff001ull}){
            c={};c.requested=requested;c.expected_trace="RE";add(c,"oversize reports out-of-memory");
        }
        c={};c.missing_heap=true;c.restore_heap=true;c.expected_trace="MPTA";
        add(c,"missing heap callback restores heap before allocation");
        c.restore_heap=false;add(c,"missing heap callback returns without restore");
        c={};c.first_result=0;c.expected_trace="AEE";c.change_error_address=true;
        add(c,"allocation failure writes two dynamic errno addresses");
        c={};c.first_result=0x1234567800000000ull;c.expected_trace="AEE";
        add(c,"low32 zero return fails but preserves high64 result");
        c={};c.first_result=0;c.retry_enabled=true;c.retry_status=0;
        c.expected_trace="ARE";add(c,"retry flag enabled but callback rejects");
        c={};c.first_result=0;c.second_result=0x987654320000abcdull;
        c.retry_enabled=true;c.retry_status=1;c.expected_trace="ARA";
        add(c,"retry succeeds and second allocation returns full64");
        c.second_result=0;c.expected_trace="ARARE";add(c,"second failure then retry rejects");
        c={};c.first_result=0;c.callback_changes_flag=true;c.expected_trace="AEE";
        add(c,"allocator callback changes retry flag before failure decision");
        for(unsigned i=0;i<16;++i){
            c={};c.requested=(std::uint64_t{0x13572468u+i}<<32)|(i*0x101u);
            c.first_result=std::uint64_t{0x2468ace0u+i}<<32|0x11000u+i;
            add(c,"seeded bounded high64 allocation path");
        }
        for(unsigned i=0;i<cases.size();++i)Compare(cases[i],i);
        std::printf("PASS 823ACBD0 %zu\n",cases.size());return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
