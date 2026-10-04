#pragma push_macro("main")
#undef main
#define main Upper61ReaderFixtureMain
#include "crt_close_reader_callers_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_close_upper61.h"
namespace upper61_oracle {
using namespace reader_callers_oracle;
void Check(const reader_callers_oracle::Case& item)
{
    GuestWindow original(ReaderRegions), recovered(ReaderRegions);
    Seed(original);
    Seed(recovered);
    Services expected(original, Mode::LockedWrite);
    Services actual(recovered, Mode::LockedWrite);
    IndexService expected_index, actual_index;
    Host expected_host(Scenario::BinarySuccess);
    Host actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock, actual_unlock;
    close_shared_oracle::SharedExtra expected_extra, actual_extra;
    Guest expected_guest(item.route), actual_guest(item.route);
    auto context = Initial(item.route);
    context.r4.u64 = 0xfeedface00000003ull;
    const auto initial_sp = context.r1.u64;
    const auto initial_r31 = context.r31.u64;
    auto state = crt_full_oracle::FromPpc(context);
    current = &expected;
    active = &expected;
    current_index = &expected_index;
    current_host = &expected_host;
    wrapper_oracle::original_unlock = &expected_unlock;
    close_shared_oracle::active_extra = &expected_extra;
    original_extra = &expected_extra;
    original_guest = &expected_guest;
    __imp__sub_82BD0CD0(context, original.Bytes());
    current = nullptr;
    active = nullptr;
    current_index = nullptr;
    current_host = nullptr;
    wrapper_oracle::original_unlock = nullptr;
    close_shared_oracle::active_extra = nullptr;
    original_extra = nullptr;
    original_guest = nullptr;
    if (!crt_close_upper61::Apply(0x82bd0cd0u, actual.memory,
            Deps(actual, actual_index, actual_host, actual_unlock,
                actual_extra, actual_guest), state))
        throw std::runtime_error("missing selected reader caller");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    if (before != after || !original.EqualCommitted(recovered) ||
        expected.events != actual.events ||
        expected.traps != actual.traps ||
        expected_index.events != actual_index.events ||
        expected_host.events != actual_host.events ||
        expected_unlock.events != actual_unlock.events ||
        expected_extra.events != actual_extra.events ||
        expected_guest.events != actual_guest.events)
    {
        for (unsigned i = 0; i < before.size(); ++i)
            if (before[i] != after[i])
                std::fprintf(stderr, "%s state[%u] %llx/%llx\n", item.name,
                    i, static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("reader caller full state/RAM/callback mismatch");
    }
    if (expected_guest.events.size() != 2u || context.r3.u64 != (0xaabbccdd00000000ull | Reader) ||
        context.r1.u64 != initial_sp || context.r31.u64 != initial_r31 || context.lr != 0x87654321u ||
        expected.memory.ReadU32(Stack - 96u) != static_cast<std::uint32_t>(initial_sp) ||
        expected.memory.ReadU32(Stack - 8u) != 0x87654321u ||
        recovery_abi::ReadU64(expected.memory,Stack - 16u) != initial_r31 ||
        expected_guest.events[0][1] != initial_sp - 240u ||
        expected_guest.events[0][2] != 0x82bd0a9cu ||
        expected.memory.ReadU32(Reader + 8u) != 0u || expected.memory.ReadU8(Reader + 24u) != 0u)
        throw std::runtime_error("constructor LR/save/backchain/live callback path absent");
    const auto node = item.route == Route::CopySuccess ? NewNode : OldNode;
    if (expected.memory.ReadU32(Reader) != node || expected.memory.ReadU32(node + 4u) != 3u)
        throw std::runtime_error("constructor allocation path absent");
}

}
int main() {
    for (auto route : {reader_callers_oracle::Route::CopySuccess,reader_callers_oracle::Route::SecondAllocationFailure}) {
        try { upper61_oracle::Check({"fixed-reader-constructor",0x82bd0cd0u,route}); }
        catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
    }
    std::puts("PASS crt-close-upper61 2 focused actual PPC cases"); return 0;
}
