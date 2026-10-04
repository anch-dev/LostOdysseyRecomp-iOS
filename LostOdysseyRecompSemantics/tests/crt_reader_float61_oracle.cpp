#pragma push_macro("main")
#undef main
#define main Float61RecursiveFixtureMain
#include "crt_close_recursive_buffer_context_oracle.cpp"
#undef main
#pragma pop_macro("main")
#include "lo_semantics/crt_reader_float61.h"
namespace float61_oracle {
struct RestoreHost {std::uint32_t control=PPCFPSCRRegister{}.getcsr();~RestoreHost(){PPCFPSCRRegister{}.setcsr(control);}};
using Event=std::array<std::uint64_t,10>;
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    std::vector<Event> events;
    void CallIndirect(GuestAddress target,GuestMemory& memory,RecursiveFull& state) override {
        events.push_back({target,state.r[1],state.lr,state.r[3],state.r[4],state.r[5],state.fpr_bits[1],state.fpr_bits[31],state.cached_fp_control,state.fpr_bits[7]});
        if(target!=RecursiveTarget || events.size()>2u) throw std::runtime_error("unexpected float append allocator");
        memory.WriteU32(0x55000u+static_cast<GuestAddress>(4u*(events.size()-1u)),static_cast<std::uint32_t>(state.r[5]));
        state.r[3]=events.size()==1u ? (0x9988776600000000ull|RecursiveNewNode):(0x7766554400000000ull|RecursiveNewBuffer);
        state.fpr_bits[31]=events.size()==1u ? 0xc014000000000000ull:0x4004000000000000ull;
        state.fpr_bits[1]=0x4022000000000000ull;state.fpr_bits[7]^=0x180u;
        state.cached_fp_control=0x9fc0u;PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
        state.cr1.gt^=1u;state.cr7.eq^=1u;state.xer_ca^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    std::vector<std::uint32_t> controls;
    void SetHostFpControl(std::uint32_t control) override {controls.push_back(control);PPCFPSCRRegister{}.setcsr(control);}
};
Guest* active_guest=nullptr;
void Check(bool growth) {
    RestoreHost restore;const auto scenario=growth ? RecursiveScenario::Growth:RecursiveScenario::Empty;
    test::GuestWindow original(RecursiveRegions),recovered(RecursiveRegions);
    auto seed=[&](test::GuestWindow& window){RecursiveSeed(window,scenario);window.Memory().WriteU8(RecursiveReader+24u,0u);};
    seed(original);seed(recovered);auto original_memory=original.Memory();auto recovered_memory=recovered.Memory();
    auto context=RecursiveInitial(RecursiveScenario::Empty);context.f1.u64=0x3ff8000000000000ull;context.f31.u64=0xc00c000000000000ull;context.fpscr.csr=0x9fc0u;
    const auto initial=crt_full_oracle::FromPpc(context);auto state=initial;Guest expected,actual;Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);recursive_original_memory=&original_memory;active_guest=&expected;
    __imp__sub_82BD10D8(context,original.Bytes());
    const auto original_host=PPCFPSCRRegister{}.getcsr();recursive_original_memory=nullptr;active_guest=nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!crt_reader_float61::Apply(0x82bd10d8u,recovered_memory,{actual,native},state)) throw std::runtime_error("missing float append entry");
    const auto recovered_host=PPCFPSCRRegister{}.getcsr();
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) || !original.EqualCommitted(recovered) ||
        expected.events!=actual.events || original_host!=recovered_host) throw std::runtime_error("float append Full72/RAM/callback/host control mismatch");
    const auto node=growth ? RecursiveNewNode:RecursiveOldNode;const auto data=growth ? RecursiveNewBuffer:RecursiveOldBuffer;
    if(context.r3.u64!=initial.r[3] || context.r1.u64!=initial.r[1] || context.lr!=0x81234567u || context.r31.u64!=initial.r[31] ||
        context.f31.u64!=initial.fpr_bits[31] || context.f1.u64!=(growth ? 0x4022000000000000ull:initial.fpr_bits[1]) ||
        context.fpscr.csr!=0x1f80u || recovered_host!=0x1f80u ||
        original_memory.ReadU32(Stack-112u)!=static_cast<std::uint32_t>(initial.r[1]) || original_memory.ReadU32(Stack-8u)!=0x81234567u ||
        recovery_abi::ReadU64(original_memory,Stack-16u)!=initial.r[31] || recovery_abi::ReadU64(original_memory,Stack-24u)!=initial.fpr_bits[31] ||
        original_memory.ReadU32(node+4u)!=4u || original_memory.ReadU32(data)!=(growth ? 0x40200000u:0x3fc00000u) ||
        original_memory.ReadU32(RecursiveReader+16u)!=data || expected.events.size()!=(growth ? 2u:0u))
        throw std::runtime_error("float append live-FPR/save/finite-store/control outcome absent");
    if(native.controls!=std::vector<std::uint32_t>(growth ? 2u:1u,0x1f80u)) throw std::runtime_error("float append flush changes absent");
    if(growth && (expected.events[0][1]!=initial.r[1]-240u || expected.events[0][2]!=0x82bd080cu || expected.events[1][2]!=0x82bd0868u))
        throw std::runtime_error("float append allocator SP/LR absent");
}
}
void OriginalFloatAppendIndirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);float61_oracle::active_guest->CallIndirect(target,*recursive_original_memory,state);crt_full_oracle::ToPpc(context,state);
}
int main(){try {float61_oracle::Check(false);float61_oracle::Check(true);std::puts("PASS crt-reader-float61 2 actual PPC cases");return 0;}
catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
