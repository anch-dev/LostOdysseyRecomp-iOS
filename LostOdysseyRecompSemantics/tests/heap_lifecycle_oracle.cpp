// Appended after pinned, verbatim generated PPC bodies and their real ABI helpers.
#include "lo_semantics/heap_allocate.h"
#include "lo_semantics/heap_decommit.h"
#include "lo_semantics/heap_free.h"
#include "lo_semantics/heap_growth.h"
#include "lo_semantics/heap_ranges.h"
#include "lo_semantics/heap_segment.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize=0x400000, OrdinaryEnd=0x3e0000;
constexpr GuestAddress Heap=0x10000, SegmentBase=0x180000, PoolBase=0x280000;
constexpr GuestAddress StackTop=0x3f0000;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}

struct Event {
    std::string name;
    std::array<std::uint32_t,7> args{};
    std::array<std::uint32_t,2> before_outputs{};
    std::array<std::uint32_t,2> after_outputs{};
    bool operator==(const Event&) const=default;
};
struct Run {
    std::uint8_t* bytes;
    GuestAddress active_sp=StackTop;
    bool segment_reserved=false,pool_reserved=false;
    std::array<unsigned,4> composed_calls{}; // Grow, Initialize, Extend, Decommit.
    unsigned fill_calls=0;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress a) const {
        Require(a+4<=MemorySize,"guest read32 bounds");
        return (std::uint32_t(bytes[a])<<24)|(std::uint32_t(bytes[a+1])<<16)|
               (std::uint32_t(bytes[a+2])<<8)|bytes[a+3];
    }
    void Write32(GuestAddress a,std::uint32_t value) {
        Require(a+4<=MemorySize,"guest write32 bounds");
        bytes[a]=std::uint8_t(value>>24);bytes[a+1]=std::uint8_t(value>>16);
        bytes[a+2]=std::uint8_t(value>>8);bytes[a+3]=std::uint8_t(value);
    }
    void Snapshot() {snapshots.emplace_back(bytes,bytes+OrdinaryEnd);}
    void Record(const char* name,std::array<std::uint32_t,7> args,
                std::array<std::uint32_t,2> outputs={}) {
        Snapshot();events.push_back({name,args,outputs,{}});
    }
    void End() {
        auto& event=events.back();
        if(event.name=="reserve"||event.name=="commit"||event.name=="freevm")
            event.after_outputs={Read32(event.args[0]),Read32(event.args[1])};
        if(event.name=="commitcallback")
            event.after_outputs={Read32(event.args[2]),Read32(event.args[3])};
        Snapshot();
    }
    std::int32_t AllocateVM(GuestAddress baseptr,GuestAddress sizeptr,
                            std::uint32_t type,std::uint32_t protect,std::uint32_t zero) {
        Require(baseptr+4<=MemorySize&&sizeptr+4<=MemorySize,"VM output pointers");
        const bool reserve=type==0x60002000;
        Require(reserve||type==0x60001000,"VM allocation type");
        const auto oldbase=Read32(baseptr),oldsize=Read32(sizeptr);
        Record(reserve?"reserve":"commit",{baseptr,sizeptr,type,protect,zero,0,0},
               {oldbase,oldsize});
        if(reserve) {
            if(oldsize==0x100000) {
                Require(!pool_reserved,"pool reserved once");pool_reserved=true;
                Write32(baseptr,PoolBase);
            } else {
                Require(!segment_reserved,"segment reserved once");segment_reserved=true;
                Require(oldsize<=0x40000,"segment reserve fits");
                Write32(baseptr,SegmentBase);
                Write32(sizeptr,0x40000);
            }
        } else {
            const auto base=Read32(baseptr),size=Read32(sizeptr);
            Require(base>=SegmentBase&&base+size<=PoolBase+0x100000,
                    "VM commit in synthetic reservations");
            std::memset(bytes+base,0,size);
        }
        End();return 0;
    }
    std::int32_t FreeVM(GuestAddress baseptr,GuestAddress sizeptr,
                        std::uint32_t type,std::uint32_t zero) {
        Record("freevm",{baseptr,sizeptr,type,zero,0,0,0},
               {Read32(baseptr),Read32(sizeptr)});
        End();return 0;
    }
    std::uint32_t Compare(GuestAddress source,std::uint32_t count,std::uint32_t value) {
        Record("compare",{source,count,value,0,0,0,0});End();return 0;
    }
};
Run* active=nullptr;
struct StackScope {
    Run& run;GuestAddress previous;
    StackScope(Run& r,GuestAddress next):run(r),previous(r.active_sp){run.active_sp=next;}
    ~StackScope(){run.active_sp=previous;}
};
struct Services final:HeapAllocateServices,HeapFreeServices,HeapGrowthServices,
                      HeapSegmentServices,HeapDecommitServices,HeapRangeServices {
    Run& run;GuestMemory memory;
    explicit Services(Run& r):run(r),memory(0,{r.bytes,MemorySize}){}
    std::uint32_t GetCurrentProcessType() override {
        run.Record("process",{});run.End();return 1;
    }
    void BugCheck(std::uint32_t code,GuestAddress heap,GuestAddress caller,
                  std::uint32_t line,std::uint32_t argument) override {
        run.Record("bug",{code,heap,caller,line,argument,0,0});run.End();
    }
    void EnterCriticalSection(GuestAddress address) override {
        run.Record("enter",{address,0,0,0,0,0,0});run.End();
    }
    void LeaveCriticalSection(GuestAddress address) override {
        run.Record("leave",{address,0,0,0,0,0,0});run.End();
    }
    GuestAddress GrowHeap(GuestAddress heap,std::uint32_t rounded) override {
        ++run.composed_calls[0];
        const auto frame=run.active_sp-144;StackScope scope(run,frame);
        return lo::semantic::gpu::GrowHeap(memory,*this,heap,rounded,frame);
    }
    GuestAddress ExtendSegment(GuestAddress heap,GuestAddress segment,
                               GuestAddress sizeptr,std::uint32_t required) override {
        ++run.composed_calls[2];
        const auto frame=run.active_sp-160;StackScope scope(run,frame);
        return ExtendHeapSegment(memory,*this,heap,segment,sizeptr,required,frame);
    }
    GuestAddress InitializeSegment(GuestAddress heap,GuestAddress base,
        std::uint32_t slot,std::uint32_t flags,GuestAddress range_begin,
        GuestAddress committed_end,GuestAddress reserved_end) override {
        ++run.composed_calls[1];
        const auto frame=run.active_sp-176;StackScope scope(run,frame);
        return InitializeHeapSegment(memory,*this,heap,base,slot,flags,
                                     range_begin,committed_end,reserved_end,frame);
    }
    std::int32_t CommitRange(GuestAddress target,GuestAddress heap,
                             GuestAddress baseptr,GuestAddress sizeptr) override {
        run.Record("commitcallback",{target,heap,baseptr,sizeptr,0,0,0},
                   {run.Read32(baseptr),run.Read32(sizeptr)});
        run.End();return 0;
    }
    void InitializeUncommittedRange(GuestAddress segment,GuestAddress base,
                                     std::uint32_t bytes) override {
        const auto frame=run.active_sp-128;StackScope scope(run,frame);
        lo::semantic::gpu::InsertRangeRecord(memory,*this,segment,base,bytes,frame);
    }
    void DecommitFreeBlock(GuestAddress heap,GuestAddress block,
                           std::uint32_t units) override {
        ++run.composed_calls[3];
        const auto frame=run.active_sp-208;StackScope scope(run,frame);
        lo::semantic::gpu::DecommitFreeBlock(memory,*this,heap,block,units,frame);
    }
    GuestAddress AllocateRangeNode(GuestAddress segment) override {
        const auto frame=run.active_sp-128;StackScope scope(run,frame);
        return lo::semantic::gpu::AllocateRangeNode(memory,*this,segment,frame);
    }
    void InsertRangeRecord(GuestAddress segment,GuestAddress base,
                            std::uint32_t bytes) override {
        const auto frame=run.active_sp-128;StackScope scope(run,frame);
        lo::semantic::gpu::InsertRangeRecord(memory,*this,segment,base,bytes,frame);
    }
    std::uint32_t CompareMemoryUlong(GuestAddress source,std::uint32_t count,
                                     std::uint32_t value) override {
        return run.Compare(source,count,value);
    }
    std::int32_t AllocateVirtualMemory(GuestAddress baseptr,GuestAddress sizeptr,
        std::uint32_t type,std::uint32_t protect,std::uint32_t zero) override {
        return run.AllocateVM(baseptr,sizeptr,type,protect,zero);
    }
    std::int32_t FreeVirtualMemory(GuestAddress baseptr,GuestAddress sizeptr,
                                   std::uint32_t type,std::uint32_t zero) override {
        return run.FreeVM(baseptr,sizeptr,type,zero);
    }
    void RaiseException(GuestAddress address) override {
        run.Record("raise",{address,0,0,0,0,0,0});run.End();
    }
};
void Setup(Run& run) {
    std::memset(run.bytes,0xa5,MemorySize);
    for(unsigned units=0;units<128;++units) {
        const GuestAddress head=Heap+(units+48)*8;
        run.Write32(head,head);run.Write32(head+4,head);
    }
    for(unsigned i=0;i<4;++i)run.Write32(Heap+(88+i)*4,0);
    for(unsigned i=0;i<64;++i)run.Write32(Heap+(24+i)*4,0);
    run.Write32(Heap+20,2);run.Write32(Heap+24,0);
    run.Write32(Heap+28,0x10000); // Prefer segment growth over dedicated VM blocks.
    run.Write32(Heap+32,0x40000);run.Write32(Heap+36,0x30000);
    run.Write32(Heap+40,128);run.Write32(Heap+44,128);
    run.Write32(Heap+48,0);run.Write32(Heap+88,Heap+88);
    run.Write32(Heap+92,Heap+88);
    run.Write32(Heap+72,0);run.Write32(Heap+76,0);
    run.Write32(Heap+1408,0x70000);run.Write32(Heap+1412,0);
    run.Write32(StackTop-320+312,CallerLR);
    run.Write32(StackTop-176+168,CallerLR);
}
void CheckAbi(const PPCContext& ctx,const std::array<std::uint64_t,18>& saved) {
    Require(ctx.r1.u64==StackTop&&ctx.lr==CallerLR,"PPC stack and LR restored");
    std::array<std::uint64_t,18> after={ctx.r14.u64,ctx.r15.u64,ctx.r16.u64,
        ctx.r17.u64,ctx.r18.u64,ctx.r19.u64,ctx.r20.u64,ctx.r21.u64,
        ctx.r22.u64,ctx.r23.u64,ctx.r24.u64,ctx.r25.u64,ctx.r26.u64,
        ctx.r27.u64,ctx.r28.u64,ctx.r29.u64,ctx.r30.u64,ctx.r31.u64};
    Require(after==saved,"PPC nonvolatile GPR restored");
}
void SetAbi(PPCContext& ctx,std::array<std::uint64_t,18>& saved) {
    std::array<PPCRegister*,18> regs={&ctx.r14,&ctx.r15,&ctx.r16,&ctx.r17,
        &ctx.r18,&ctx.r19,&ctx.r20,&ctx.r21,&ctx.r22,&ctx.r23,&ctx.r24,
        &ctx.r25,&ctx.r26,&ctx.r27,&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
    for(unsigned i=0;i<regs.size();++i) {
        saved[i]=0x1122334455667700ull+i;regs[i]->u64=saved[i];
    }
    ctx.r1.u64=StackTop;ctx.lr=CallerLR;
}
void CompareState(const Run& original,const Run& recovered,const char* phase) {
    if(original.events!=recovered.events || original.snapshots!=recovered.snapshots ||
       std::memcmp(original.bytes,recovered.bytes,OrdinaryEnd)!=0) {
        std::size_t first=0;while(first<OrdinaryEnd&&original.bytes[first]==recovered.bytes[first])++first;
        std::fprintf(stderr,"FAIL %s: events=%zu/%zu snapshots=%zu/%zu first=%08zx\n",
                     phase,original.events.size(),recovered.events.size(),
                     original.snapshots.size(),recovered.snapshots.size(),first);
        for(std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
            if(!(original.events[i]==recovered.events[i])) {
                auto& a=original.events[i];auto& b=recovered.events[i];
                std::fprintf(stderr,"event %zu %s/%s args:",i,a.name.c_str(),b.name.c_str());
                for(unsigned j=0;j<7;++j)std::fprintf(stderr," %08x/%08x",a.args[j],b.args[j]);
                std::fprintf(stderr," before %08x/%08x %08x/%08x after %08x/%08x %08x/%08x\n",
                    a.before_outputs[0],b.before_outputs[0],
                    a.before_outputs[1],b.before_outputs[1],
                    a.after_outputs[0],b.after_outputs[0],
                    a.after_outputs[1],b.after_outputs[1]);
            }
        throw std::runtime_error("PPC/recovered lifecycle mismatch");
    }
}
void CompareLifecycle() {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a&&b,"independent guest windows");
    try {
        Run original{a},recovered{b};Setup(original);std::memcpy(b,a,MemorySize);
        GuestMemory memory(0,{b,MemorySize});Services services(recovered);
        PPCContext ctx{};std::array<std::uint64_t,18> saved{};
        const std::array<std::uint32_t,2> requests={0x10000,0x18000};
        for(unsigned operation=0;operation<3;++operation) {
            std::memset(a+OrdinaryEnd,0xa5,MemorySize-OrdinaryEnd);
            std::memset(b+OrdinaryEnd,0xa5,MemorySize-OrdinaryEnd);
            SetAbi(ctx,saved);
            ctx.r3.u64=Heap;ctx.r4.u64=operation==0?8:0;
            if(operation!=1)ctx.r5.u64=requests[operation==0?0:1];
            else ctx.r5.u64=original.Read32(0x70004);
            const GuestAddress topframe=StackTop-(operation==1?176:320);
            original.Write32(topframe+(operation==1?168:312),CallerLR);
            recovered.Write32(topframe+(operation==1?168:312),CallerLR);
            if(operation==1)ctx.r5.u64=original.Read32(0x70004);
            active=&original;
            if(operation==1)oracle_FreeHeapBlock(ctx,a);
            else oracle_AllocateHeapBlock(ctx,a);
            active=nullptr;
            CheckAbi(ctx,saved);
            const auto original_result=ctx.r3.u32;
            GuestAddress recovered_result=0;
            {
                StackScope scope(recovered,topframe);
                if(operation==1)
                    recovered_result=FreeHeapBlock(memory,services,Heap,0,recovered.Read32(0x70004),topframe);
                else recovered_result=AllocateHeapBlock(memory,services,Heap,operation==0?8:0,
                    requests[operation==0?0:1],topframe);
            }
            Require(original_result==recovered_result,"top-level return value");
            Require(operation==1?original_result==1:original_result!=0,
                    "allocation nonzero and free successful");
            if(operation==0) {
                original.Write32(0x70004,original_result);
                recovered.Write32(0x70004,recovered_result);
                Require(original.fill_calls==1 && original.bytes[original_result]==0 &&
                        recovered.bytes[recovered_result]==0,
                        "first allocation composed FillGuestMemory");
            }
            const char* phase=operation==0?"allocate-new-segment":
                              operation==1?"free-and-decommit":"allocate-from-extended-segment";
            CompareState(original,recovered,phase);
            if(operation==1) {
                for(unsigned offset:{80u,84u,88u,100u})
                    Require(original.Read32(topframe+offset)==recovered.Read32(topframe+offset),
                            "free live frame local");
            } else {
                for(unsigned offset:{88u,92u,96u,100u,104u,108u,112u,124u,128u})
                    Require(original.Read32(topframe+offset)==recovered.Read32(topframe+offset),
                            "allocate live frame local");
            }
            if(operation==0) {
                constexpr GuestAddress grow_frame=StackTop-320-144;
                constexpr GuestAddress child_slot=grow_frame+60;
                Require(child_slot==StackTop-320-144-176+236,
                        "Initialize spill aliases its Grow caller frame");
                Require(original.Read32(child_slot)==SegmentBase+0x30000 &&
                        recovered.Read32(child_slot)==original.Read32(child_slot),
                        "Initialize committed-end spill in parent Grow frame");
                const GuestAddress first_range=original.Read32(SegmentBase+56);
                Require(first_range!=0 && original.Read32(first_range+4)==SegmentBase+0x30000 &&
                        original.Read32(first_range+8)==0x10000,
                        "Initialize records initial uncommitted tail");
            }
            if(operation==1) {
                const GuestAddress merged_range=original.Read32(SegmentBase+56);
                Require(merged_range!=0 && original.Read32(SegmentBase+52)==1 &&
                        original.Read32(merged_range+4)==SegmentBase+0x10000 &&
                        original.Read32(merged_range+8)==0x30000,
                        "decommit merges with existing uncommitted tail");
            }
        }
        Require(original.segment_reserved&&original.pool_reserved,
                "lifecycle exercises segment and range pool reserve");
        Require(original.composed_calls==recovered.composed_calls &&
                original.composed_calls==std::array<unsigned,4>{2,1,1,1},
                "Grow, Initialize, Extend and Decommit all composed");
        Require(std::ranges::any_of(original.events,[](const Event& event) {
                    return event.name=="freevm" && event.args[2]==0x4000;
                }),"decommit kernel VM type");
        Require(original.Read32(Heap+96)==SegmentBase,"lifecycle initialized segment slot");
        Require(original.events.size()>=8,"lifecycle crossed kernel boundaries");
        std::printf("PASS LIFECYCLE 1 events=%zu\n",original.events.size());
    }catch(...) {active=nullptr;VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);throw;}
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

void oracle_CommitIndirect(PPCContext& ctx,std::uint8_t*,std::uint32_t target) {
    active->Record("commitcallback",{target,ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,0,0,0},
                   {active->Read32(ctx.r4.u32),active->Read32(ctx.r5.u32)});
    active->End();ctx.r3.s64=0;
}
PPC_FUNC(sub_823AD544){oracle_LeaveHeapAllocateCriticalSection(ctx,base);}
PPC_FUNC(sub_823AE0BC){oracle_LeaveHeapCriticalSection(ctx,base);}
PPC_FUNC(sub_823AE108){oracle_CoalesceFreeBlocks(ctx,base);}
PPC_FUNC(sub_827CBA60){oracle_InsertFreeBlocks(ctx,base);}
PPC_FUNC(sub_827CC428){++active->composed_calls[0];oracle_GrowHeap(ctx,base);}
PPC_FUNC(sub_827CB778){++active->composed_calls[2];oracle_ExtendHeapSegment(ctx,base);}
PPC_FUNC(sub_827CC2C0){++active->composed_calls[1];oracle_InitializeHeapSegment(ctx,base);}
PPC_FUNC(sub_827CC668){++active->composed_calls[3];oracle_DecommitFreeBlock(ctx,base);}
PPC_FUNC(sub_827CB498){oracle_AllocateRangeNode(ctx,base);}
PPC_FUNC(sub_827CB658){oracle_InsertRangeRecord(ctx,base);}
PPC_FUNC(sub_82B7BC40){++active->fill_calls;oracle_FillGuestMemory(ctx,base);}
PPC_FUNC(__imp__KeGetCurrentProcessType){active->Record("process",{});active->End();ctx.r3.u64=1;}
PPC_FUNC(__imp__KeBugCheckEx){active->Record("bug",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32,0,0});active->End();}
PPC_FUNC(__imp__RtlEnterCriticalSection){active->Record("enter",{ctx.r3.u32,0,0,0,0,0,0});active->End();}
PPC_FUNC(__imp__RtlLeaveCriticalSection){active->Record("leave",{ctx.r3.u32,0,0,0,0,0,0});active->End();}
PPC_FUNC(__imp__NtAllocateVirtualMemory){ctx.r3.s64=active->AllocateVM(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32);}
PPC_FUNC(__imp__NtFreeVirtualMemory){ctx.r3.s64=active->FreeVM(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32);}
PPC_FUNC(__imp__RtlCompareMemoryUlong){ctx.r3.u64=active->Compare(ctx.r3.u32,ctx.r4.u32,ctx.r5.u32);}
PPC_FUNC(__imp__RtlRaiseException){active->Record("raise",{ctx.r3.u32,0,0,0,0,0,0});active->End();}

int main() {
    try {CompareLifecycle();return 0;}
    catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
