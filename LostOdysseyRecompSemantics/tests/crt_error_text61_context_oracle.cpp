#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_error_text61_context.h"
namespace error_text61_oracle
{
namespace family=lo::semantic::gpu::crt_error_text61_context;
constexpr test::Region Regions[]={{0u,0x100000u},{0x8321f000u,0x1000u}};
constexpr GuestAddress CountAddress=0x8321ffb8u,TableAddress=0x8321ff08u;
constexpr GuestAddress GoodText=0x55000u,FallbackText=0x55100u;
struct Case {const char* name;std::uint64_t input;GuestAddress expected;};
constexpr Case Cases[]={{"valid-error-index-high64",0xaabbccdd00000002ull,GoodText},
    {"negative-error-index-fallback",0x99887766ffffffffull,FallbackText},
    {"equal-count-error-index-fallback",0x8877665500000003ull,FallbackText}};
void Seed(test::GuestWindow& window)
{
    window.Fill(0u);auto memory=window.Memory();
    memory.WriteU32(CountAddress,3u);
    memory.WriteU32(TableAddress,GoodText);memory.WriteU32(TableAddress+4u,GoodText);
    memory.WriteU32(TableAddress+8u,GoodText);memory.WriteU32(TableAddress+12u,FallbackText);
    memory.WriteU8(GoodText,'G');memory.WriteU8(GoodText+1u,0u);
    memory.WriteU8(FallbackText,'?');memory.WriteU8(FallbackText+1u,0u);
}
PPCContext Initial(std::uint64_t input)
{
    PPCContext context{};auto gprs=crt_full_oracle::Gprs(context);
    for(unsigned i=0;i<32u;++i)gprs[i]->u64=0x1122334400000000ull+i;
    auto fprs=crt_full_oracle::Fprs(context);
    for(unsigned i=0;i<32u;++i)fprs[i]->u64=0x3ff0000000000000ull+i;
    context.r1.u64=0x1234567800080000ull;context.r3.u64=input;
    context.lr=0xaabbccdd12345678ull;context.ctr.u64=0x8877665500002403ull;
    context.xer.so=1;context.xer.ca=1;context.cr0.gt=1;context.cr1.gt=1;
    context.cr6.eq=1;context.cr7.lt=1;context.fpscr.csr=0x1f80u;
    return context;
}
void Check(const Case& item)
{
    test::GuestWindow original(Regions),recovered(Regions);Seed(original);Seed(recovered);
    auto context=Initial(item.input);auto state=crt_full_oracle::FromPpc(context);
    __imp__sub_82DF4218(context,original.Bytes());
    auto memory=recovered.Memory();
    if(!family::Apply(0x82df4218u,memory,state)) throw std::runtime_error("missing error lookup");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||
        !original.EqualCommitted(recovered)) throw std::runtime_error("error lookup Full72/RAM mismatch");
    if(context.r3.u64!=item.expected||context.r1.u64!=0x1234567800080000ull||
        context.r31.u64!=0x112233440000001full||context.lr!=0x12345678u||
        context.r11.u64!=(item.expected==GoodText?8u:12u)||
        original.Memory().ReadU32(0x7fff8u)!=0x12345678u)
        throw std::runtime_error("missed error clamp/table/call LR or save ABI");
}
} // namespace error_text61_oracle
int main()
{
    for(const auto& item:error_text61_oracle::Cases)
    {
        try {error_text61_oracle::Check(item);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::puts("PASS crt-error-text61-context 3 actual PPC cases");
    std::puts("LIMIT ordinary RAM only; no native callback/fault/MMIO/runtime coverage");
    return 0;
}
