// Appended after verbatim original PPC bodies and actual ABI helper bodies.
#include "lo_semantics/heap_segment.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize=0x400000,ScratchStart=0x3e0000;
constexpr GuestAddress Heap=0x10000,Segment=0x30000,NodeA=0x40000,NodeB=0x40100;
constexpr GuestAddress BytesAddress=0x50000,Block=0x100000,RangeStart=0x120000;
constexpr GuestAddress SegmentBase=0x200000,StackTop=0x3f0000;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool okay,const char* reason){if(!okay)throw std::runtime_error(reason);}
enum class Kind{Extend,Initialize};
struct Case {
    Kind kind=Kind::Extend;
    std::string name,expected_trace;
    std::uint32_t request_bytes=0x10000,node_size=0x10000;
    bool has_node=true,second_node=false,use_indirect=false;
    bool select_second=false,scan_previous=false;
    GuestAddress callback_target=0x82345678u;
    GuestAddress required_start=0,committed_base=RangeStart;
    std::int32_t commit_status=0;
    bool commit_changes_bytes=false,commit_changes_base=false;
    std::uint32_t slot=0,flags=0;
    GuestAddress range_begin=SegmentBase,committed_end=SegmentBase+0x10000;
    GuestAddress reserved_end=SegmentBase+0x30000;
    bool range_changes_committed=false,vm_changes_size=false;
};
struct Event{
    std::string name;
    std::array<std::uint32_t,7> args{};
    bool operator==(const Event&) const=default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& scenario;
    GuestAddress frame;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress a) const{
        return (std::uint32_t(bytes[a])<<24)|(std::uint32_t(bytes[a+1])<<16)|
               (std::uint32_t(bytes[a+2])<<8)|bytes[a+3];
    }
    void Write16(GuestAddress a,std::uint16_t value){
        bytes[a]=std::uint8_t(value>>8);bytes[a+1]=std::uint8_t(value);
    }
    void Write32(GuestAddress a,std::uint32_t value){
        bytes[a]=std::uint8_t(value>>24);bytes[a+1]=std::uint8_t(value>>16);
        bytes[a+2]=std::uint8_t(value>>8);bytes[a+3]=std::uint8_t(value);
    }
    void Snapshot(){
        snapshots.emplace_back(bytes,bytes+ScratchStart);
        snapshots.back().insert(snapshots.back().end(),bytes+frame+80,bytes+frame+84);
        if(scenario.kind==Kind::Initialize)
            snapshots.back().insert(snapshots.back().end(),bytes+frame+236,bytes+frame+240);
        const auto lr=frame+(scenario.kind==Kind::Extend?152:168);
        snapshots.back().insert(snapshots.back().end(),bytes+lr,bytes+lr+4);
    }
    void Begin(const char* name,std::array<std::uint32_t,7> args){
        Snapshot();events.push_back({name,args});
    }
    void End(){Snapshot();}
    std::int32_t Commit(GuestAddress target,GuestAddress heap,
        GuestAddress base_inout,GuestAddress bytes_inout){
        Begin("indirect",{target,heap,base_inout,bytes_inout,0,0,0});
        if(scenario.commit_status>=0){
            const auto base=scenario.select_second?scenario.committed_base+0x20000:scenario.committed_base;
            Write32(base_inout,scenario.commit_changes_base?base+0x10000:base);
            if(scenario.commit_changes_bytes)Write32(bytes_inout,scenario.request_bytes/2);
        }
        End();return scenario.commit_status;
    }
    std::int32_t VM(GuestAddress base_inout,GuestAddress size_inout,
        std::uint32_t type,std::uint32_t protect,std::uint32_t zero){
        Begin("vm",{base_inout,size_inout,type,protect,zero,0,0});
        if(scenario.commit_status>=0){
            if(scenario.kind==Kind::Extend){
                const auto base=scenario.select_second?scenario.committed_base+0x20000:scenario.committed_base;
                Write32(base_inout,scenario.commit_changes_base?base+0x10000:base);
                if(scenario.commit_changes_bytes)Write32(size_inout,scenario.request_bytes/2);
            } else if(scenario.vm_changes_size){
                Write32(size_inout,0x10000);
            }
        }
        End();return scenario.commit_status;
    }
    void Range(GuestAddress segment,GuestAddress committed_end,std::uint32_t bytes_count){
        Begin("range",{segment,committed_end,bytes_count,0,0,0,0});
        if(scenario.range_changes_committed)Write32(frame+236,committed_end+0x1000);
        End();
    }
};
Run* active=nullptr;
struct Services final:HeapSegmentServices{
    Run& run;explicit Services(Run& value):run(value){}
    std::int32_t CommitRange(GuestAddress target,GuestAddress heap,
        GuestAddress base_inout,GuestAddress bytes_inout) override{
        return run.Commit(target,heap,base_inout,bytes_inout);
    }
    std::int32_t AllocateVirtualMemory(GuestAddress base_inout,GuestAddress size_inout,
        std::uint32_t type,std::uint32_t protect,std::uint32_t zero) override{
        return run.VM(base_inout,size_inout,type,protect,zero);
    }
    void InitializeUncommittedRange(GuestAddress segment,GuestAddress committed_end,
        std::uint32_t bytes_count) override{run.Range(segment,committed_end,bytes_count);}
};
void Header(Run& run,GuestAddress block,std::uint16_t units,std::uint8_t flags){
    run.Write16(block,units);run.Write16(block+2,0);
    run.bytes[block+4]=0;run.bytes[block+5]=flags;
}
void Setup(Run& run){
    const Case& c=run.scenario;
    std::memset(run.bytes,0xa5,MemorySize);
    for(unsigned size=0;size<128;++size){
        const auto head=Heap+(size+48)*8;
        run.Write32(head,head);run.Write32(head+4,head);
    }
    for(unsigned word=0;word<4;++word)run.Write32(Heap+(88+word)*4,0);
    run.Write32(Heap+48,0);
    run.Write32(run.frame+(c.kind==Kind::Extend?152:168),CallerLR);
    if(c.kind==Kind::Initialize){
        run.Write16(SegmentBase,0);
        return;
    }
    run.Write32(BytesAddress,c.request_bytes);
    run.Write32(Heap+1412,c.use_indirect?c.callback_target:0);
    run.Write32(Heap+76,0);
    run.Write32(Segment+24,Heap);
    run.Write32(Segment+28,c.node_size);
    run.Write32(Segment+40,Block);
    run.Write32(Segment+44,c.committed_base+(c.select_second?0x20000:0)+c.node_size);
    run.Write32(Segment+48,0x1000);
    run.Write32(Segment+52,c.has_node?(c.second_node?2u:1u):0u);
    run.Write32(Segment+56,c.has_node?NodeA:0);
    run.Write32(Segment+64,Block);
    const auto chosen_base=c.committed_base+(c.select_second?0x20000:0);
    Header(run,Block,static_cast<std::uint16_t>((chosen_base-Block)>>4),0x10);
    if(c.scan_previous){
        Header(run,Block,0x1000,0);
        Header(run,Block+0x10000,0x1000,0);
        run.Write16(c.committed_base,0);
    }
    if(c.has_node){
        run.Write32(NodeA,c.second_node?NodeB:0);
        run.Write32(NodeA+4,c.committed_base);
        run.Write32(NodeA+8,c.select_second?0x8000:c.node_size);
        if(c.second_node){
            run.Write32(NodeB,0);
            run.Write32(NodeB+4,c.committed_base+0x20000);
            run.Write32(NodeB+8,c.node_size);
        }
    }
}
std::string Trace(const Run& run){
    std::string result;
    for(const auto& event:run.events)
        result+=(event.name=="indirect"?'I':event.name=="vm"?'V':'R');
    return result;
}
void Compare(const Case& c,unsigned ordinal){
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a&&b,"allocate independent guest windows");
    try{
        const GuestAddress frame=StackTop-(c.kind==Kind::Extend?160:176);
        Run original{a,c,frame},recovered{b,c,frame};Setup(original);std::memcpy(b,a,MemorySize);
        PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;
        constexpr std::array<std::uint64_t,10> saved={
            0x1122334455667788ull,0x2233445566778899ull,0x33445566778899aaull,
            0x445566778899aabbull,0x5566778899aabbccull,0x66778899aabbccddull,
            0x778899aabbccddeeull,0x8899aabbccddeeffull,0x99aabbccddeeff00ull,
            0xaabbccddeeff0011ull};
        ctx.r22.u64=saved[0];ctx.r23.u64=saved[1];ctx.r24.u64=saved[2];
        ctx.r25.u64=saved[3];ctx.r26.u64=saved[4];ctx.r27.u64=saved[5];
        ctx.r28.u64=saved[6];ctx.r29.u64=saved[7];ctx.r30.u64=saved[8];ctx.r31.u64=saved[9];
        ctx.r3.u64=Heap;
        if(c.kind==Kind::Extend){
            ctx.r4.u64=Segment;ctx.r5.u64=BytesAddress;ctx.r6.u64=c.required_start;
        } else {
            ctx.r4.u64=SegmentBase;ctx.r5.u64=c.slot;ctx.r6.u64=c.flags;
            ctx.r7.u64=c.range_begin;ctx.r8.u64=c.committed_end;ctx.r9.u64=c.reserved_end;
        }
        active=&original;
        if(c.kind==Kind::Extend)oracle_ExtendHeapSegment(ctx,a);
        else oracle_InitializeHeapSegment(ctx,a);
        active=nullptr;
        Require(ctx.r1.u64==StackTop && ctx.lr==CallerLR,"original PPC stack/LR restored");
        Require(ctx.r22.u64==saved[0]&&ctx.r23.u64==saved[1]&&ctx.r24.u64==saved[2]&&
                ctx.r25.u64==saved[3]&&ctx.r26.u64==saved[4]&&ctx.r27.u64==saved[5]&&
                ctx.r28.u64==saved[6]&&ctx.r29.u64==saved[7]&&ctx.r30.u64==saved[8]&&
                ctx.r31.u64==saved[9],"original PPC nonvolatile registers restored");
        GuestMemory memory(0,{b,MemorySize});Services services(recovered);
        const auto result=c.kind==Kind::Extend?
            ExtendHeapSegment(memory,services,Heap,Segment,BytesAddress,c.required_start,frame):
            InitializeHeapSegment(memory,services,Heap,SegmentBase,c.slot,c.flags,
                                  c.range_begin,c.committed_end,c.reserved_end,frame);
        const bool same=ctx.r3.u32==result&&original.events==recovered.events&&
            original.snapshots==recovered.snapshots&&std::equal(a,a+ScratchStart,b)&&
            std::equal(a+frame+80,a+frame+84,b+frame+80)&&
            std::equal(a+frame+(c.kind==Kind::Extend?152:168),
                       a+frame+(c.kind==Kind::Extend?156:172),
                       b+frame+(c.kind==Kind::Extend?152:168))&&
            (c.kind==Kind::Extend||std::equal(a+frame+236,a+frame+240,b+frame+236));
        const auto trace=Trace(original);
        if(!same||trace!=c.expected_trace){
            GuestAddress first=0;while(first<ScratchStart&&a[first]==b[first])++first;
            std::fprintf(stderr,"FAIL %s #%u %s result=%08x/%08x trace=%s/%s events=%zu/%zu first=%08x\n",
                         c.kind==Kind::Extend?"extend":"initialize",ordinal,c.name.c_str(),
                         ctx.r3.u32,result,trace.c_str(),c.expected_trace.c_str(),
                         original.events.size(),recovered.events.size(),first);
            for(std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if(!(original.events[i]==recovered.events[i])){
                    auto& x=original.events[i];auto& y=recovered.events[i];
                    std::fprintf(stderr,"event %zu %s/%s args %08x,%08x,%08x,%08x,%08x,%08x,%08x / %08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
                        i,x.name.c_str(),y.name.c_str(),x.args[0],x.args[1],x.args[2],x.args[3],x.args[4],x.args[5],x.args[6],
                        y.args[0],y.args[1],y.args[2],y.args[3],y.args[4],y.args[5],y.args[6]);
                }
            std::fprintf(stderr,"frame+80=%08x/%08x caller+236=%08x/%08x\n",
                         original.Read32(frame+80),recovered.Read32(frame+80),
                         original.Read32(frame+236),recovered.Read32(frame+236));
            throw std::runtime_error("PPC/recovered heap segment differs");
        }
    }catch(...){active=nullptr;VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);throw;}
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

