// The accepted close fixture supplies selected CRT errno services and full
// PPC register/RAM comparison without executing its own case matrix.
#define main ParseChainSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_numeric_parse_chain61_context.h"

namespace parse_chain_oracle
{
namespace adjacent=crt_numeric_parse_chain61_context;
constexpr GuestAddress Output=0x55000u,Source=0x55100u;
enum class ScanRoute {SignedWhitespace,UnsignedOverflow,InvalidBase,UpperSuccess,UpperBound};
struct ScanCase {const char* name;GuestAddress entry;ScanRoute route;};
constexpr std::array ScanCases{
    ScanCase{"signed-whitespace-predicate",0x82b86be8u,ScanRoute::SignedWhitespace},
    ScanCase{"unsigned-overflow-errno34",0x82b86be8u,ScanRoute::UnsignedOverflow},
    ScanCase{"parser-invalid-base-errno22",0x82b86be8u,ScanRoute::InvalidBase},
    ScanCase{"upper-search-parse-format-copy",0x82df3dc0u,ScanRoute::UpperSuccess},
    ScanCase{"upper-next-value-limit",0x82df3dc0u,ScanRoute::UpperBound}};
constexpr GuestAddress EndPointer=0x55300u,Locale=0x57000u,ByteLocale=0x57200u,Classes=0x57400u;
void Seed(GuestWindow& window,ScanRoute route)
{
    close_shared_oracle::SeedShared(window,close_shared_oracle::Route::NullPath);
    auto memory=window.Memory();
    memory.WriteU32(0x83215300u,Locale);
    memory.WriteU32(Locale+172u,2u);
    memory.WriteU32(Locale+200u,Classes);
    memory.WriteU32(0x83215304u,ByteLocale);memory.WriteU32(ByteLocale+8u,1u);
    for(unsigned i=0;i<256u;++i)
    {
        memory.WriteU8(ByteLocale+29u+i,0u);
        memory.WriteU16(Classes+2u*i,i==' '?8u:i>='0'&&i<='9'?4u:
            (i>='a'&&i<='z')||(i>='A'&&i<='Z')?1u:0u);
    }
    const char* text=route==ScanRoute::SignedWhitespace?" -42x":
        route==ScanRoute::UnsignedOverflow?"4294967296":
        route==ScanRoute::InvalidBase?"12":"save.1";
    for(unsigned i=0;;++i)
    {memory.WriteU8(Source+i,static_cast<std::uint8_t>(text[i]));if(text[i]==0)break;}
    memory.WriteU32(EndPointer,0xdeadbeefu);
}
PPCContext Initial(ScanRoute route)
{
    auto context=close_shared_oracle::Initial(close_shared_oracle::Route::NullPath);
    if(route==ScanRoute::UpperSuccess||route==ScanRoute::UpperBound)
    {
        context.r3.u64=0xaabbccdd00000000ull|Source;
        context.r4.u64=16u;context.r5.u64=route==ScanRoute::UpperSuccess?100u:2u;
    }
    else
    {
        context.r3.u64=0x8877665583215300ull;
        context.r4.u64=0xaabbccdd00000000ull|Source;
        context.r5.u64=EndPointer;
        context.r6.u64=route==ScanRoute::InvalidBase?1u:10u;
        context.r7.u64=route==ScanRoute::UnsignedOverflow?1u:0u;
    }
    return context;
}
void Check(const ScanCase& item,unsigned ordinal)
{
    GuestWindow original(OpenRegions),recovered(OpenRegions);
    Seed(original,item.route);Seed(recovered,item.route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService expected_index,actual_index;
    Host expected_host(Scenario::BinarySuccess),actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock,actual_unlock;
    close_shared_oracle::SharedExtra expected_extra,actual_extra;
    auto context=Initial(item.route);
    auto state=crt_full_oracle::FromPpc(context);
    current=&expected;active=&expected;current_index=&expected_index;
    current_host=&expected_host;
    wrapper_oracle::original_unlock=&expected_unlock;
    close_shared_oracle::active_extra=&expected_extra;
    if(item.entry==0x82df3dc0u) __imp__sub_82DF3DC0(context,original.Bytes());
    else __imp__sub_82B86BE8(context,original.Bytes());
    current=nullptr;active=nullptr;current_index=nullptr;current_host=nullptr;
    wrapper_oracle::original_unlock=nullptr;
    close_shared_oracle::active_extra=nullptr;
    if(!adjacent::Apply(item.entry,actual.memory,
            close_shared_oracle::Deps(actual,actual_index,actual_host,
                actual_unlock,actual_extra),state))
        throw std::runtime_error("missing adjacent scan body");
    const auto before=crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after=crt_full_oracle::Snapshot(state);
    if(before!=after||!original.EqualCommitted(recovered)||
        expected.events!=actual.events||
        expected_index.events!=actual_index.events||
        expected_host.events!=actual_host.events||
        expected_unlock.events!=actual_unlock.events||
        expected_extra.events!=actual_extra.events||
        expected.traps!=actual.traps||
        expected.converted_errors!=actual.converted_errors)
    {
        for(unsigned i=0;i<before.size();++i)
            if(before[i]!=after[i])
                std::fprintf(stderr,"case %u state[%u] %llx/%llx\n",ordinal,i,
                    static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        for(const auto region:OpenRegions)
            for(std::size_t i=0;i<region.size;++i)
                if(original.Bytes()[region.base+i]!=
                    recovered.Bytes()[region.base+i])
                {std::fprintf(stderr,"case %u RAM %08llx %02x/%02x\n",
                    ordinal,static_cast<unsigned long long>(region.base+i),
                    original.Bytes()[region.base+i],
                    recovered.Bytes()[region.base+i]);break;}
        throw std::runtime_error("adjacent scan full state/RAM/callback mismatch");
    }
    if(context.r1.u64!=(0x8877665500000000ull|Stack)||context.lr!=0x81234567u||
        context.r28.u64!=0x112233440000001cull||context.r31.u64!=0x112233440000001full)
        throw std::runtime_error("missed parser/caller saved register ABI");
    if(item.route==ScanRoute::SignedWhitespace)
    {
        if(context.r3.s64!=-42||expected.memory.ReadU32(EndPointer)!=Source+4u)
            throw std::runtime_error("missed signed whitespace/end pointer");
    }
    else if(item.route==ScanRoute::UnsignedOverflow)
    {
        if(context.r3.u32!=UINT32_MAX||expected.memory.ReadU32(0x83215210u)!=34u||
            expected.memory.ReadU32(EndPointer)!=Source+10u)
            throw std::runtime_error("missed parser range clamp");
    }
    else if(item.route==ScanRoute::InvalidBase)
    {
        if(context.r3.u64!=0u||expected.memory.ReadU32(0x83215210u)!=22u||
            expected.memory.ReadU32(EndPointer)!=Source)
            throw std::runtime_error("missed parser invalid base");
    }
    else if(item.route==ScanRoute::UpperSuccess)
    {
        if(context.r3.u64!=0u||expected.memory.ReadU8(Source+5u)!='2'||
            expected.memory.ReadU8(Source+6u)!=0u)
            throw std::runtime_error("missed complete suffix chain");
    }
    else if(context.r3.s64!=-1||expected.memory.ReadU8(Source+5u)!='1')
        throw std::runtime_error("missed suffix limit");
}

} // namespace parse_chain_oracle

int main()
{
    for(unsigned i=0;i<parse_chain_oracle::ScanCases.size();++i)
    {
        const auto& item=parse_chain_oracle::ScanCases[i];
        try{parse_chain_oracle::Check(item,i);}
        catch(const std::exception& error)
        {std::fprintf(stderr,"%s: %s\n",item.name,error.what());return 1;}
    }
    std::printf("PASS crt-numeric-parse-chain61-context %zu actual PPC cases\n",
        parse_chain_oracle::ScanCases.size());
    std::puts("LIMIT selected full context and accepted locale/errno/copy selected boundary; no runtime/fault/MMIO/concurrency validation");
    return 0;
}
