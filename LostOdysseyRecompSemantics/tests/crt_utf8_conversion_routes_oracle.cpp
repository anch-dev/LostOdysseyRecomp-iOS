#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_thread_error_routes.h"
#include "lo_semantics/crt_utf8_conversion_routes.h"

namespace
{
namespace conversion = crt_utf8_conversion_routes;
constexpr GuestAddress Input=0x40000u,Output=0x42000u;

enum class ConversionMode
{AsciiCount,TwoByte,FourByteDirect,ShortCapacity,NativeSuccess,NativeFailure};
struct ConversionCase
{const char* name;GuestAddress address;ConversionMode mode;};
constexpr std::array ConversionCases{
    ConversionCase{"utf8-ascii-count-with-nul",0x8229c560u,ConversionMode::AsciiCount},
    ConversionCase{"utf8-two-byte-output",0x8229c560u,ConversionMode::TwoByte},
    ConversionCase{"utf8-four-byte-surrogate-direct",0x827ca660u,ConversionMode::FourByteDirect},
    ConversionCase{"utf8-capacity-error",0x8229c560u,ConversionMode::ShortCapacity},
    ConversionCase{"native-code-page-success",0x8229c560u,ConversionMode::NativeSuccess},
    ConversionCase{"native-code-page-status-error",0x8229c560u,ConversionMode::NativeFailure}};

using NativeEvent=std::array<std::uint64_t,7>;
struct NativeCallbacks final : conversion::NativeServices
{
    ConversionMode mode;
    std::vector<NativeEvent> events;
    explicit NativeCallbacks(ConversionMode selected):mode(selected){}
    void RtlMultiByteToUnicodeN(GuestMemory& memory,
        conversion::Registers& state) override
    {
        events.push_back({1u,state.sp,state.lr,state.r[3],state.r[4],
            state.r[6],state.r[7]});
        if(mode!=ConversionMode::NativeSuccess&&
            mode!=ConversionMode::NativeFailure)
            throw std::runtime_error("unexpected native conversion");
        if(mode==ConversionMode::NativeSuccess)
        {
            memory.WriteU16(Output,'A');
            memory.WriteU16(Output+2u,'B');
            memory.WriteU16(Output+4u,0);
            state.r[3]=0;
        }
        else
            state.r[3]=0xffffffffc0000017ull;
        state.r[11]=0x8877665500000011ull;
    }
    void RtlNtStatusToDosError(GuestMemory&,
        conversion::Registers& state) override
    {
        events.push_back({2u,state.sp,state.lr,state.r[3],state.r[4],
            state.r[6],state.r[7]});
        if(mode!=ConversionMode::NativeFailure||
            state.r[3]!=0xffffffffc0000017ull)
            throw std::runtime_error("unexpected status conversion");
        state.r[3]=5;
    }
};

struct UnusedTls final : crt_thread_error_routes::NativeServices
{
    void KeTlsGetValue(GuestMemory&,family::Registers&) override
    {throw std::runtime_error("unexpected TLS get");}
    void KeTlsSetValue(GuestMemory&,family::Registers&) override
    {throw std::runtime_error("unexpected TLS set");}
};

NativeCallbacks* active_native=nullptr;

void SeedConversion(GuestWindow& window,const ConversionCase& item)
{
    Seed(window,Mode::Binary);
    auto memory=window.Memory();
    // image_disc1.bin guest VA 0x831E7C90..0x831E7DC7, offset VA-0x82000000.
    // The 8 threshold words and 6 lead-byte masks below are exact image bytes.
    constexpr std::array<std::uint32_t,14> words{
        0x0000fffdu,0x0000ffffu,0x0010ffffu,0x0000000au,
        0x00010000u,0x000003ffu,0x0000d800u,0x0000dc00u,
        0u,0x00003080u,0x000e2080u,0x03c82080u,
        0xfa082080u,0x82082080u};
    for(std::size_t index=0;index<words.size();++index)
        memory.WriteU32(0x831e7c90u+static_cast<GuestAddress>(index*4u),words[index]);
    // Exact 256-byte image table: 00 x192, 01 x32, 02 x16,
    // 03 x8, 04 x4, 05 x4. Fill(0) supplied the first 192 bytes.
    for(unsigned byte=0xc0u;byte<=0xffu;++byte)
        memory.WriteU8(0x831e7cc8u+byte,
            byte<0xe0u?1u:byte<0xf0u?2u:byte<0xf8u?3u:
            byte<0xfcu?4u:5u);
    constexpr std::array<std::uint8_t,5> ascii{{'A',0,0,0,0}};
    constexpr std::array<std::uint8_t,5> two{{0xc2u,0xa2u,0,0,0}};
    constexpr std::array<std::uint8_t,5> four{{0xf0u,0x9fu,0x98u,0x80u,0}};
    constexpr std::array<std::uint8_t,5> short_input{{'A','B',0,0,0}};
    constexpr std::array<std::uint8_t,5> native{{'A','B',0,0,0}};
    const auto& input=item.mode==ConversionMode::AsciiCount?ascii:
        item.mode==ConversionMode::TwoByte?two:
        item.mode==ConversionMode::FourByteDirect?four:
        item.mode==ConversionMode::ShortCapacity?short_input:native;
    for(unsigned index=0;index<input.size();++index)
        memory.WriteU8(Input+index,input[index]);
}

PPCContext InitialConversion(const ConversionCase& item)
{
    PPCContext context{};
    context.r1.u64=0x1234567800000000ull|Stack;
    context.lr=0xabcdef0123456789ull;
    context.r13.u64=Environment;
    context.r20.u64=0x2011223344556677ull;
    context.r31.u64=0x3111223344556677ull;
    context.xer.so=1;context.xer.ca=1;
    const auto length=item.mode==ConversionMode::AsciiCount||
        item.mode==ConversionMode::NativeSuccess?UINT64_MAX:
        item.mode==ConversionMode::FourByteDirect?4u:2u;
    const auto capacity=item.mode==ConversionMode::AsciiCount?0u:
        item.mode==ConversionMode::ShortCapacity?1u:
        item.mode==ConversionMode::NativeSuccess?3u:2u;
    if(item.address==0x827ca660u)
    {
        context.r3.u64=0x1020304000000000ull|Input;
        context.r4.u64=length;
        context.r5.u64=0x9080706000000000ull|Output;
        context.r6.u64=capacity;
    }
    else
    {
        context.r3.u64=item.mode==ConversionMode::NativeSuccess||
            item.mode==ConversionMode::NativeFailure?1252u:
            0x112233440000fde9ull;
        context.r5.u64=0x1020304000000000ull|Input;
        context.r6.u64=length;
        context.r7.u64=0x9080706000000000ull|Output;
        context.r8.u64=capacity;
    }
    return context;
}

void CheckConversion(const ConversionCase& item)
{
    GuestWindow original(Regions),recovered(Regions);
    SeedConversion(original,item);SeedConversion(recovered,item);
    Services expected(original,Mode::Binary),actual(recovered,Mode::Binary);
    NativeCallbacks original_native(item.mode),recovered_native(item.mode);
    PPCContext context=InitialConversion(item);
    auto state=FromPpc(context);
    active=&expected;active_native=&original_native;
    if(item.address==0x827ca660u)
        __imp__sub_827CA660(context,original.Bytes());
    else
        __imp__sub_8229C560(context,original.Bytes());
    active_native=nullptr;active=nullptr;
    if(!conversion::Apply(item.address,actual.memory,recovered_native,state))
        throw std::runtime_error("missing conversion entry");
    if(!Same(FromPpc(context),state)||
        original_native.events!=recovered_native.events||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("selected context, native events or RAM mismatch");

    const auto memory=expected.memory;
    switch(item.mode)
    {
    case ConversionMode::AsciiCount:
        if(context.r3.u64!=2u||!original_native.events.empty())
            throw std::runtime_error("UTF8 count including terminator");
        break;
    case ConversionMode::TwoByte:
        if(context.r3.u64!=1u||memory.ReadU16(Output)!=0xa2u)
            throw std::runtime_error("UTF8 two-byte payload");
        break;
    case ConversionMode::FourByteDirect:
        if(context.r3.u64!=2u||memory.ReadU16(Output)!=0xd83du||
            memory.ReadU16(Output+2u)!=0xde00u)
            throw std::runtime_error("UTF8 four-byte surrogate pair");
        break;
    case ConversionMode::ShortCapacity:
        if(context.r3.u64!=0u||memory.ReadU16(Output)!='A'||
            memory.ReadU32(ErrorState+352u)!=122u)
            throw std::runtime_error("UTF8 capacity error store");
        break;
    case ConversionMode::NativeSuccess:
        if(context.r3.u64!=3u||original_native.events.size()!=1u||
            memory.ReadU16(Output+2u)!='B')
            throw std::runtime_error("native code-page success");
        break;
    case ConversionMode::NativeFailure:
        if(context.r3.u64!=0u||original_native.events.size()!=2u||
            memory.ReadU32(ErrorState+352u)!=5u)
            throw std::runtime_error("native status error store");
        break;
    }
}
} // namespace

void OriginalMultiByte(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active_native->RtlMultiByteToUnicodeN(active->memory,state);
    ToPpc(context,state);
}

void OriginalStatusError(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    active_native->RtlNtStatusToDosError(active->memory,state);
    ToPpc(context,state);
}

void OriginalStoreThreadError(PPCContext& context,std::uint8_t*)
{
    auto state=FromPpc(context);
    UnusedTls native;
    if(!crt_thread_error_routes::Apply(0x822ca180u,active->memory,native,state))
        throw std::runtime_error("missing accepted thread error tail");
    ToPpc(context,state);
}

int main()
{
    try
    {
        for(const auto& item:ConversionCases)
        {
            try{CheckConversion(item);}
            catch(const std::exception& error)
            {
                std::fprintf(stderr,"%s: %s\n",item.name,error.what());
                return 1;
            }
        }
        std::printf("PASS crt-utf8-conversion-routes %zu actual PPC cases\n",
            ConversionCases.size());
        std::puts("LIMIT exact image tables, selected native import context; faults, MMIO, concurrency and runtime open");
        return 0;
    }
    catch(const std::exception& error)
    {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
