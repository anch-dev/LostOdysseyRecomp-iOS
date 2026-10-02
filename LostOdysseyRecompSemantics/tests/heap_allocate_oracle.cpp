// Appended after verbatim original PPC bodies and actual ABI helper bodies.
#include "lo_semantics/heap_allocate.h"

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
constexpr std::size_t MemorySize=0x400000, ScratchStart=0x3e0000;
constexpr GuestAddress Heap=0x10000, Block=0x100000, GrowthBlock=0x180000;
constexpr GuestAddress NextBlock=0x280000, VirtualBase=0x300000;
constexpr GuestAddress Descriptor=0x360000, Sentinel=0x370000;
constexpr GuestAddress StackTop=0x3f0000, Frame=StackTop-320;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool okay,const char* reason) { if (!okay) throw std::runtime_error(reason); }
enum class Kind { Allocate, Cleanup };
struct Case {
    Kind kind=Kind::Allocate;
    std::string name;
    std::uint32_t bytes=33, flags=0, heap_flags=0, heap_status=0;
    std::uint32_t block_units=4, vm_cutoff=0xffff, lock_owned=0;
    bool seed_block=true, last_flag=false, next_free=false;
    bool grow_success=false, process_guard=false, process_mismatch=false;
    bool process_mutates=false, enter_changes_mirror=false;
    bool grow_changes_rounded=false;
    bool enter_changes_rounded=false, leave_changes_result=false;
    bool leave_changes_lock=false;
    bool vm_writes=true, vm_result_changes=false, raise_changes_result=false;
    std::uint32_t next_units=3, grow_units=256, vm_size=0x2000;
    std::int32_t vm_status=0;
};
struct Event {
    std::string name;
    std::array<std::uint32_t,5> args{};
    bool operator==(const Event&) const=default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& scenario;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress a) const {
        return (std::uint32_t(bytes[a])<<24)|(std::uint32_t(bytes[a+1])<<16)|
               (std::uint32_t(bytes[a+2])<<8)|bytes[a+3];
    }
    void Write16(GuestAddress a,std::uint16_t v) {
        bytes[a]=std::uint8_t(v>>8);bytes[a+1]=std::uint8_t(v);
    }
    void Write32(GuestAddress a,std::uint32_t v) {
        bytes[a]=std::uint8_t(v>>24);bytes[a+1]=std::uint8_t(v>>16);
        bytes[a+2]=std::uint8_t(v>>8);bytes[a+3]=std::uint8_t(v);
    }
    void Snapshot() {
        snapshots.emplace_back(bytes,bytes+ScratchStart);
        snapshots.back().insert(snapshots.back().end(),bytes+Frame+80,bytes+Frame+168);
        if (scenario.kind==Kind::Allocate)
            snapshots.back().insert(snapshots.back().end(),bytes+Frame+312,bytes+Frame+316);
    }
    void Record(const char* name,std::array<std::uint32_t,5> args) {
        Snapshot();events.push_back({name,args});
        if (scenario.enter_changes_rounded && std::strcmp(name,"enter")==0)
            Write32(Frame+88,Read32(Frame+88)+16);
        if (scenario.enter_changes_mirror && std::strcmp(name,"enter")==0)
            Write32(Frame+104,0);
        if (scenario.process_mutates && std::strcmp(name,"process")==0) {
            bytes[Heap+379]=3;
            Write32(Frame+312,0x99887766);
        }
        if (scenario.leave_changes_lock && std::strcmp(name,"enter")==0)
            Write32(Heap+1408,Sentinel+4);
        if (scenario.leave_changes_result && std::strcmp(name,"leave")==0)
            Write32(Frame+100,0x1234abcd);
        if (scenario.vm_writes && std::strcmp(name,"vm")==0) {
            Write32(Frame+84,VirtualBase);
            Write32(Frame+88,scenario.vm_size);
        }
        if (scenario.vm_result_changes && std::strcmp(name,"vm")==0)
            Write32(Frame+100,0xabc12345);
        if (scenario.raise_changes_result && std::strcmp(name,"raise")==0)
            Write32(Frame+100,0x76543210);
        Snapshot();
    }
    void Grow(GuestAddress heap,std::uint32_t rounded) {
        Record("grow",{heap,rounded,0,0,0});
        if (scenario.grow_changes_rounded)
            Write32(Frame+88,Read32(Frame+88)+32);
        if (!scenario.grow_success) return;
        const auto node=GrowthBlock+8,head=Heap+384;
        Require(Read32(head)==head,"GrowHeap starts with empty large list");
        Write16(GrowthBlock,static_cast<std::uint16_t>(scenario.grow_units));
        Write16(GrowthBlock+2,0);
        bytes[GrowthBlock+4]=0;bytes[GrowthBlock+5]=0x10;
        Write32(node,head);Write32(node+4,head);
        Write32(head,node);Write32(head+4,node);
        Write32(Heap+48,Read32(Heap+48)+scenario.grow_units);
        Snapshot();
    }
};
Run* active=nullptr;
struct Services final : HeapAllocateServices {
    Run& run;
    explicit Services(Run& value):run(value) {}
    std::uint32_t GetCurrentProcessType() override {
        run.Record("process",{0,0,0,0,0});return run.scenario.process_mismatch?2:1;
    }
    void BugCheck(std::uint32_t code,GuestAddress heap,GuestAddress caller_return,
                  std::uint32_t line,GuestAddress requested_bytes) override {
        run.Record("bug",{code,heap,caller_return,line,requested_bytes});
    }
    void EnterCriticalSection(GuestAddress address) override {
        run.Record("enter",{address,0,0,0,0});
    }
    void LeaveCriticalSection(GuestAddress address) override {
        run.Record("leave",{address,0,0,0,0});
    }
    GuestAddress GrowHeap(GuestAddress heap,std::uint32_t rounded) override {
        Require(heap==Heap,"GrowHeap heap argument");
        run.Grow(heap,rounded);return run.scenario.grow_success?GrowthBlock:0;
    }
    std::int32_t AllocateVirtualMemory(GuestAddress base_out,GuestAddress size_out,
        std::uint32_t type,std::uint32_t protect,std::uint32_t zero) override {
        run.Record("vm",{base_out,size_out,type,protect,zero});
        return run.scenario.vm_status;
    }
    void RaiseException(GuestAddress record) override {
        run.Record("raise",{record,0,0,0,0});
    }
};
GuestAddress Head(std::uint32_t units) {return Heap+(units+48)*8;}
void Header(Run& run,GuestAddress block,std::uint16_t units,std::uint8_t flags) {
    run.Write16(block,units);run.Write16(block+2,0);
    run.bytes[block+4]=0;run.bytes[block+5]=flags;
}
void LinkSingleton(Run& run,GuestAddress block,std::uint32_t units) {
    const auto head=Head(units<128?units:0),node=block+8;
    Require(run.Read32(head)==head,"empty seeded list");
    run.Write32(head,node);run.Write32(head+4,node);
    run.Write32(node,head);run.Write32(node+4,head);
    if (units<128) {
        auto word=Heap+((units>>5)+88)*4;
        run.Write32(word,run.Read32(word)|(1u<<(units&31)));
    }
    run.Write32(Heap+48,run.Read32(Heap+48)+units);
}
void Setup(Run& run) {
    const Case& c=run.scenario;
    std::memset(run.bytes,0xa5,MemorySize);
    for (unsigned size=0;size<128;++size) {
        auto head=Head(size);run.Write32(head,head);run.Write32(head+4,head);
    }
    for (unsigned i=0;i<4;++i)run.Write32(Heap+(88+i)*4,0);
    run.Write32(Heap+88,Heap+88);run.Write32(Heap+92,Heap+88);
    run.Write32(Heap+48,0);run.Write32(Heap+20,c.heap_status|(c.process_guard?0x40000:0));
    run.Write32(Heap+24,c.heap_flags);run.Write32(Heap+28,c.vm_cutoff);
    run.Write32(Heap+96,Descriptor);run.Write32(Descriptor+44,0x340000);
    run.Write32(Descriptor+64,Block);
    run.Write32(Heap+1408,Sentinel);run.bytes[Heap+379]=1;
    run.Write32(Frame+312,CallerLR);
    if (c.kind==Kind::Cleanup || !c.seed_block) return;
    Header(run,Block,static_cast<std::uint16_t>(c.block_units),c.last_flag?0x10:0);
    LinkSingleton(run,Block,c.block_units);
    if (c.block_units<=0xf000) {
        auto next=Block+c.block_units*16;
        Header(run,next,static_cast<std::uint16_t>(c.next_units),c.next_free?0:1);
        if (c.next_free) LinkSingleton(run,next,c.next_units);
        Header(run,next+c.next_units*16,1,1);
    }
}
void Compare(const Case& c,unsigned ordinal) {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a && b,"allocate independent guest windows");
    try {
        Run original{a,c},recovered{b,c};Setup(original);std::memcpy(b,a,MemorySize);
        PPCContext ctx{};ctx.r1.u64=StackTop;ctx.lr=CallerLR;
        constexpr std::array<std::uint64_t,10> saved={
            0x1122334455667788ull,0x2233445566778899ull,0x33445566778899aaull,
            0x445566778899aabbull,0x5566778899aabbccull,0x66778899aabbccddull,
            0x778899aabbccddeeull,0x8899aabbccddeeffull,0x99aabbccddeeff00ull,
            0xaabbccddeeff0011ull};
        ctx.r22.u64=saved[0];ctx.r23.u64=saved[1];ctx.r24.u64=saved[2];
        ctx.r25.u64=saved[3];ctx.r26.u64=saved[4];ctx.r27.u64=saved[5];
        ctx.r28.u64=saved[6];ctx.r29.u64=saved[7];ctx.r30.u64=saved[8];ctx.r31.u64=saved[9];
        if (c.kind==Kind::Allocate) {
            ctx.r3.u64=Heap;ctx.r4.u64=c.flags;ctx.r5.u64=c.bytes;
        } else {
            ctx.r12.u64=StackTop;ctx.r27.u64=Heap;ctx.r22.u64=c.lock_owned;
        }
        const auto expected22=ctx.r22.u64,expected27=ctx.r27.u64;
        active=&original;
        if (c.kind==Kind::Allocate)oracle_AllocateHeapBlock(ctx,a);
        else oracle_LeaveHeapAllocateCriticalSection(ctx,a);
        active=nullptr;
        Require(ctx.r1.u64==StackTop && ctx.lr==(c.process_mutates?0x99887766u:CallerLR),
                "original stack/LR restored from guest save slot");
        Require(ctx.r22.u64==expected22 && ctx.r23.u64==saved[1] && ctx.r24.u64==saved[2] &&
                ctx.r25.u64==saved[3] && ctx.r26.u64==saved[4] && ctx.r27.u64==expected27 &&
                ctx.r28.u64==saved[6] && ctx.r29.u64==saved[7] && ctx.r30.u64==saved[8] &&
                ctx.r31.u64==saved[9],"original nonvolatile registers restored");
        GuestMemory memory(0,{b,MemorySize});Services services(recovered);
        std::uint32_t result=0;
        if (c.kind==Kind::Allocate)
            result=AllocateHeapBlock(memory,services,Heap,c.flags,c.bytes,Frame);
        else LeaveHeapAllocateCriticalSection(memory,services,Heap,c.lock_owned);
        bool same=original.events==recovered.events && original.snapshots==recovered.snapshots &&
                  std::equal(a,a+ScratchStart,b) &&
                  std::equal(a+Frame+80,a+Frame+168,b+Frame+80) &&
                  (c.kind==Kind::Cleanup || std::equal(a+Frame+312,a+Frame+316,b+Frame+312));
        if (c.kind==Kind::Allocate)same=same && ctx.r3.u32==result;
        if (c.kind==Kind::Allocate && (c.flags&4)!=0 && result==0 &&
            std::any_of(original.events.begin(),original.events.end(),
                [](const Event& e){return e.name=="raise";}))
            Require(original.Read32(Frame+156)==0xa5a5a5a5u,
                    "PPC exception record +156 remains untouched");
        if (!same) {
            GuestAddress first=0;
            while (first<ScratchStart && a[first]==b[first])++first;
            std::fprintf(stderr,"FAIL %s #%u %s return=%08x/%08x events=%zu/%zu first=%08x byte=%02x/%02x snapshots=%zu/%zu\n",
                         c.kind==Kind::Allocate?"allocate":"cleanup",ordinal,c.name.c_str(),
                         ctx.r3.u32,result,original.events.size(),recovered.events.size(),first,
                         first<ScratchStart?a[first]:0,first<ScratchStart?b[first]:0,
                         original.snapshots.size(),recovered.snapshots.size());
            for (std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if (!(original.events[i]==recovered.events[i])) {
                    auto& x=original.events[i];auto& y=recovered.events[i];
                    std::fprintf(stderr,"event %zu %s/%s args %08x,%08x,%08x,%08x,%08x / %08x,%08x,%08x,%08x,%08x\n",
                                 i,x.name.c_str(),y.name.c_str(),x.args[0],x.args[1],x.args[2],x.args[3],x.args[4],
                                 y.args[0],y.args[1],y.args[2],y.args[3],y.args[4]);
                }
            for (unsigned offset=80;offset<168;offset+=4)
                if (original.Read32(Frame+offset)!=recovered.Read32(Frame+offset))
                    std::fprintf(stderr,"frame+%u %08x/%08x\n",offset,
                                 original.Read32(Frame+offset),recovered.Read32(Frame+offset));
            throw std::runtime_error("PPC/recovered heap allocation differs");
        }
    } catch (...) {active=nullptr;VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);throw;}
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

