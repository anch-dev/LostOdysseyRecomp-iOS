// Appended to the pinned original PPC bodies by the local test runner.
#include "lo_semantics/heap_decommit.h"
#include "lo_semantics/heap_ranges.h"
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
constexpr std::size_t MemorySize = 0x400000, ScratchStart = 0x3e0000;
constexpr GuestAddress Heap = 0x10000, Segment = 0x300000;
constexpr GuestAddress Owner = 0x310000, Node = 0x320000;
constexpr GuestAddress StackTop = 0x3f0000, Frame = StackTop - 208;
constexpr std::uint32_t CallerLR = 0x81234567;
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }

struct Case {
    GuestAddress block = 0x100000;
    std::uint32_t units = 0x3000, previous = 0, initial_free = 0;
    std::uint8_t flags = 0;
    bool disabled = false, node_failure = false, populated = false;
    bool allocate_mutates = false, vm_mutates = false, record_mutates = false;
    bool vm_changes_owner = false;
    bool compose_ranges = false;
    std::int32_t status = 0;
    GuestAddress last = 0x100000;
    std::string name;
};
struct Event {
    std::string name;
    std::array<std::uint32_t, 4> args{};
    bool operator==(const Event&) const = default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& c;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress a) const {
        return (std::uint32_t(bytes[a]) << 24) | (std::uint32_t(bytes[a+1]) << 16) |
               (std::uint32_t(bytes[a+2]) << 8) | bytes[a+3];
    }
    void Write16(GuestAddress a, std::uint16_t v) { bytes[a] = std::uint8_t(v >> 8); bytes[a+1] = std::uint8_t(v); }
    void Write32(GuestAddress a, std::uint32_t v) {
        bytes[a] = std::uint8_t(v >> 24); bytes[a+1] = std::uint8_t(v >> 16);
        bytes[a+2] = std::uint8_t(v >> 8); bytes[a+3] = std::uint8_t(v);
    }
    void Snapshot() {
        snapshots.emplace_back(bytes, bytes + ScratchStart);
        snapshots.back().insert(snapshots.back().end(), bytes + Frame + 80, bytes + Frame + 88);
    }
    void Record(const char* name, std::array<std::uint32_t, 4> args) {
        Snapshot(); events.push_back({name,args});
        if (c.allocate_mutates && std::strcmp(name,"node") == 0) {
            Write32(Frame+84, Read32(Frame+84)+0x10000);
            Write32(Frame+80,0x10000);
        }
        if (c.vm_mutates && std::strcmp(name,"vm") == 0) {
            Write32(args[0],Read32(args[0])+0x10000); Write32(args[1],0x10000);
        }
        if (c.vm_changes_owner && std::strcmp(name,"vm") == 0)
            Write32(Segment+24,Owner+0x100);
        if (c.record_mutates && std::strcmp(name,"record") == 0) {
            Write32(Frame+80,0x30000); Write32(Frame+84,0x180000);
            Write32(Segment+48,0xffffffffu);
        }
        Snapshot();
    }
};
Run* active = nullptr;
struct RangeServices final : HeapRangeServices {
    std::int32_t AllocateVirtualMemory(GuestAddress,GuestAddress,std::uint32_t,
                                     std::uint32_t,std::uint32_t) override {
        throw std::runtime_error("composed fixture must reuse the range node pool");
    }
    std::int32_t FreeVirtualMemory(GuestAddress,GuestAddress,std::uint32_t,std::uint32_t) override {
        throw std::runtime_error("composed range allocator must not release VM");
    }
};
struct Services final : HeapDecommitServices {
    Run& run;
    explicit Services(Run& r) : run(r) {}
    GuestAddress AllocateRangeNode(GuestAddress segment) override {
        if (run.c.compose_ranges) {
            GuestMemory memory(0,{run.bytes,MemorySize}); RangeServices services;
            return lo::semantic::gpu::AllocateRangeNode(memory,services,segment,Frame-128);
        }
        run.Record("node",{segment,0,0,0}); return run.c.node_failure ? 0 : Node;
    }
    void InsertRangeRecord(GuestAddress segment, GuestAddress base, std::uint32_t bytes) override {
        if (run.c.compose_ranges) {
            GuestMemory memory(0,{run.bytes,MemorySize}); RangeServices services;
            lo::semantic::gpu::InsertRangeRecord(memory,services,segment,base,bytes,Frame-128);
            return;
        }
        run.Record("record",{segment,base,bytes,0});
    }
    std::int32_t FreeVirtualMemory(GuestAddress base, GuestAddress size,
                                  std::uint32_t type, std::uint32_t zero) override {
        run.Record("vm",{base,size,type,zero}); return run.c.status;
    }
};
GuestAddress Head(std::uint32_t units) { return Heap+(units+48)*8; }
void Header(Run& run, GuestAddress a, std::uint16_t size,
            std::uint16_t previous, std::uint8_t flags) {
    run.Write16(a,size); run.Write16(a+2,previous);
    run.bytes[a+4]=0; run.bytes[a+5]=flags;
}
void Append(Run& run, GuestAddress block, std::uint16_t size) {
    Header(run,block,size,0,0);
    auto head=Head(size<128 ? size : 0), tail=run.Read32(head+4), node=block+8;
    run.Write32(node,head); run.Write32(node+4,tail);
    run.Write32(tail,node); run.Write32(head+4,node);
    if (size<128) {
        auto word=Heap+((size>>5)+88)*4;
        run.Write32(word,run.Read32(word)|(1u<<(size&31)));
    }
}
void Setup(Run& run) {
    std::memset(run.bytes,0xa5,MemorySize);
    for (unsigned size=0;size<128;++size) {
        auto head=Head(size); run.Write32(head,head); run.Write32(head+4,head);
    }
    for (unsigned i=0;i<4;++i) run.Write32(Heap+(88+i)*4,0);
    run.Write32(Heap+48,run.c.initial_free); run.Write32(Heap+96,Segment);
    run.Write32(Heap+1412,run.c.disabled ? 1 : 0);
    run.Write32(Segment+24,Owner); run.Write32(Owner+76,Node+16);
    run.Write32(Owner+0x100+76,Node+32);
    run.Write32(Segment+40,0x0f0000); run.Write32(Segment+44,0x200000);
    run.Write32(Segment+48,7); run.Write32(Segment+64,run.c.last);
    if (run.c.compose_ranges) {
        run.Write32(Owner+76,Node); run.Write32(Node,Node+16); run.Write32(Node+16,0);
        run.Write32(Owner+0x100+76,Node+32); run.Write32(Node+32,0);
        run.Write32(Segment+56,0); run.Write32(Segment+52,0); run.Write32(Segment+28,0);
    }
    run.bytes[Segment+4]=0;
    if (run.c.populated) {
        const std::array<std::uint16_t,7> sizes={2,31,127,128,1024,4097,8192};
        for (unsigned i=0;i<sizes.size();++i) Append(run,0x240000+i*16,sizes[i]);
    }
    if (run.c.previous)
        Header(run,run.c.block-run.c.previous*16,std::uint16_t(run.c.previous),0,1);
    Header(run,run.c.block,std::uint16_t(run.c.units),std::uint16_t(run.c.previous),run.c.flags);
    Header(run,run.c.block+run.c.units*16,2,std::uint16_t(run.c.units),1);
}
void Compare(const Case& c, unsigned ordinal) {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a && b,"independent guest windows");
    try {
        Run original{a,c}, recovered{b,c}; Setup(original); std::memcpy(b,a,MemorySize);
        PPCContext ctx{}; ctx.r1.u64=StackTop; ctx.lr=CallerLR;
        std::array<PPCRegister*,13> registers={&ctx.r19,&ctx.r20,&ctx.r21,&ctx.r22,&ctx.r23,
            &ctx.r24,&ctx.r25,&ctx.r26,&ctx.r27,&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
        for (unsigned i=0;i<registers.size();++i) registers[i]->u64=0x1122334455667700ull+i;
        ctx.r3.u64=Heap; ctx.r4.u64=c.block; ctx.r5.u64=c.units;
        active=&original; oracle_DecommitFreeBlock(ctx,a);
        GuestMemory memory(0,{b,MemorySize}); Services services(recovered);
        DecommitFreeBlock(memory,services,Heap,c.block,c.units,Frame);
        Require(ctx.r1.u64==StackTop && ctx.lr==CallerLR,"original ABI stack/LR");
        for (unsigned i=0;i<registers.size();++i)
            Require(registers[i]->u64==0x1122334455667700ull+i,"original nonvolatile GPR");
        Require(original.events==recovered.events,"callback names/arguments/order");
        Require(original.snapshots==recovered.snapshots,"callback memory boundaries");
        if (std::memcmp(a,b,ScratchStart)!=0) {
            for (std::size_t i=0;i<ScratchStart;++i) if (a[i]!=b[i]) {
                std::fprintf(stderr,"first byte %08zx PPC=%02x semantic=%02x\n",i,a[i],b[i]); break;
            }
            throw std::runtime_error("ordinary guest memory");
        }
        Require(std::memcmp(a+Frame+80,b+Frame+80,8)==0,"guest frame locals");
    } catch (const std::exception& error) {
        std::fprintf(stderr,"case %u (%s): %s\n",ordinal,c.name.c_str(),error.what());
        VirtualFree(a,0,MEM_RELEASE); VirtualFree(b,0,MEM_RELEASE); throw;
    }
    VirtualFree(a,0,MEM_RELEASE); VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

PPC_FUNC(sub_827CBA60) { oracle_InsertFreeBlocks(ctx,base); }
PPC_FUNC(sub_827CB498) {
    if (active->c.compose_ranges) { oracle_AllocateRangeNode(ctx,base); return; }
    active->Record("node",{ctx.r3.u32,0,0,0}); ctx.r3.u64=active->c.node_failure ? 0 : Node;
}
PPC_FUNC(sub_827CB658) {
    if (active->c.compose_ranges) { oracle_InsertRangeRecord(ctx,base); return; }
    active->Record("record",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,0});
}
PPC_FUNC(__imp__NtAllocateVirtualMemory) {
    throw std::runtime_error("composed original must reuse the range node pool");
}
PPC_FUNC(__imp__NtFreeVirtualMemory) {
    active->Record("vm",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32}); ctx.r3.s64=active->c.status;
}
// heap.cpp also contains coalescing; this ordinary test does not call it.
int main() {
    try {
        std::vector<Case> cases;
        auto add=[&](Case c, const char* name) { c.name=name; cases.push_back(c); };
        Case c;
        add(c,"page-aligned, segment last in decommitted range");
        c.last=0x0f0000; add(c,"segment last before range");
        c.last=0x150000; add(c,"segment last after range");
        c=Case{}; c.previous=2; add(c,"page-aligned previous block becomes segment end");
        c=Case{}; c.flags=0x10; add(c,"last block has no successor");
        c=Case{}; c.disabled=true; add(c,"heap disable falls back to insert");
        c=Case{}; c.node_failure=true; add(c,"range node allocation failure");
        for (auto status : {-1,(std::numeric_limits<std::int32_t>::min)(),1,(std::numeric_limits<std::int32_t>::max)()}) {
            c=Case{}; c.status=status; add(c,"signed VM status");
        }
        for (auto offset : {0u,0x10u,0xfe00u,0xffe0u,0xfff0u}) {
            for (auto units : {2u,127u,128u,4096u,4097u,8192u,12288u}) {
                c=Case{}; c.block+=offset; c.units=units; add(c,"page and single-unit remainder boundaries");
            }
        }
        for (auto populated : {false,true}) {
            for (auto prefix : {2u,31u,127u,128u,1024u,4097u}) {
                for (auto tail : {0u,2u,31u,127u,128u,1024u,4097u}) {
                    c=Case{}; c.block=0x120000-prefix*16;
                    c.units=prefix+8192+tail; c.populated=populated;
                    c.initial_free=0xfffffff0u; add(c,"small/large remainder list insertion and count wrap");
                }
            }
        }
        c=Case{}; c.allocate_mutates=true; add(c,"range allocator changes VM locals");
        c=Case{}; c.vm_mutates=true; add(c,"VM updates output locals");
        c=Case{}; c.vm_changes_owner=true; add(c,"VM changes range-node owner");
        c=Case{}; c.record_mutates=true; add(c,"record callback changes accounting locals");
        c=Case{}; c.status=-1; c.vm_changes_owner=true; c.vm_mutates=true;
        add(c,"VM failure recycles node then inserts entire block");
        for (auto offset : {0u,0x10u,0xffe0u,0xfff0u}) {
            for (auto status : {-1,0,1}) {
                c=Case{}; c.compose_ranges=true; c.block+=offset; c.status=status;
                add(c,"original decommit composes original node/range insertion");
            }
        }
        for (unsigned i=0;i<cases.size();++i) Compare(cases[i],i);
        std::printf("PASS 827CC668 %zu\n",cases.size()); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"FAIL %s\n",error.what()); return 1; }
}