void oracle_CommitIndirect(PPCContext& ctx,std::uint8_t*,std::uint32_t target){
    ctx.r3.s64=active->Commit(target,ctx.r3.u32,ctx.r4.u32,ctx.r5.u32);
}
PPC_FUNC(__imp__NtAllocateVirtualMemory){
    ctx.r3.s64=active->VM(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32);
}
PPC_FUNC(sub_827CB658){active->Range(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32);}
PPC_FUNC(sub_827CBA60){oracle_InsertFreeBlocks(ctx,base);}

int main(){
    try{
        unsigned extend_cases=0,init_cases=0;
        auto test=[&](Case c){Compare(c,c.kind==Kind::Extend?extend_cases++:init_cases++);};
        Case c;c.name="empty range list";c.has_node=false;c.expected_trace="";test(c);
        c={};c.name="node smaller than request";c.node_size=0x8000;c.expected_trace="";test(c);
        c={};c.name="required start mismatch";c.required_start=RangeStart+0x10;
        c.expected_trace="";test(c);
        c={};c.name="required start matches range";c.required_start=RangeStart;
        c.expected_trace="V";test(c);
        c={};c.name="default kernel full node consume";c.expected_trace="V";test(c);
        c={};c.name="default kernel partial node consume";c.node_size=0x20000;
        c.expected_trace="V";test(c);
        c={};c.name="select second range after first too small";
        c.second_node=true;c.select_second=true;c.expected_trace="V";test(c);
        c={};c.name="scan previous block before new range";
        c.scan_previous=true;c.expected_trace="V";test(c);
        c={};c.name="kernel negative status";c.commit_status=-1;c.expected_trace="V";test(c);
        c={};c.name="kernel positive high status";
        c.commit_status=(std::numeric_limits<std::int32_t>::max)();
        c.expected_trace="V";test(c);
        c={};c.name="indirect commit success";c.use_indirect=true;c.expected_trace="I";test(c);
        c.callback_target=0x8234567bu;c.name="indirect tagged callback target";test(c);
        c={};c.name="indirect commit failure";c.use_indirect=true;c.commit_status=-1;
        c.expected_trace="I";test(c);
        c={};c.name="commit modifies requested byte count";c.commit_changes_bytes=true;
        c.node_size=0x20000;c.expected_trace="V";test(c);
        c={};c.name="kernel commit changes output base";c.commit_changes_base=true;
        c.expected_trace="V";test(c);
        c.use_indirect=true;c.name="indirect commit changes output base";
        c.expected_trace="I";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize with uncommitted tail";
        c.expected_trace="R";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize no uncommitted tail";
        c.reserved_end=c.committed_end;c.expected_trace="";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize slot two flags A5";
        c.slot=2;c.flags=0xa5;c.expected_trace="R";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize range begins after header";
        c.range_begin=SegmentBase+0x1000;c.expected_trace="R";test(c);
        c={};c.kind=Kind::Initialize;c.name="unaligned reserved tail rounds pages";
        c.reserved_end=SegmentBase+0x21001;c.expected_trace="R";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize insufficient total span";
        c.committed_end=SegmentBase+0x20;c.reserved_end=SegmentBase+0x20;
        c.expected_trace="";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize extra commit";
        c.committed_end=SegmentBase;c.expected_trace="VR";test(c);
        c.vm_changes_size=true;c.name="extra commit updates size output";test(c);
        c={};c.kind=Kind::Initialize;c.name="initialize extra commit fails";
        c.committed_end=SegmentBase;c.commit_status=-1;c.expected_trace="V";test(c);
        c={};c.kind=Kind::Initialize;c.name="range callback updates committed end";
        c.range_changes_committed=true;c.expected_trace="R";test(c);
        std::printf("PASS 827CB778 %u\n",extend_cases);
        std::printf("PASS 827CC2C0 %u\n",init_cases);
        std::puts("LIMIT: synthetic commit/VM/range callbacks; bounded ordinary guest heap; no runtime/MMIO/concurrency");
        return 0;
    }catch(const std::exception& error){
        active=nullptr;std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;
    }
}