PPC_FUNC(sub_823AD544) { oracle_LeaveHeapAllocateCriticalSection(ctx,base); }
PPC_FUNC(sub_827CBA60) { oracle_InsertFreeBlocks(ctx,base); }
PPC_FUNC(sub_82B7BC40) { oracle_FillGuestMemory(ctx,base); }
PPC_FUNC(sub_827CC428) {
    active->Grow(ctx.r3.u32,ctx.r4.u32);
    ctx.r3.u64=active->scenario.grow_success?GrowthBlock:0;
}
PPC_FUNC(__imp__KeGetCurrentProcessType) {
    active->Record("process",{0,0,0,0,0});ctx.r3.u64=active->scenario.process_mismatch?2:1;
}
PPC_FUNC(__imp__KeBugCheckEx) {
    active->Record("bug",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32});
}
PPC_FUNC(__imp__RtlEnterCriticalSection) { active->Record("enter",{ctx.r3.u32,0,0,0,0}); }
PPC_FUNC(__imp__RtlLeaveCriticalSection) { active->Record("leave",{ctx.r3.u32,0,0,0,0}); }
PPC_FUNC(__imp__NtAllocateVirtualMemory) {
    active->Record("vm",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32});
    ctx.r3.s64=active->scenario.vm_status;
}
PPC_FUNC(__imp__RtlRaiseException) { active->Record("raise",{ctx.r3.u32,0,0,0,0}); }

