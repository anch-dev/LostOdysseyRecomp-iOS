// Appended after verbatim generated PPC bodies and actual ABI helpers.
#include "lo_semantics/heap_free.h"

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
constexpr GuestAddress Heap = 0x10000, Block = 0x100000, Existing = 0x0fffe0;
constexpr GuestAddress Descriptor = 0x310000, Sentinel = 0x320000;
constexpr GuestAddress StackTop = 0x3f0000, Frame = StackTop - 176;
constexpr std::uint32_t CallerLR = 0x81234567;

void Require(bool okay, const char* reason) { if (!okay) throw std::runtime_error(reason); }
enum class Kind { Free, Cleanup };
struct Case {
    Kind kind = Kind::Free;
    std::uint32_t units = 4, flags = 0, heap_flags = 0;
    bool null_payload = false, process_guard = false, process_mismatch = false;
    bool virtual_block = false, vm_writes = false, mutate_lock = false;
    bool process_mutates = false, bug_mutates = false, leave_mutates = false;
    bool vm_pre_mutates = false, vm_result_mutates = false;
    bool prev_free = false, next_free = false, compare_changes_size = false;
    bool compare_changes_mirror = false;
    bool decommit = false, large_existing = false;
    std::uint32_t prev_units = 2, next_units = 3;
    std::uint32_t threshold = 0xffffffffu, cap = 0xffffffffu;
    std::uint32_t initial_free_count = 0;
    std::int32_t vm_status = 0;
    std::uint32_t lock_owned = 0;
    std::uint8_t block_flags = 0;
    std::string name;
};
struct Event {
    std::string name;
    std::array<std::uint32_t, 5> args{};
    bool operator==(const Event&) const = default;
};
struct Run {
    std::uint8_t* bytes;
    const Case& scenario;
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
        snapshots.back().insert(snapshots.back().end(), bytes + Frame + 80, bytes + Frame + 104);
    }
    void Record(const char* name, std::array<std::uint32_t, 5> args) {
        Snapshot(); events.push_back({name, args});
        if (scenario.mutate_lock && std::strcmp(name, "enter") == 0)
            Write32(Heap + 1408, Sentinel + 4);
        if (scenario.process_mutates && std::strcmp(name, "process") == 0) {
            bytes[Heap + 379] = 3;
            Write32(Frame + 168, 0x99887766);
        }
        if (scenario.bug_mutates && std::strcmp(name, "bug") == 0)
            Write32(Frame + 88, 0);
        if (scenario.compare_changes_size && std::strcmp(name, "compare") == 0)
            Write32(Frame + 80, Read32(Frame + 80) + 1);
        if (scenario.compare_changes_mirror && std::strcmp(name, "compare") == 0)
            Write32(Frame + 84, 0);
        if (scenario.leave_mutates && std::strcmp(name, "leave") == 0) {
            Write32(Heap + 1408, Sentinel + 8);
            Write32(Frame + 88, 0xabcd1234);
        }
        if (scenario.vm_pre_mutates && std::strcmp(name, "leave") == 0) {
            Write32(Frame + 96, Sentinel + 12);
            Write32(Frame + 80, 0x98765432);
        }
        if (scenario.vm_writes && std::strcmp(name, "vm") == 0) {
            Write32(Frame + 96, Sentinel + 8);
            Write32(Frame + 80, 0x1234);
        }
        if (scenario.vm_result_mutates && std::strcmp(name, "vm") == 0)
            Write32(Frame + 88, 0x76543210);
        Snapshot();
    }
};
Run* active = nullptr;
struct Services final : HeapFreeServices {
    Run& run;
    explicit Services(Run& r) : run(r) {}
    std::uint32_t CompareMemoryUlong(GuestAddress s, std::uint32_t n, std::uint32_t v) override {
        run.Record("compare", {s,n,v,0,0}); return 0x12345678;
    }
    std::uint32_t GetCurrentProcessType() override {
        run.Record("process", {0,0,0,0,0}); return run.scenario.process_mismatch ? 2 : 1;
    }
    void BugCheck(std::uint32_t code, GuestAddress heap, GuestAddress lr,
                  std::uint32_t line, GuestAddress payload) override {
        run.Record("bug", {code,heap,lr,line,payload});
    }
    void EnterCriticalSection(GuestAddress address) override {
        run.Record("enter", {address,0,0,0,0});
    }
    void LeaveCriticalSection(GuestAddress address) override {
        run.Record("leave", {address,0,0,0,0});
    }
    std::int32_t FreeVirtualMemory(GuestAddress base_out, GuestAddress size_out,
                                   std::uint32_t type, std::uint32_t zero) override {
        run.Record("vm", {base_out,size_out,type,zero,0}); return run.scenario.vm_status;
    }
    void DecommitFreeBlock(GuestAddress heap, GuestAddress block, std::uint32_t units) override {
        run.Record("decommit", {heap,block,units,0,0});
    }
};
GuestAddress Head(std::uint32_t size) { return Heap + (size + 48) * 8; }
void Header(Run& run, GuestAddress address, std::uint16_t size,
            std::uint16_t previous, std::uint8_t flags) {
    run.Write16(address, size); run.Write16(address+2, previous);
    run.bytes[address+4] = 0; run.bytes[address+5] = flags;
}
void LinkSingleton(Run& run, GuestAddress block, std::uint32_t size) {
    const auto head = Head(size < 128 ? size : 0), node = block + 8;
    Require(run.Read32(head) == head, "empty fixture list");
    run.Write32(head,node); run.Write32(head+4,node);
    run.Write32(node,head); run.Write32(node+4,head);
    if (size < 128) {
        const auto word = Heap + ((size >> 5) + 88) * 4;
        run.Write32(word, run.Read32(word) | (1u << (size & 31)));
    }
    run.Write32(Heap+48,run.Read32(Heap+48)+size);
}
void Setup(Run& run) {
    const Case& c = run.scenario;
    std::memset(run.bytes, 0xa5, MemorySize);
    for (unsigned size=0; size<128; ++size) {
        auto head=Head(size); run.Write32(head,head); run.Write32(head+4,head);
    }
    for (unsigned i=0;i<4;++i) run.Write32(Heap+(88+i)*4,0);
    run.Write32(Heap+48,c.initial_free_count); run.Write32(Heap+20,c.process_guard ? 0x40000 : 0);
    run.Write32(Heap+24,c.heap_flags); run.Write32(Heap+40,c.threshold);
    run.Write32(Heap+44,c.cap); run.Write32(Heap+1408,Sentinel);
    run.bytes[Heap+379]=1;
    run.Write32(Heap+96,Descriptor); run.Write32(Descriptor+44,0x300000);
    run.Write32(Descriptor+64,Block);
    run.Write32(Frame+168,CallerLR);
    if (c.kind==Kind::Cleanup || c.null_payload) return;
    Header(run,Block,static_cast<std::uint16_t>(c.units),
           c.prev_free ? static_cast<std::uint16_t>(c.prev_units) : 0,
           c.virtual_block ? 8 : c.block_flags);
    if (c.virtual_block) {
        const auto vm=Block-32;
        run.Write32(vm,Sentinel); run.Write32(vm+4,Sentinel);
        run.Write32(Sentinel,vm); run.Write32(Sentinel+4,vm);
        return;
    }
    if (c.prev_free) {
        auto previous=Block-c.prev_units*16;
        Header(run,previous,static_cast<std::uint16_t>(c.prev_units),0,
               c.compare_changes_size ? 4 : 0);
        LinkSingleton(run,previous,c.prev_units);
    }
    auto next=Block+c.units*16;
    Header(run,next,static_cast<std::uint16_t>(c.next_units),
           static_cast<std::uint16_t>(c.units),c.next_free ? 0 : 1);
    if (c.next_free) LinkSingleton(run,next,c.next_units);
    Header(run,next+c.next_units*16,1,static_cast<std::uint16_t>(c.next_units),1);
    if (c.large_existing) {
        Header(run,Existing,130,0,0);
        LinkSingleton(run,Existing,130);
    }
}
void Compare(const Case& c, unsigned ordinal) {
    auto* a=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* b=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(a && b,"allocate independent guest windows");
    try {
        Run original{a,c}, recovered{b,c}; Setup(original);
        std::memcpy(b,a,MemorySize);
        PPCContext ctx{};
        ctx.r1.u64=StackTop; ctx.lr=CallerLR;
        constexpr std::array<std::uint64_t,7> saved={
            0x1122334455667788ull,0x2233445566778899ull,0x33445566778899aaull,
            0x445566778899aabbull,0x5566778899aabbccull,0x66778899aabbccddull,
            0x778899aabbccddeeull};
        ctx.r25.u64=saved[0];ctx.r26.u64=saved[1];ctx.r27.u64=saved[2];
        ctx.r28.u64=saved[3];ctx.r29.u64=saved[4];ctx.r30.u64=saved[5];ctx.r31.u64=saved[6];
        if (c.kind==Kind::Free) {
            ctx.r3.u64=Heap;ctx.r4.u64=c.flags;ctx.r5.u64=c.null_payload ? 0 : Block+16;
        } else {
            ctx.r12.u64=StackTop;ctx.r30.u64=Heap;ctx.r25.u64=c.lock_owned;
        }
        std::uint64_t expected25=ctx.r25.u64, expected30=ctx.r30.u64;
        active=&original;
        if (c.kind==Kind::Free) oracle_FreeHeapBlock(ctx,a);
        else oracle_LeaveHeapCriticalSection(ctx,a);
        active=nullptr;
        Require(ctx.r1.u64==StackTop && ctx.lr==(c.process_mutates ? 0x99887766u : CallerLR),
                "original stack/LR restored from guest save slot");
        Require(ctx.r25.u64==expected25 && ctx.r26.u64==saved[1] && ctx.r27.u64==saved[2] &&
                ctx.r28.u64==saved[3] && ctx.r29.u64==saved[4] && ctx.r30.u64==expected30 &&
                ctx.r31.u64==saved[6],"original nonvolatile registers restored");
        GuestMemory memory(0,{b,MemorySize}); Services services(recovered);
        std::uint32_t result=0;
        if (c.kind==Kind::Free)
            result=FreeHeapBlock(memory,services,Heap,c.flags,c.null_payload?0:Block+16,Frame);
        else LeaveHeapCriticalSection(memory,services,Heap,c.lock_owned);
        bool same=original.events==recovered.events && original.snapshots==recovered.snapshots &&
                  std::equal(a,a+ScratchStart,b) &&
                  std::equal(a+Frame+80,a+Frame+104,b+Frame+80);
        if (c.kind==Kind::Free) same=same && ctx.r3.u32==result;
        if (!same) {
            std::fprintf(stderr,"FAIL %s #%u %s return=%08x/%08x events=%zu/%zu\n",
                         c.kind==Kind::Free?"free":"cleanup",ordinal,c.name.c_str(),
                         ctx.r3.u32,result,original.events.size(),recovered.events.size());
            for (std::size_t i=0;i<(std::min)(original.events.size(),recovered.events.size());++i)
                if (!(original.events[i]==recovered.events[i]))
                    std::fprintf(stderr,"event %zu %s/%s\n",i,original.events[i].name.c_str(),recovered.events[i].name.c_str());
            for (unsigned i=80;i<104;i+=4)
                if (original.Read32(Frame+i)!=recovered.Read32(Frame+i))
                    std::fprintf(stderr,"frame+%u %08x/%08x\n",i,original.Read32(Frame+i),recovered.Read32(Frame+i));
            throw std::runtime_error("PPC/recovered heap free mismatch");
        }
    } catch (...) {
        active=nullptr;VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);throw;
    }
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
}
} // namespace

