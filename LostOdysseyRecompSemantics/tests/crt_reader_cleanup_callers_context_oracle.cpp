#pragma push_macro("main")
#undef main
#define main CleanupRecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_cleanup_callers_context.h"

namespace cleanup_oracle {
using Full = crt_close_recursive_buffer_context::Registers;
using Event = std::array<std::uint64_t, 9>;
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<Event> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Full& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.r[5],state.ctr,state.fpr_bits[7],state.r[10]});
        if (target != RecursiveTarget || events.size() > 6u)
            throw std::runtime_error("unexpected cleanup callback");
        memory.WriteU32(0x55000u + static_cast<GuestAddress>(4u * (events.size()-1u)),static_cast<std::uint32_t>(state.r[4]));
        state.r[3] = 0xaabbccdd01234567ull;
        state.r[10] ^= 0x9876543212345678ull;
        state.fpr_bits[7] ^= 0x180u;
        state.cr1.gt ^= 1u; state.cr7.eq ^= 1u; state.xer_ca ^= 1u;
    }
};
Guest* active_guest = nullptr;
void Seed(test::GuestWindow& window,bool live) {
    RecursiveSeed(window,RecursiveScenario::Empty);
    auto m=window.Memory();
    m.WriteU32(RecursiveVtable+12u,RecursiveTarget|3u);
    m.WriteU32(RecursiveReader+8u,live ? 0x43000u : 0u);
    m.WriteU32(RecursiveReader+12u,live ? 0x44000u : 0u);
    m.WriteU32(RecursiveReader+4u,live ? RecursiveOldNode : 0u);
    m.WriteU32(RecursiveOldNode+12u,live ? RecursiveNewNode : 0u);
    m.WriteU32(RecursiveNewNode,RecursiveNewBuffer);
    m.WriteU32(RecursiveNewNode+12u,0u);
}
void Check(GuestAddress entry,bool live) {
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    Seed(original,live); Seed(recovered,live);
    auto original_memory=original.Memory(); auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);
    const auto initial=crt_full_oracle::FromPpc(context);
    auto state=initial; Guest expected,actual;
    recursive_original_memory=&original_memory; active_guest=&expected;
    if (entry==0x82bd0b48u) __imp__sub_82BD0B48(context,original.Bytes());
    else __imp__sub_82BD0DF0(context,original.Bytes());
    recursive_original_memory=nullptr; active_guest=nullptr;
    if (!crt_reader_cleanup_callers_context::Apply(entry,recovered_memory,actual,state))
        throw std::runtime_error("missing cleanup entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events)
        throw std::runtime_error("cleanup Full72/RAM/callback mismatch");
    if (context.r3.u64 != initial.r[3] || context.r1.u64 != initial.r[1] || context.lr != 0x81234567u ||
        original_memory.ReadU32(Stack-128u) != static_cast<std::uint32_t>(initial.r[1]) ||
        original_memory.ReadU32(Stack-8u) != 0x81234567u)
        throw std::runtime_error("cleanup return/SP/backchain/LR absent");
    for (unsigned i=28u;i<=31u;++i)
        if (recovery_abi::ReadU64(original_memory,Stack-8u*(33u-i)) != initial.r[i])
            throw std::runtime_error("cleanup nonvolatile save absent");
    if (expected.events.size() != (live ? 6u : 0u))
        throw std::runtime_error("cleanup live path absent");
    if (live) {
        constexpr std::array<GuestAddress,6> released{0x43000u,0x44000u,RecursiveOldBuffer,RecursiveOldNode,RecursiveNewBuffer,RecursiveNewNode};
        constexpr std::array<GuestAddress,6> returns{0x82bd0b80u,0x82bd0ba8u,0x82bd0be4u,0x82bd0c00u,0x82bd0be4u,0x82bd0c00u};
        for (unsigned i=0;i!=6u;++i)
            if (expected.events[i][1] != initial.r[1]-128u || expected.events[i][2] != returns[i] || expected.events[i][4] != released[i])
                throw std::runtime_error("cleanup callback order/SP/LR absent");
        if (original_memory.ReadU32(RecursiveReader+8u) || original_memory.ReadU32(RecursiveReader+12u) ||
            original_memory.ReadU32(RecursiveOldNode) || original_memory.ReadU32(RecursiveNewNode))
            throw std::runtime_error("cleanup pointer clearing absent");
    }
}
}
void OriginalCleanupIndirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    cleanup_oracle::active_guest->CallIndirect(target,*recursive_original_memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    try {
        cleanup_oracle::Check(0x82bd0b48u,false);
        cleanup_oracle::Check(0x82bd0b48u,true);
        cleanup_oracle::Check(0x82bd0df0u,true);
        std::puts("PASS crt-reader-cleanup-callers-context 3 actual PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
