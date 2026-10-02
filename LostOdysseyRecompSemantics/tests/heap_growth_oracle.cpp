// Appended after verbatim original PPC bodies and actual ABI helpers.
#include "lo_semantics/heap_growth.h"

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
constexpr GuestAddress Heap=0x10000,Segment0=0x30000,Segment1=0x31000;
constexpr GuestAddress Block=0x100000,VMBase=0x200000,GrowthBlock=0x220000;
constexpr GuestAddress StackTop=0x3f0000,Frame=StackTop-144;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool okay,const char* reason) {if(!okay)throw std::runtime_error(reason);}
struct Case {
    std::string name,expected_trace;
    std::uint32_t rounded=0x10000;
    std::uint32_t first_segment_pages=0,first_segment_limit=0;
    std::uint32_t second_segment_pages=0,second_segment_limit=0;
    int extend_success_slot=-1;
    bool extend_changes_size=false,all_slots_used=false,vm_enabled=false;
    bool init_success=true,free_changes_outputs=false;
    std::uint32_t preferred_reserve=0x80000,preferred_commit=0x30000;
    GuestAddress vm_base=VMBase;
    std::uint32_t reserve_failure_output_size=0,commit_output_size=0;
    GuestAddress init_result=1;
    unsigned reserve_failures=0;
    std::int32_t reserve_success_status=0,commit_status=0;
};
struct Event {
    std::string name;
    std::array<std::uint32_t,7> args{};
    bool operator==(const Event&) const=default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& scenario;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    unsigned reserve_attempts=0;
    std::uint32_t Read32(GuestAddress a) const {
        return (std::uint32_t(bytes[a])<<24)|(std::uint32_t(bytes[a+1])<<16)|
               (std::uint32_t(bytes[a+2])<<8)|bytes[a+3];
    }
    void Write16(GuestAddress a,std::uint16_t value) {
        bytes[a]=std::uint8_t(value>>8);bytes[a+1]=std::uint8_t(value);
    }
    void Write32(GuestAddress a,std::uint32_t value) {
        bytes[a]=std::uint8_t(value>>24);bytes[a+1]=std::uint8_t(value>>16);
        bytes[a+2]=std::uint8_t(value>>8);bytes[a+3]=std::uint8_t(value);
    }
    void Snapshot() {
        snapshots.emplace_back(bytes,bytes+ScratchStart);
        snapshots.back().insert(snapshots.back().end(),bytes+Frame+80,bytes+Frame+96);
        snapshots.back().insert(snapshots.back().end(),bytes+Frame+136,bytes+Frame+140);
    }
    void Begin(const char* name,std::array<std::uint32_t,7> args) {
        Snapshot();events.push_back({name,args});
    }
    void End() {Snapshot();}
    GuestAddress Extend(GuestAddress heap,GuestAddress segment,GuestAddress size_inout,
                        std::uint32_t zero) {
        Begin("extend",{heap,segment,size_inout,zero,0,0,0});
        const int slot=segment==Segment0?0:segment==Segment1?1:-1;
        Require(slot>=0,"known segment callback address");
        if (scenario.extend_changes_size)
            Write32(size_inout,Read32(size_inout)+0x10000);
        GuestAddress result=0;
        if (slot==scenario.extend_success_slot) {
            const std::uint32_t units=Read32(size_inout)>>4;
            Require(units!=0 && units<=0xffff,"bounded extender block units");
            Write16(Block,static_cast<std::uint16_t>(units));
            Write16(Block+2,0);
            bytes[Block+4]=static_cast<std::uint8_t>(slot);
            bytes[Block+5]=0x10;
            const auto descriptor=segment;
            Write32(descriptor+44,Block+(units<<4));
            Write32(descriptor+64,Block);
            result=Block;
        }
        End();return result;
    }
    std::int32_t VM(GuestAddress base_inout,GuestAddress size_inout,
                    std::uint32_t type,std::uint32_t protect,std::uint32_t zero) {
        const bool reserve=type==0x60002000;
        Require(reserve || type==0x60001000,"VM allocation type");
        Begin(reserve?"reserve":"commit",{base_inout,size_inout,type,protect,zero,0,0});
        std::int32_t status=0;
        if (reserve) {
            status=reserve_attempts++<scenario.reserve_failures ? -1 : scenario.reserve_success_status;
            if (status>=0)Write32(base_inout,scenario.vm_base);
            else if (scenario.reserve_failure_output_size)
                Write32(size_inout,scenario.reserve_failure_output_size);
        } else {
            status=scenario.commit_status;
            if (status>=0 && scenario.commit_output_size)
                Write32(size_inout,scenario.commit_output_size);
        }
        End();return status;
    }
    GuestAddress Initialize(GuestAddress heap,GuestAddress base,
        std::uint32_t slot_index,std::uint32_t zero,GuestAddress range_begin,
        GuestAddress committed_end,GuestAddress reserved_end) {
        Begin("init",{heap,base,slot_index,zero,range_begin,committed_end,reserved_end});
        if (scenario.init_success) {
            const std::uint32_t units=0x100;
            Write32(heap+(slot_index+24)*4,base);
            Write32(base+40,GrowthBlock);
            Write32(base+44,GrowthBlock+units*16);
            Write32(base+64,GrowthBlock);
            Write16(GrowthBlock,static_cast<std::uint16_t>(units));
            Write16(GrowthBlock+2,0);
            bytes[GrowthBlock+4]=0;bytes[GrowthBlock+5]=0x10;
            const auto head=heap+384,node=GrowthBlock+8;
            Require(Read32(head)==head,"VM init empty large list");
            Write32(head,node);Write32(head+4,node);
            Write32(node,head);Write32(node+4,head);
            Write32(heap+48,Read32(heap+48)+units);
        }
        End();return scenario.init_success?scenario.init_result:0;
    }
    std::int32_t Free(GuestAddress base_inout,GuestAddress size_inout,
                      std::uint32_t type,std::uint32_t zero) {
        Begin("free",{base_inout,size_inout,type,zero,0,0,0});
        if (scenario.free_changes_outputs) {
            Write32(base_inout,0);
            Write32(size_inout,0);
        }
        End();return 0;
    }
    std::uint32_t CompareMemoryUlong(GuestAddress source,std::uint32_t length,
                                     std::uint32_t value) {
        Begin("compare",{source,length,value,0,0,0,0});End();return 0;
    }
};
Run* active=nullptr;
struct Services final:HeapGrowthServices {
    Run& run;
    explicit Services(Run& value):run(value) {}
    std::uint32_t CompareMemoryUlong(GuestAddress source,std::uint32_t bytes,
        std::uint32_t value) override {return run.CompareMemoryUlong(source,bytes,value);}
    GuestAddress ExtendSegment(GuestAddress heap,GuestAddress segment,
        GuestAddress size_inout,std::uint32_t zero) override {
        return run.Extend(heap,segment,size_inout,zero);
    }
    std::int32_t AllocateVirtualMemory(GuestAddress base_inout,GuestAddress size_inout,
        std::uint32_t type,std::uint32_t protect,std::uint32_t zero) override {
        return run.VM(base_inout,size_inout,type,protect,zero);
    }
    GuestAddress InitializeSegment(GuestAddress heap,GuestAddress base,
        std::uint32_t slot_index,std::uint32_t zero,GuestAddress range_begin,
        GuestAddress committed_end,GuestAddress reserved_end) override {
        return run.Initialize(heap,base,slot_index,zero,range_begin,committed_end,reserved_end);
    }
    std::int32_t FreeVirtualMemory(GuestAddress base_inout,GuestAddress size_inout,
        std::uint32_t type,std::uint32_t zero) override {
        return run.Free(base_inout,size_inout,type,zero);
    }
};
void Setup(Run& run) {
    const Case& c=run.scenario;
    std::memset(run.bytes,0xa5,MemorySize);
    for (unsigned size=0;size<128;++size) {
        auto head=Heap+(size+48)*8;
        run.Write32(head,head);run.Write32(head+4,head);
    }
    for (unsigned i=0;i<4;++i)run.Write32(Heap+(88+i)*4,0);
    run.Write32(Heap+48,0);
    run.Write32(Heap+20,c.vm_enabled?2:0);
    run.Write32(Heap+32,c.preferred_reserve);
    run.Write32(Heap+36,c.preferred_commit);
    for (unsigned slot=0;slot<64;++slot)run.Write32(Heap+(slot+24)*4,0);
    if (c.first_segment_pages || c.all_slots_used) {
        run.Write32(Heap+96,Segment0);
        run.Write32(Segment0+48,c.first_segment_pages);
        run.Write32(Segment0+28,c.first_segment_limit);
    }
    if (c.second_segment_pages) {
        run.Write32(Heap+100,Segment1);
        run.Write32(Segment1+48,c.second_segment_pages);
        run.Write32(Segment1+28,c.second_segment_limit);
    }
    if (c.all_slots_used)
        for (unsigned slot=0;slot<64;++slot)
            run.Write32(Heap+(slot+24)*4,Segment0);
    run.Write32(Frame+136,CallerLR);
}
std::string Trace(const Run& run) {
    std::string value;
    for (const auto& event:run.events)
        value+=(event.name=="extend"?'E':event.name=="reserve"?'R':
                event.name=="commit"?'C':event.name=="init"?'I':
                event.name=="free"?'F':'D');
    return value;
}
void Compare(const Case& c,unsigned ordinal) {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a&&b,"allocate independent guest windows");
    try {
        Run original{a,c},recovered{b,c};Setup(original);std::memcpy(b,a,MemorySize);
        PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;
        ctx.r3.u64=Heap;ctx.r4.u64=c.rounded;
        constexpr std::array<std::uint64_t,5> saved={
            0x1122334455667788ull,0x2233445566778899ull,0x33445566778899aaull,
            0x445566778899aabbull,0x5566778899aabbccull};
        ctx.r27.u64=saved[0];ctx.r28.u64=saved[1];ctx.r29.u64=saved[2];
        ctx.r30.u64=saved[3];ctx.r31.u64=saved[4];
        active=&original;oracle_GrowHeap(ctx,a);active=nullptr;
        Require(ctx.r1.u64==StackTop && ctx.lr==CallerLR &&
                ctx.r27.u64==saved[0] && ctx.r28.u64==saved[1] &&
                ctx.r29.u64==saved[2] && ctx.r30.u64==saved[3] &&
                ctx.r31.u64==saved[4],"original PPC ABI restored");
        GuestMemory memory(0,{b,MemorySize});Services services(recovered);
        const auto result=GrowHeap(memory,services,Heap,c.rounded,Frame);
        if (c.commit_status<0 && c.preferred_reserve==0x80000 && c.rounded==0x10000)
            Require(original.Read32(Heap+32)==0x100000,
                    "PPC retains reserve accounting after commit failure");
        const bool same=ctx.r3.u32==result && original.events==recovered.events &&
            original.snapshots==recovered.snapshots &&
            std::equal(a,a+ScratchStart,b) &&
            std::equal(a+Frame+80,a+Frame+96,b+Frame+80) &&
            std::equal(a+Frame+136,a+Frame+140,b+Frame+136);
        const auto trace=Trace(original);
        if (!same || trace!=c.expected_trace) {
            GuestAddress first=0;
            while(first<ScratchStart&&a[first]==b[first])++first;
            std::fprintf(stderr,"FAIL #%u %s return=%08x/%08x trace=%s/%s events=%zu/%zu first=%08x\n",
                         ordinal,c.name.c_str(),ctx.r3.u32,result,trace.c_str(),
                         c.expected_trace.c_str(),original.events.size(),recovered.events.size(),first);
            for (std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if (!(original.events[i]==recovered.events[i])) {
                    auto& x=original.events[i];auto& y=recovered.events[i];
                    std::fprintf(stderr,"event %zu %s/%s args %08x,%08x,%08x,%08x,%08x,%08x,%08x / %08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
                         i,x.name.c_str(),y.name.c_str(),x.args[0],x.args[1],x.args[2],x.args[3],x.args[4],x.args[5],x.args[6],
                         y.args[0],y.args[1],y.args[2],y.args[3],y.args[4],y.args[5],y.args[6]);
                }
            for (unsigned offset=80;offset<96;offset+=4)
                if (original.Read32(Frame+offset)!=recovered.Read32(Frame+offset))
                    std::fprintf(stderr,"frame+%u %08x/%08x\n",offset,
                                 original.Read32(Frame+offset),recovered.Read32(Frame+offset));
            throw std::runtime_error("PPC/recovered heap growth differs");
        }
    } catch (...) {active=nullptr;VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);throw;}
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