PPC_FUNC(sub_823AE108) { oracle_CoalesceFreeBlocks(ctx,base); }
PPC_FUNC(sub_827CBA60) { oracle_InsertFreeBlocks(ctx,base); }
PPC_FUNC(sub_823AE0BC) { oracle_LeaveHeapCriticalSection(ctx,base); }
PPC_FUNC(__imp__KeGetCurrentProcessType) { ctx.r3.u64=active->scenario.process_mismatch?2:1; active->Record("process",{0,0,0,0,0}); }
PPC_FUNC(__imp__KeBugCheckEx) { active->Record("bug",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32}); }
PPC_FUNC(__imp__RtlEnterCriticalSection) { active->Record("enter",{ctx.r3.u32,0,0,0,0}); }
PPC_FUNC(__imp__RtlLeaveCriticalSection) { active->Record("leave",{ctx.r3.u32,0,0,0,0}); }
PPC_FUNC(__imp__NtFreeVirtualMemory) {
    active->Record("vm",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,0});
    ctx.r3.s64=active->scenario.vm_status;
}
PPC_FUNC(__imp__RtlCompareMemoryUlong) {
    active->Record("compare",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,0,0});
    ctx.r3.u64=0x12345678;
}
PPC_FUNC(sub_827CC668) { active->Record("decommit",{ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,0,0}); }

