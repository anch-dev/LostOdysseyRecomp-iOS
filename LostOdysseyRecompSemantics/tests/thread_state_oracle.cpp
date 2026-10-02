// Appended after verbatim original PPC leaf and forwarding wrapper.
#include "lo_semantics/thread_state.h"

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
constexpr std::size_t MemorySize = 0x20000;
constexpr GuestAddress Thread = 0x1000, Target = 0x3000;
constexpr std::uint64_t CallerLR = 0x82345678;
void Require(bool okay, const char* reason) { if (!okay) throw std::runtime_error(reason); }
struct Case {
    std::uint32_t guard, code;
    GuestAddress target = Target;
    bool alias_guard = false;
    bool invalid_target = false;
    std::uint32_t thread_address = Thread;
    std::string name;
};
struct Window {
    std::uint8_t* bytes;
    Window() : bytes(static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,MemorySize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))) {
        Require(bytes != nullptr,"allocate independent guest window");
    }
    ~Window() { VirtualFree(bytes,0,MEM_RELEASE); }
    Window(const Window&)=delete;
    Window& operator=(const Window&)=delete;
};
void Write32(std::uint8_t* base, GuestAddress address, std::uint32_t value) {
    base[address]=std::uint8_t(value>>24);base[address+1]=std::uint8_t(value>>16);
    base[address+2]=std::uint8_t(value>>8);base[address+3]=std::uint8_t(value);
}
std::uint32_t Read32(const std::uint8_t* base, GuestAddress address) {
    return (std::uint32_t(base[address])<<24)|(std::uint32_t(base[address+1])<<16)|
           (std::uint32_t(base[address+2])<<8)|base[address+3];
}
void Compare(const Case& c, bool wrapper, unsigned ordinal) {
    Window original, recovered;
    std::memset(original.bytes,0xa5,MemorySize);
    const auto thread = c.thread_address;
    const auto target = c.alias_guard ? thread-16 : c.target;
    Write32(original.bytes,thread+336,c.guard);
    Write32(original.bytes,thread+256,c.invalid_target ? 0xffffff00u : target);
    if (!c.invalid_target && !c.alias_guard)
        Write32(original.bytes,target+352,0x87654321);
    std::memcpy(recovered.bytes,original.bytes,MemorySize);
    PPCContext ctx{};
    const std::uint64_t r3=0x1234567800000000ull|c.code;
    const std::uint64_t r13=0x8765432100000000ull|thread;
    ctx.r3.u64=r3;ctx.r13.u64=r13;ctx.r1.u64=0x1f000;ctx.lr=CallerLR;
    constexpr std::array<std::uint64_t,7> saved={
        0x1122334455667788ull,0x2233445566778899ull,0x33445566778899aaull,
        0x445566778899aabbull,0x5566778899aabbccull,0x66778899aabbccddull,
        0x778899aabbccddeeull};
    ctx.r25.u64=saved[0];ctx.r26.u64=saved[1];ctx.r27.u64=saved[2];
    ctx.r28.u64=saved[3];ctx.r29.u64=saved[4];ctx.r30.u64=saved[5];ctx.r31.u64=saved[6];
    if (wrapper) oracle_ReportAllocationFailure(ctx,original.bytes);
    else oracle_StoreThreadFailureCode(ctx,original.bytes);
    Require(ctx.r3.u64==r3 && ctx.r13.u64==r13 && ctx.r1.u64==0x1f000 && ctx.lr==CallerLR,
            "PPC r3/r13/stack/LR preserved");
    Require(ctx.r25.u64==saved[0] && ctx.r26.u64==saved[1] && ctx.r27.u64==saved[2] &&
            ctx.r28.u64==saved[3] && ctx.r29.u64==saved[4] && ctx.r30.u64==saved[5] &&
            ctx.r31.u64==saved[6],"PPC nonvolatile registers preserved");
    GuestMemory memory(0,{recovered.bytes,MemorySize});
    if (wrapper) ReportAllocationFailure(memory,thread,c.code);
    else StoreThreadFailureCode(memory,thread,c.code);
    if (!std::equal(original.bytes,original.bytes+MemorySize,recovered.bytes)) {
        GuestAddress first=0;
        while (first<MemorySize && original.bytes[first]==recovered.bytes[first]) ++first;
        std::fprintf(stderr,"FAIL %s #%u %s at %08x original=%02x recovered=%02x\n",
                     wrapper?"822CA180":"822CA188",ordinal,c.name.c_str(),first,
                     first<MemorySize?original.bytes[first]:0,
                     first<MemorySize?recovered.bytes[first]:0);
        throw std::runtime_error("PPC/recovered thread state differs");
    }
    if (c.guard==0)
        Require(Read32(original.bytes,target+352)==c.code,"enabled PPC writes low32 code");
    else
        Require(Read32(original.bytes,thread+336)==c.guard,"disabled PPC retains guard");
}
} // namespace

PPC_FUNC(sub_822CA188) { oracle_StoreThreadFailureCode(ctx,base); }

int main() {
    try {
        const std::vector<Case> cases={
            {0,0,Target,false,false,Thread,"enabled zero code"},
            {0,1,Target,false,false,Thread,"enabled one code"},
            {0,0xffffffffu,Target,false,false,Thread,"enabled all bits"},
            {0,0x80000000u,Target,false,false,Thread,"enabled sign bit"},
            {0,0x12345678u,Target,false,false,Thread,"enabled mixed code"},
            {1,0x12,Target,false,false,Thread,"guard one"},
            {0xffffffffu,0x12345678u,Target,false,false,Thread,"guard all bits"},
            {0x80000000u,0xabcdef01u,Target,false,false,Thread,"guard high bit"},
            {1,0x55555555u,Target,false,true,Thread,"disabled invalid target"},
            {0xffffffffu,0xaaaaaaaau,Target,false,true,Thread,"disabled invalid target all bits"},
            {0,0x37,Target,true,false,Thread,"target write aliases guard"},
            {0,0x87654321u,0x7000,false,false,0x4000,"different low guest addresses"},
        };
        for (std::size_t i=0;i<cases.size();++i) {
            Compare(cases[i],false,static_cast<unsigned>(i));
            Compare(cases[i],true,static_cast<unsigned>(i));
        }
        std::printf("PASS 822CA188 %zu\n",cases.size());
        std::printf("PASS 822CA180 %zu\n",cases.size());
        std::puts("LIMIT: bounded ordinary guest memory; no thread-local runtime or MMIO/concurrent effects");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;
    }
}