PPC_FUNC(sub_823AE108) {oracle_CoalesceFreeBlocks(ctx,base);}
PPC_FUNC(sub_827CBA60) {oracle_InsertFreeBlocks(ctx,base);}
PPC_FUNC(sub_827CB778) {
    ctx.r3.u64=active->Extend(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32);
}
PPC_FUNC(__imp__NtAllocateVirtualMemory) {
    ctx.r3.s64=active->VM(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32);
}
PPC_FUNC(sub_827CC2C0) {
    ctx.r3.u64=active->Initialize(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,
                                  ctx.r7.u32,ctx.r8.u32,ctx.r9.u32);
}
PPC_FUNC(__imp__NtFreeVirtualMemory) {
    ctx.r3.s64=active->Free(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32);
}
PPC_FUNC(__imp__RtlCompareMemoryUlong) {
    ctx.r3.u64=active->CompareMemoryUlong(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32);
}

int main() {
    try {
        unsigned cases=0;
        auto test=[&](Case c){Compare(c,cases++);};
        Case c;c.name="no segments VM disabled";c.expected_trace="";test(c);
        c={};c.name="segment lacks pages";c.first_segment_pages=0;
        c.first_segment_limit=0x10000;c.expected_trace="";test(c);
        c={};c.name="segment lacks byte limit";c.first_segment_pages=1;
        c.first_segment_limit=0x8000;c.expected_trace="";test(c);
        c={};c.name="segment extender fails";c.first_segment_pages=1;
        c.first_segment_limit=0x10000;c.expected_trace="E";test(c);
        c={};c.name="segment extender succeeds and inserts";c.first_segment_pages=1;
        c.first_segment_limit=0x10000;c.extend_success_slot=0;c.expected_trace="E";test(c);
        c={};c.name="segment extender changes size before coalesce";c.first_segment_pages=2;
        c.first_segment_limit=0x20000;c.extend_success_slot=0;c.extend_changes_size=true;
        c.expected_trace="E";test(c);
        c={};c.name="second segment after first extender fails";c.first_segment_pages=1;
        c.first_segment_limit=0x10000;c.second_segment_pages=1;
        c.second_segment_limit=0x10000;c.extend_success_slot=1;c.expected_trace="EE";test(c);
        c.second_segment_limit=0x20000;c.extend_changes_size=true;
        c.name="first extender changes size used by second segment";test(c);
        c={};c.name="all segment slots occupied no VM";c.all_slots_used=true;
        c.vm_enabled=true;c.expected_trace="";test(c);
        c={};c.name="VM reserve commit initialize succeed";c.vm_enabled=true;
        c.expected_trace="RCI";test(c);
        c={};c.name="VM one reserve retry";c.vm_enabled=true;c.reserve_failures=1;
        c.expected_trace="RRCI";test(c);
        c={};c.name="VM reserve failure callback rewrites size";c.vm_enabled=true;
        c.reserve_failures=1;c.reserve_failure_output_size=0x60000;
        c.expected_trace="RRCI";test(c);
        c={};c.name="VM reserve exhausted";c.vm_enabled=true;c.reserve_failures=3;
        c.expected_trace="RRR";test(c);
        c={};c.name="VM commit fails then frees";c.vm_enabled=true;c.commit_status=-1;
        c.expected_trace="RCF";test(c);
        c.free_changes_outputs=true;c.name="VM free changes output aliases";test(c);
        c={};c.name="VM initializer fails then frees";c.vm_enabled=true;c.init_success=false;
        c.expected_trace="RCIF";test(c);
        c={};c.name="VM first empty slot one";c.vm_enabled=true;
        c.first_segment_pages=1;c.first_segment_limit=0;
        c.expected_trace="RCI";test(c);
        c={};c.name="VM reserve callback chooses different base";c.vm_enabled=true;
        c.vm_base=VMBase+0x10000;c.expected_trace="RCI";test(c);
        c={};c.name="VM commit callback changes committed size";c.vm_enabled=true;
        c.commit_output_size=0x40000;c.expected_trace="RCI";test(c);
        c={};c.name="VM initializer high-bit nonzero return";c.vm_enabled=true;
        c.init_result=0x80000000u;c.expected_trace="RCI";test(c);
        c={};c.name="VM reserve positive status";c.vm_enabled=true;
        c.reserve_success_status=(std::numeric_limits<std::int32_t>::max)();
        c.expected_trace="RCI";test(c);
        c={};c.name="VM commit positive status";c.vm_enabled=true;
        c.commit_status=(std::numeric_limits<std::int32_t>::max)();
        c.expected_trace="RCI";test(c);
        std::printf("PASS 827CC428 %u\n",cases);
        std::puts("LIMIT: synthetic extender, initializer and kernel VM callbacks; no game runtime/MMIO/concurrency");
        return 0;
    } catch(const std::exception& error) {
        active=nullptr;std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;
    }
}