int main() {
    try {
        unsigned allocate_count=0,cleanup_count=0;
        auto test=[&](Case c) {Compare(c,c.kind==Kind::Allocate?allocate_count++:cleanup_count++);};
        Case c;c.name="exact small";test(c);
        c={};c.name="exact small padding16 not last";c.bytes=16;c.block_units=2;test(c);
        c={};c.name="zero byte request";c.bytes=0;c.block_units=2;test(c);
        c={};c.name="small remainder one";c.block_units=5;test(c);
        c={};c.name="small remainder two";c.block_units=6;test(c);
        c={};c.name="small last flag split";c.block_units=6;c.last_flag=true;test(c);
        c={};c.name="small split next busy";c.block_units=6;test(c);
        c={};c.name="small split next free";c.block_units=6;c.next_free=true;test(c);
        c={};c.name="small exact no lock";c.heap_flags=1;test(c);
        c={};c.name="caller skip lock";c.flags=1;test(c);
        c={};c.name="enter changes rounded bytes";c.enter_changes_rounded=true;test(c);
        c={};c.name="enter changes lock mirror only";c.enter_changes_mirror=true;test(c);
        c={};c.name="leave changes result and rereads lock";c.leave_changes_result=true;c.leave_changes_lock=true;test(c);
        c={};c.name="guard mismatch";c.process_guard=true;c.process_mismatch=true;test(c);
        c={};c.name="guard callback changes saved LR";c.process_guard=true;
        c.process_mismatch=true;c.process_mutates=true;test(c);
        c={};c.name="zero payload fill";c.flags=8;test(c);
        c={};c.name="bitmap size31 exact";c.bytes=31*16-31;c.block_units=31;test(c);
        c={};c.name="bitmap size32 exact";c.bytes=32*16-31;c.block_units=32;test(c);
        c={};c.name="bitmap next word size32";c.bytes=31*16-31;c.block_units=32;test(c);
        c={};c.name="bitmap word0 highest bit31";c.bytes=1;c.block_units=31;test(c);
        c={};c.name="bitmap word2 size64";c.bytes=63*16-31;c.block_units=64;test(c);
        c={};c.name="bitmap word3 size96";c.bytes=95*16-31;c.block_units=96;test(c);
        c={};c.name="bitmap skip three empty words";c.bytes=1;c.block_units=96;test(c);
        c={};c.name="large first fit size128";c.bytes=128*16-31;c.block_units=128;test(c);
        c={};c.name="large first fit remainder one";c.bytes=128*16-31;c.block_units=129;test(c);
        c={};c.name="remainder plus next exceeds F000";c.bytes=1;c.block_units=0xf000;
        c.next_free=true;c.next_units=100;test(c);
        c={};c.name="grow succeeds";c.seed_block=false;c.grow_success=true;test(c);
        c={};c.name="grow fails";c.seed_block=false;test(c);
        c={};c.name="grow fails and raises exception";c.seed_block=false;c.flags=4;test(c);
        c={};c.name="grow callback changes failure record size";c.seed_block=false;
        c.grow_changes_rounded=true;c.flags=4;test(c);
        c={};c.name="request arithmetic wraps";c.seed_block=false;c.bytes=0xfffffff0u;test(c);
        c={};c.name="VM threshold equal stays heap";c.bytes=128*16-31;c.block_units=128;
        c.vm_cutoff=128;c.heap_status=2;test(c);
        c={};c.name="VM threshold greater succeeds";c.bytes=129*16-31;c.seed_block=false;
        c.vm_cutoff=128;c.heap_status=2;test(c);
        c={};c.name="VM zero flag changes type";c.bytes=129*16-31;c.seed_block=false;
        c.vm_cutoff=128;c.heap_status=2;c.flags=8;test(c);
        c={};c.name="VM positive NTSTATUS succeeds";c.bytes=129*16-31;c.seed_block=false;
        c.vm_cutoff=128;c.heap_status=2;c.vm_status=(std::numeric_limits<std::int32_t>::max)();test(c);
        c.vm_status=0;c.vm_result_changes=true;
        c.name="VM callback result overwritten by success";test(c);
        c={};c.name="VM negative NTSTATUS fails";c.bytes=129*16-31;c.seed_block=false;
        c.vm_cutoff=128;c.heap_status=2;c.vm_status=(std::numeric_limits<std::int32_t>::min)();test(c);
        c.vm_result_changes=true;c.name="VM callback result overwritten by failure";test(c);
        c={};c.name="VM fails and raises";c.bytes=129*16-31;c.seed_block=false;
        c.vm_cutoff=128;c.heap_status=2;c.vm_status=-1;c.flags=4;test(c);
        c.raise_changes_result=true;c.name="RaiseException callback result then cleared";test(c);
        for (auto lock:{0u,1u,0xffffffffu}) {
            c={};c.kind=Kind::Cleanup;c.lock_owned=lock;c.name="standalone cleanup";test(c);
        }
        c={};c.kind=Kind::Cleanup;c.lock_owned=1;c.leave_changes_result=true;
        c.name="standalone cleanup callback side effect";test(c);
        std::printf("PASS 823ACCB0 %u\n",allocate_count);
        std::printf("PASS 823AD544 %u\n",cleanup_count);
        std::puts("LIMIT: bounded synthetic heap and kernel/Grow callbacks; no exception landing pad or runtime");
        return 0;
    } catch (const std::exception& error) {
        active=nullptr;std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;
    }
}
