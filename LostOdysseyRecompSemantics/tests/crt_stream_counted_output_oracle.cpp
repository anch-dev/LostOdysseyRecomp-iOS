#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_stream_counted_output.h"

namespace
{
enum class OutputMode {CountOnly,Buffered,Flush,Error};
struct OutputCase {const char* name;OutputMode mode;};
constexpr std::array OutputCases{
    OutputCase{"count-only-overflow",OutputMode::CountOnly},
    OutputCase{"buffered-high-byte",OutputMode::Buffered},
    OutputCase{"real-stream-flush",OutputMode::Flush},
    OutputCase{"error-after-native-write",OutputMode::Error}};
constexpr GuestAddress OutputCount=0x58000u;
bool CheckOutput(const OutputCase& item)
{
    const auto mode=item.mode==OutputMode::Error?Mode::NativeFailureFive:
        item.mode==OutputMode::Flush?Mode::FlushChar:Mode::BufferedChar;
    GuestWindow original(Regions),recovered(Regions);
    Seed(original,mode);Seed(recovered,mode);
    auto configure=[&](GuestWindow& window)
    {
        auto m=window.Memory();m.WriteU32(OutputCount,0xffffffffu);
        m.WriteU32(Stream+4u,item.mode==OutputMode::Buffered?1u:0u);
        if(item.mode==OutputMode::CountOnly)
        {m.WriteU32(Stream+12u,0x40u);m.WriteU32(Stream+8u,0u);}
        if(item.mode==OutputMode::Error)m.WriteU32(Stream,Buffer+3u);
    };
    configure(original);configure(recovered);
    Services expected(original,mode),actual(recovered,mode);
    PPCContext c{};
    c.r1.u64=0x1234567800000000ull|Stack;c.lr=0xabcdef0123456789ull;
    c.r3.u64=0x88776655443322e1ull;c.r4.u64=Stream;
    c.r5.u64=OutputCount;c.r13.u64=Environment;
    c.r31.u64=0x123456789abcdef0ull;c.xer.so=1;
    auto state=FromPpc(c);active=&expected;
    __imp__sub_82B82728(c,original.Bytes());active=nullptr;
    if(!crt_stream_counted_output::Apply(0x82b82728u,actual.memory,
            Dependencies(actual),state))throw std::runtime_error("missing output entry");
    if(!Same(FromPpc(c),state)||expected.events!=actual.events||
        !original.EqualCommitted(recovered))throw std::runtime_error(item.name);
    const auto count=expected.memory.ReadU32(OutputCount);
    if(count!=(item.mode==OutputMode::Error?0xffffffffu:0u))
        throw std::runtime_error("independent caller output count");
    if(item.mode==OutputMode::Buffered&&expected.memory.ReadU8(Buffer)!=0xe1u)
        throw std::runtime_error("independent emitted byte");
    if(item.mode==OutputMode::Flush&&
        !(expected.writes==1u&&expected.locks==1u&&expected.unlocks==1u))
        throw std::runtime_error("selected composed stream path");
    return true;
}
}
int main()
{
    try
    {
        for(const auto& item:OutputCases)CheckOutput(item);
        std::printf("PASS crt-stream-counted-output %zu actual PPC cases\n",OutputCases.size());
        std::puts("LIMIT selected output/flush chain; accepted lower ABI and kernel callbacks, faults/MMIO/concurrency/runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
