// The runner prepends pinned original PPC functions; fixtures contain no game data.
#include "lo_semantics/heap_ranges.h"
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
constexpr std::size_t MemorySize=0x400000, ScratchStart=0x3e0000;
constexpr GuestAddress Segment=0x300000, Owner=0x310000, Node=0x320000;
constexpr GuestAddress OldPool=0x140000, NewPool=0x180000;
constexpr GuestAddress StackTop=0x3f0000, Frame=StackTop-128;
constexpr std::uint32_t CallerLR=0x81234567;
void Require(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
enum class Kind { Allocate, Insert };
struct Case {
    Kind kind=Kind::Allocate;
    bool free_node=true, old_pool=false, full_pool=false, empty_ranges=false;
    bool reserve_changes_size=false, commit_changes_size=false, commit_changes_owner=false;
    std::int32_t reserve_status=0, commit_status=0;
    GuestAddress base=0x60000;
    std::uint32_t size=0x10000, count=3, maximum=0x10000;
    std::string name;
};
struct Event {
    std::string name;
    std::array<std::uint32_t,5> args{};
    bool operator==(const Event&) const=default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& c;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress a) const {
        return (std::uint32_t(bytes[a])<<24)|(std::uint32_t(bytes[a+1])<<16)|
               (std::uint32_t(bytes[a+2])<<8)|bytes[a+3];
    }
    void Write32(GuestAddress a,std::uint32_t v) {
        bytes[a]=std::uint8_t(v>>24); bytes[a+1]=std::uint8_t(v>>16);
        bytes[a+2]=std::uint8_t(v>>8); bytes[a+3]=std::uint8_t(v);
    }
    GuestAddress AllocatorFrame() const { return c.kind==Kind::Allocate ? Frame : Frame-128; }
    void Snapshot() {
        snapshots.emplace_back(bytes,bytes+ScratchStart);
        auto local=AllocatorFrame()+80;
        snapshots.back().insert(snapshots.back().end(),bytes+local,bytes+local+16);
    }
    std::int32_t Allocate(std::array<std::uint32_t,5> args) {
        Snapshot(); events.push_back({"allocate",args});
        auto [base,size,type,protect,zero]=args;
        Require(protect==4 && zero==0,"VM fixture contract");
        std::int32_t result;
        if (type==0x60002000) {
            Write32(base,NewPool);
            if (c.reserve_changes_size) Write32(size,0x50000);
            result=c.reserve_status;
        } else {
            Require(type==0x60001000,"commit type");
            if (c.commit_changes_size) Write32(size,0x20000);
            if (c.commit_changes_owner) Write32(Segment+24,Owner+0x100);
            result=c.commit_status;
        }
        Snapshot(); return result;
    }
    std::int32_t Free(std::array<std::uint32_t,5> args) {
        Snapshot(); events.push_back({"free",args});
        Write32(args[0],0); Write32(args[1],0); Snapshot(); return -1;
    }
};
Run* active=nullptr;
struct Services final : HeapRangeServices {
    Run& run;
    explicit Services(Run& r):run(r) {}
    std::int32_t AllocateVirtualMemory(GuestAddress b,GuestAddress s,std::uint32_t t,
                                     std::uint32_t p,std::uint32_t z) override {
        return run.Allocate({b,s,t,p,z});
    }
    std::int32_t FreeVirtualMemory(GuestAddress b,GuestAddress s,std::uint32_t t,std::uint32_t z) override {
        return run.Free({b,s,t,z,0});
    }
};
void Setup(Run& r) {
    std::memset(r.bytes,0xa5,MemorySize);
    r.Write32(Segment+24,Owner);
    for (auto owner : {Owner,Owner+0x100}) {
        r.Write32(owner+72,r.c.old_pool ? OldPool : 0);
        r.Write32(owner+76,r.c.free_node ? Node : 0);
    }
    r.Write32(Node,Node+16); r.Write32(Node+16,0);
    r.Write32(OldPool,0); r.Write32(OldPool+4,0x30000);
    r.Write32(OldPool+8,r.c.full_pool ? 0x30000 : 0x10000);
    r.Write32(Segment+52,r.c.count); r.Write32(Segment+28,r.c.maximum);
    r.Write32(Segment+56,r.c.empty_ranges ? 0 : Node+0x100);
    const std::array<std::uint32_t,3> bases={0x50000,0x80000,0xa0000};
    for (unsigned i=0;i<bases.size();++i) {
        auto node=Node+0x100+i*16;
        r.Write32(node,i+1<bases.size() ? node+16 : 0);
        r.Write32(node+4,bases[i]); r.Write32(node+8,0x10000);
    }
}
void Compare(const Case& c,unsigned ordinal) {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a && b,"independent guest windows");
    try {
        Run original{a,c},recovered{b,c}; Setup(original); std::memcpy(b,a,MemorySize);
        PPCContext ctx{}; ctx.r1.u64=StackTop; ctx.lr=CallerLR;
        std::array<PPCRegister*,4> regs={&ctx.r28,&ctx.r29,&ctx.r30,&ctx.r31};
        for (unsigned i=0;i<regs.size();++i) regs[i]->u64=0x1122334455667700ull+i;
        ctx.r3.u64=Segment; ctx.r4.u64=c.base; ctx.r5.u64=c.size;
        active=&original;
        if (c.kind==Kind::Allocate) oracle_AllocateRangeNode(ctx,a);
        else oracle_InsertRangeRecord(ctx,a);
        GuestMemory memory(0,{b,MemorySize}); Services services(recovered);
        if (c.kind==Kind::Allocate)
            Require(ctx.r3.u32==AllocateRangeNode(memory,services,Segment,Frame),"node return");
        else InsertRangeRecord(memory,services,Segment,c.base,c.size,Frame);
        Require(ctx.r1.u64==StackTop && ctx.lr==CallerLR,"original stack/LR");
        for (unsigned i=0;i<regs.size();++i)
            Require(regs[i]->u64==0x1122334455667700ull+i,"original nonvolatile GPR");
        Require(original.events==recovered.events,"VM calls and actual arguments");
        Require(original.snapshots==recovered.snapshots,"VM boundary memory snapshots");
        if (std::memcmp(a,b,ScratchStart)!=0) {
            for (std::size_t i=0;i<ScratchStart;++i) if (a[i]!=b[i]) {
                std::fprintf(stderr,"first byte %08zx PPC=%02x semantic=%02x\n",i,a[i],b[i]); break;
            }
            throw std::runtime_error("ordinary guest memory");
        }
        auto local=original.AllocatorFrame()+80;
        Require(std::memcmp(a+local,b+local,16)==0,"allocator frame locals");
    } catch (const std::exception& error) {
        std::fprintf(stderr,"case %u (%s): %s\n",ordinal,c.name.c_str(),error.what());
        VirtualFree(a,0,MEM_RELEASE); VirtualFree(b,0,MEM_RELEASE); throw;
    }
    VirtualFree(a,0,MEM_RELEASE); VirtualFree(b,0,MEM_RELEASE);
}
} // namespace
PPC_FUNC(sub_827CB498) { oracle_AllocateRangeNode(ctx,base); }
PPC_FUNC(__imp__NtAllocateVirtualMemory) {
    ctx.r3.s64=active->Allocate({ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32});
}
PPC_FUNC(__imp__NtFreeVirtualMemory) {
    ctx.r3.s64=active->Free({ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,0});
}
int main() {
    try {
        std::vector<Case> cases;
        auto add=[&](Case c,const char* name) { c.name=name; cases.push_back(c); };
        Case c; add(c,"reuse free range node");
        for (auto old_pool : {false,true}) for (auto full : {false,true}) {
            for (auto reserve : {-1,0,1}) for (auto commit : {-1,0,1}) {
                c=Case{}; c.free_node=false; c.old_pool=old_pool; c.full_pool=full;
                c.reserve_status=reserve; c.commit_status=commit;
                add(c,"pool reserve/commit/failure cleanup");
            }
        }
        for (auto old_pool : {false,true}) {
            c=Case{}; c.free_node=false; c.old_pool=old_pool;
            c.reserve_changes_size=true; c.commit_changes_size=true;
            add(c,"VM adjusts committed/reserved sizes");
            c.commit_changes_owner=true; add(c,"VM changes segment owner");
        }
        for (auto base : {0x30000u,0x40000u,0x60000u,0x65000u,0x70000u,0x90000u,0xb0000u,0xc0000u}) {
            for (auto size : {0x10000u,0x20000u,0x30000u}) {
                for (auto maximum : {0u,0x10000u,0x40000u}) {
                    c=Case{}; c.kind=Kind::Insert; c.base=base; c.size=size;
                    c.maximum=maximum; add(c,"range insertion/backward/forward/bridge merge");
                }
            }
        }
        c=Case{}; c.kind=Kind::Insert; c.empty_ranges=true; c.count=0xffffffffu;
        add(c,"empty range list and count wrap");
        c=Case{}; c.kind=Kind::Insert; c.count=0; c.size=0x20000;
        add(c,"merged count underflow");
        for (auto old_pool : {false,true}) for (auto commit : {-1,0}) {
            c=Case{}; c.kind=Kind::Insert; c.base=0xc0000; c.free_node=false;
            c.old_pool=old_pool; c.commit_status=commit;
            add(c,"insert composes node allocator VM paths");
        }
        std::array<unsigned,2> counts{};
        for (unsigned i=0;i<cases.size();++i) { Compare(cases[i],i); ++counts[unsigned(cases[i].kind)]; }
        std::printf("PASS 827CB498 %u\nPASS 827CB658 %u\n",counts[0],counts[1]); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"FAIL %s\n",error.what()); return 1; }
}
