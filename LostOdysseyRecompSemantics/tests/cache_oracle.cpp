// Appended to the original PPC control flow with dcbf/sync trace instrumentation.
#include "lo_semantics/cache.h"

#include <array>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using namespace lo::semantic::gpu;
struct Event {
    char operation;
    GuestAddress address;
    bool operator==(const Event&) const = default;
};
struct PrefixLimit {};
struct Trace {
    std::vector<Event> events;
    unsigned limit = 0;
    void Flush(GuestAddress address) {
        events.push_back({'F', address});
        if (limit && events.size() == limit) throw PrefixLimit{};
    }
    void Sync() { events.push_back({'S', 0}); }
};
Trace* active_trace = nullptr;
class Services final : public CacheServices {
public:
    explicit Services(Trace& trace) : trace_(trace) {}
    void FlushLine(GuestAddress address) override { trace_.Flush(address); }
    void Sync() override { trace_.Sync(); }
private:
    Trace& trace_;
};
void Compare(GuestAddress begin, GuestAddress end, unsigned limit = 0) {
    Trace oracle{{}, limit}, recovered{{}, limit};
    PPCContext context{};
    context.r3.u64 = begin;
    context.r4.u64 = end;
    context.r5.u64 = 0xa5a55a5a; // Ignored third original argument.
    std::array<std::uint8_t, 1> unused_memory{};
    bool oracle_stopped = false, recovered_stopped = false;
    active_trace = &oracle;
    try { oracle_FlushDataCacheRange(context, unused_memory.data()); }
    catch (const PrefixLimit&) { oracle_stopped = true; }
    active_trace = nullptr;
    Services services(recovered);
    try { FlushDataCacheRange(services, begin, end); }
    catch (const PrefixLimit&) { recovered_stopped = true; }
    if (oracle.events != recovered.events || oracle_stopped != recovered_stopped) {
        std::fprintf(stderr, "cache mismatch begin=%08x end=%08x limit=%u\n", begin, end, limit);
        throw std::runtime_error("cache instruction trace mismatch");
    }
    if (limit && !oracle_stopped) throw std::runtime_error("expected bounded prefix was not reached");
}
}

static void TraceFlush(std::uint32_t address) { active_trace->Flush(address); }
static void TraceSync() { active_trace->Sync(); }

int main() {
    try {
        const std::array<std::array<GuestAddress, 2>, 16> cases{{
            {0, 0}, {0, 1}, {127, 128}, {128, 129}, {0x100, 0x180},
            {0x100, 0x500}, {0x100, 0x580}, {0x100, 0x1000},
            {0x7f0fffff, 0x7f100001}, {0x7f100000, 0x7f100010},
            {0x86ffffff, 0x87000001}, {0x87000000, 0x87000081},
            {0xfffffff0, 0xffffffff}, {0xfffffffe, 0x100},
            {0x7f100001, 0}, {0x86fffffe, 0xffffffff},
        }};
        unsigned count = 0;
        for (const auto& item : cases) { Compare(item[0], item[1]); ++count; }
        std::mt19937 random(0x823ea178);
        for (unsigned i = 0; i < 1024; ++i) {
            const auto begin = random();
            Compare(begin, begin + random() % 4097);
            ++count;
        }
        // Reversed/huge signed spans request enormous unsigned loops in PPC.
        // Validate prefixes without pretending to have executed their full range.
        Compare(0x1000, 0, 32); ++count;
        Compare(0, 0x80000000, 32); ++count;
        std::printf("PASS: %u cache trace cases (1040 complete traces, 2 bounded prefixes)\n", count);
        std::puts("LIMIT: dcbf/sync emitted as trace events; no hardware cache effects or runtime adapter tested");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