int main() {
    try {
        unsigned free_count=0,cleanup_count=0;
    auto test=[&](Case c) { Compare(c,c.kind==Kind::Free?free_count++:cleanup_count++); };
        Case c; c.name="null";c.null_payload=true;test(c);
        c.process_guard=true;c.process_mismatch=true;c.name="guard null bugcheck";test(c);
        c.process_mismatch=false;c.name="guard same process";test(c);
        c={};c.null_payload=true;c.process_guard=true;c.process_mismatch=true;
        c.process_mutates=true;c.name="process callback changes guard and saved LR";test(c);
        c={};c.null_payload=true;c.process_guard=true;c.process_mismatch=true;
        c.bug_mutates=true;c.name="bug callback changes result but null returns one";test(c);
        for (std::uint32_t units:{1u,2u,31u,32u,127u,128u,0xf000u,0xf001u}) {
            c={};c.units=units;c.name="size boundary";test(c);
        }
        c={};c.units=4;c.heap_flags=1;c.name="heap no lock";test(c);
        c.flags=1;c.heap_flags=0;c.name="caller no lock";test(c);
        c={};c.mutate_lock=true;c.name="lock pointer reread";test(c);
        c={};c.leave_mutates=true;c.name="cleanup leave callback changes result";test(c);
        c={};c.prev_free=true;c.next_free=true;c.name="both neighbors merge";test(c);
        c={};c.prev_free=true;c.compare_changes_size=true;c.name="debug callback changes frame size";test(c);
        c={};c.prev_free=true;c.compare_changes_mirror=true;
        c.name="debug callback changes lock mirror";test(c);
        c={};c.next_free=true;c.name="next merge";test(c);
        c={};c.units=150;c.large_existing=true;c.name="ordered large insert";test(c);
        c={};c.units=0xf001;c.name="large split insertion";test(c);
        c={};c.units=128;c.threshold=128;c.cap=128;c.name="decommit threshold";test(c);
        c={};c.prev_free=true;c.next_free=true;c.threshold=9;c.cap=9;
        c.name="decommit after both-neighbor coalesce";test(c);
        c={};c.units=128;c.threshold=128;c.cap=0x70;
        c.initial_free_count=0xfffffff0u;c.name="decommit threshold sum wraps";test(c);
        for (auto status:{0,1,-1,(std::numeric_limits<std::int32_t>::min)(),
                         (std::numeric_limits<std::int32_t>::max)()}) {
            c={};c.virtual_block=true;c.vm_status=status;c.vm_writes=true;
            c.name="virtual release and output writes";test(c);
        }
        c={};c.virtual_block=true;c.vm_pre_mutates=true;c.vm_result_mutates=true;
        c.name="leave changes VM locals then VM changes result";test(c);
        c={};c.virtual_block=true;c.vm_result_mutates=true;c.vm_status=-1;
        c.name="negative VM status overrides callback result";test(c);
        c={};c.virtual_block=true;c.heap_flags=1;c.name="virtual release without lock";test(c);
        c={};c.units=128;c.threshold=128;c.cap=0xffffffffu;
        c.name="threshold without capacity leaves ordinary insertion";test(c);
        for (std::uint32_t lock:{0u,1u,0xffffffffu}) {
            c={};c.kind=Kind::Cleanup;c.lock_owned=lock;c.name="standalone cleanup";test(c);
        }
        c={};c.kind=Kind::Cleanup;c.lock_owned=1;c.leave_mutates=true;
        c.name="standalone cleanup callback side effect";test(c);
        std::printf("PASS 823ADE28 %u\n",free_count);
        std::printf("PASS 823AE0BC %u\n",cleanup_count);
        std::puts("LIMIT: synthetic kernel, decommit and compare callbacks; no exception landing pad, concurrent/MMIO effects or game runtime");
        return 0;
    } catch (const std::exception& e) {
        active=nullptr;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;
    }
}
