#define main OutputUpperRefillFixtureMain
#include "crt_stream_refill_context_oracle.cpp"
#undef main
#include "lo_semantics/crt_stream_output_upper61.h"
#include <fstream>
namespace output_upper_oracle {
namespace family=crt_stream_output_upper61;
constexpr GuestAddress Format=0x40000u,Standard=0x83214af0u;
constexpr GuestAddress Stdout=Standard+32u,OutBuffer=0x55000u;
const auto Regions=[] {std::vector<test::Region> r(OpenRegions.begin(),OpenRegions.end());
    r.push_back({0x82020000u,0x1000u});r.push_back({0x820d3000u,0x1000u});
    r.push_back({0x83313000u,0x1000u});return r;}();
struct Extra : close_shared_oracle::SharedExtra {
    GuestMemory* memory=nullptr;
    std::vector<std::array<std::uint64_t,6>> output_events;
    std::vector<crt_formatting_support::Registers> output_states;
    std::string console;
    static constexpr GuestAddress AnsiScratch=0x57000u;
    void InitializeUnicodeString(GuestMemory& m,crt_formatting_support::Registers& state) override {
        output_states.push_back(state);
        if(state.lr!=0x82be471cu)throw std::runtime_error("prompt Unicode init LR");
        const auto source=Address(state.r4);unsigned length=0;
        while(m.ReadU16(source+length*2u)!=0u) {
            if(length>=1u)throw std::runtime_error("prompt expected one actual code unit");
            ++length;
        }
        m.WriteU16(Address(state.r3),static_cast<std::uint16_t>(length*2u));
        m.WriteU16(Address(state.r3+2u),static_cast<std::uint16_t>((length+1u)*2u));
        m.WriteU32(Address(state.r3+4u),source);
        state.r10=0x1020304000000011ull;
    }
    void UnicodeStringToAnsiString(GuestMemory& m,crt_formatting_support::Registers& state) override {
        output_states.push_back(state);
        if(state.lr!=0x82be472cu||state.r5!=1u)throw std::runtime_error("prompt Unicode conversion ABI");
        const auto descriptor=Address(state.r4),source=m.ReadU32(descriptor+4u);
        const auto length=m.ReadU16(descriptor)/2u;
        for(unsigned i=0;i<length;++i) {
            const auto unit=m.ReadU16(source+i*2u);
            if(unit>0x7fu)throw std::runtime_error("unselected prompt non-ASCII conversion");
            m.WriteU8(AnsiScratch+i,static_cast<std::uint8_t>(unit));
        }
        m.WriteU8(AnsiScratch+length,0u);
        m.WriteU16(Address(state.r3),static_cast<std::uint16_t>(length));
        m.WriteU16(Address(state.r3+2u),static_cast<std::uint16_t>(length+1u));
        m.WriteU32(Address(state.r3+4u),AnsiScratch);
        state.r3=0u;state.r10=0x5060708000000022ull;
    }
    void FreeAnsiString(GuestMemory& m,crt_formatting_support::Registers& state) override {
        output_states.push_back(state);
        if(state.lr!=0x82be4758u||m.ReadU32(Address(state.r3+4u))!=AnsiScratch)
            throw std::runtime_error("prompt actual allocated ANSI release");
        state.r3=0x1234567800000066ull;state.r10=0x90a0b0c000000033ull;
    }
    void InitAnsiString(GuestMemory& m,GuestAddress descriptor,GuestAddress message) override {
        if(message!=AnsiScratch)throw std::runtime_error("prompt ANSI output pointer");
        unsigned length=0;while(m.ReadU8(message+length)!=0u)++length;
        output_events.push_back({1u,descriptor,message,length,0u,0u});
        m.WriteU16(descriptor,static_cast<std::uint16_t>(length));
        m.WriteU16(descriptor+2u,static_cast<std::uint16_t>(length+1u));
        m.WriteU32(descriptor+4u,message);
    }
    std::uint64_t WriteAnsi(GuestAddress buffer,std::uint16_t length) override {
        if(!memory||buffer!=AnsiScratch||length!=1u)throw std::runtime_error("prompt ANSI write descriptor");
        output_events.push_back({2u,buffer,length,memory->ReadU8(buffer),0u,0u});
        for(unsigned i=0;i<length;++i)console.push_back(static_cast<char>(memory->ReadU8(buffer+i)));
        return 0u;
    }
    void EnterCriticalSection(GuestMemory&,crt_stream_bulk_close_routes::Registers& state) override {
        events.push_back({3u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0x1234567800000044ull;
    }
    void LeaveCriticalSection(GuestMemory&,crt_stream_bulk_close_routes::Registers& state) override {
        if(state.lr!=0x82b7b584u||Address(state.r[3])!=Standard+32u)
            throw std::runtime_error("prompt fgets live tail boundary missed");
        events.push_back({4u,state.sp,state.lr,state.r[3],state.r[10],state.r[31]});
        state.r[10]=0x9876543200000055ull;
    }
};
family::Dependencies* original_dependencies=nullptr;
void ReadImageRange(GuestMemory& memory,GuestAddress address,
    std::size_t length)
{
    std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",
        std::ios::binary);
    if(!image) throw std::runtime_error("missing private image fixture");
    image.seekg(static_cast<std::streamoff>(address-0x82000000u));
    std::vector<char> bytes(length);
    image.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if(image.gcount()!=static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("short private image fixture range");
    for(std::size_t i=0;i<bytes.size();++i)
        memory.WriteU8(address+static_cast<GuestAddress>(i),
            static_cast<std::uint8_t>(bytes[i]));
}

void Seed(GuestWindow& window,unsigned route) {
    refill_oracle::SeedRefill(window);auto m=window.Memory();
    ReadImageRange(m,0x820d3000u,0x1000u);
    ReadImageRange(m,0x83214000u,0x3000u);
    m.WriteU32(Environment+256u,ErrorState);
    m.WriteU32(0x83214d74u,0x7200u);m.WriteU32(0x83214d78u,1u);
    m.WriteU32(0x83214fc8u+24u,0x8231a2a0u);
    m.WriteU32(0x83214fc8u+32u,0x82b7eef8u);
    m.WriteU32(0x83214fc8u+36u,0x82b7ee58u);
    m.WriteU32(0x83215358u+17u*8u,0x63000u);
    // Built-in stdin uses accepted indexed lock16, separate from stdout17.
    // Both are initialized; lock initialization/allocation is outside this case.
    m.WriteU32(0x83215358u+16u*8u,0x64000u);
    m.WriteU32(0x83313638u,route?1u:0u);
    m.WriteU32(0x8331365cu,0u);m.WriteU32(0x83313658u,0u);
    m.WriteU16(Format,'A');m.WriteU16(Format+2u,0u);
    m.WriteU16(0x82020d6cu,10u);m.WriteU16(0x82020d6eu,0u);
    m.WriteU32(Stdout,OutBuffer);m.WriteU32(Stdout+4u,128u);
    m.WriteU32(Stdout+8u,OutBuffer);m.WriteU32(Stdout+12u,0x42u);
    m.WriteU32(Stdout+24u,128u);
    m.WriteU32(Standard,Buffer);m.WriteU32(Standard+4u,2u);
    m.WriteU32(Standard+8u,Buffer);m.WriteU32(Standard+12u,0x40u);
    m.WriteU32(Standard+16u,0xfffffffeu);m.WriteU32(Standard+24u,4u);
    m.WriteU8(Buffer,route==1u?'Y':'N');m.WriteU8(Buffer+1u,10u);
    m.WriteU8(0x8321531cu,0u);m.WriteU8(0x83215340u,0u);
}
void Check(unsigned route) {
    GuestWindow original(Regions),recovered(Regions);Seed(original,route);Seed(recovered,route);
    Services expected(original,Mode::LockedWrite),actual(recovered,Mode::LockedWrite);
    IndexService ei,ai;refill_oracle::RefillHost eh,ah;
    wrapper_oracle::WrapperUnlock eu,au;Extra ee,ae;ee.memory=&expected.memory;ae.memory=&actual.memory;
    auto ed=refill_oracle::Deps(expected,ei,eh,eu,ee);
    auto ad=refill_oracle::Deps(actual,ai,ah,au,ae);
    auto context=refill_oracle::InitialRefill(refill_oracle::RefillRoute::Binary);
    context.r1.u64=0x8877665500000000ull|Stack;context.lr=0x1234567887654321ull;
    context.r4.u64=0x1122334400000000ull|Format;
    auto state=crt_full_oracle::FromPpc(context);
    active=&expected;original_dependencies=&ed;
    __imp__sub_827C8648(context,original.Bytes());active=nullptr;original_dependencies=nullptr;
    if(!family::Apply(0x827c8648u,actual.memory,ad,state))throw std::runtime_error("missing prompt upper");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state)||
       !original.EqualCommitted(recovered)||expected.events!=actual.events||ei.events!=ai.events||
       eh.events!=ah.events||eu.events!=au.events||ee.events!=ae.events||
       ee.output_events!=ae.output_events||ee.output_states!=ae.output_states||ee.console!=ae.console||expected.traps!=actual.traps||
       expected.converted_errors!=actual.converted_errors)
        throw std::runtime_error("prompt upper Full72/RAM/live callbacks mismatch");
    const GuestAddress formatted=Stack-8560u+352u;
    if(context.r3.u32!=(route==2u?0u:1u)||expected.memory.ReadU16(formatted)!='A'||
       expected.memory.ReadU16(formatted+2u)!=0u||context.r1.u64!=(0x8877665500000000ull|Stack)||
       context.lr!=0x87654321u)
        throw std::runtime_error("actual format result/prompt answer/LR/SP missed");
    if(route) {
        if(ee.console!="A\n"||ee.output_states.size()!=6u||ee.output_events.size()!=4u||
           expected.memory.ReadU32(Stdout)!=OutBuffer||
           expected.memory.ReadU32(Standard)!=Buffer+2u||!ee.events.empty()||
           expected.events.size()!=3u||ei.events.size()!=3u||
           expected.events[0][2]!=0x82b81b70u||expected.events[0][3]!=0x63000u||
           expected.events[1][2]!=0x82b81b70u||expected.events[1][3]!=0x63000u||
           expected.events[2][2]!=0x82b81b70u||expected.events[2][3]!=0x64000u||
           ei.events[2][2]!=0x82b7b584u||ei.events[2][3]!=0x64000u)
            throw std::runtime_error("actual puts/fgets/lock chain missed");
    } else if(!ee.events.empty()||!ei.events.empty()||!ee.output_states.empty()||!ee.console.empty())throw std::runtime_error("disabled prompt touched IO");
}
}
void OriginalOutputUpperLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    if(!crt_stream_output_upper61::ApplyAcceptedLower(entry,active->memory,
        *output_upper_oracle::original_dependencies,state))throw std::runtime_error("missing prompt lower");
    crt_full_oracle::ToPpc(context,state);
}
// The inherited next-reader pins use the same actual lower/native composition.
void OriginalNextReaderLower(GuestAddress entry,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    auto& deps=*output_upper_oracle::original_dependencies;
    const bool found=entry==0x82b81360u
        ?crt_stream_byte_read_context::Apply(entry,active->memory,deps,state)
        :crt_stream_close_shared_lower::ApplyAcceptedLower(entry,active->memory,deps,state);
    if(!found)throw std::runtime_error("missing actual next reader lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalNextReaderEnter(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    output_upper_oracle::original_dependencies->native.EnterCriticalSection(active->memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    for(unsigned route=0;route<3u;++route) {
        try{output_upper_oracle::Check(route);}
        catch(const std::exception& e){std::fprintf(stderr,"prompt route%u: %s\n",route,e.what());return 1;}
    }
    std::puts("CRT prompt upper: 3 actual format/puts/fgets cases passed");return 0;
}
