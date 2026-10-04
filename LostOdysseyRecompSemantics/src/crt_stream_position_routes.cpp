#include "lo_semantics/crt_stream_position_routes.h"

#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_position_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }
std::int64_t Signed(std::uint64_t value)
{ return std::bit_cast<std::int64_t>(value); }

void Compare(crt_async_status_transfer::Condition& condition,
    const Registers& state, std::uint64_t left, std::uint64_t right,
    bool signed_words)
{
    if (signed_words)
    {
        const auto a = S(left), b = S(right);
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), state.xer_so};
    }
    else
    {
        const auto a = W(left), b = W(right);
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), state.xer_so};
    }
}

void CompareDouble(crt_async_status_transfer::Condition& condition,
    const Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Signed(left), b = Signed(right);
    condition = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void Save(GuestMemory& memory, Registers& state, unsigned first,
    GuestAddress return_address, unsigned frame)
{
    R(state, 12) = state.lr;
    for (unsigned index = first; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 16u -
            8u * (31u - index)), R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
    state.lr = return_address;
    memory.WriteU32(Address(R(state, 1) - frame), W(R(state, 1)));
    R(state, 1) -= frame;
}

void Restore(GuestMemory& memory, Registers& state, unsigned first,
    unsigned frame)
{
    R(state, 1) += frame;
    for (unsigned index = first; index <= 31u; ++index)
        R(state, index) = ReadU64(memory, Address(R(state, 1) - 16u -
            8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

crt_stream_operations::Registers ToStream(const Registers& state)
{
    crt_stream_operations::Registers lower{};
    lower.r = state.r; lower.sp = state.r[1];
    lower.lr = state.lr; lower.ctr = state.ctr;
    lower.xer_so = state.xer_so; lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so};
    lower.cr6 = {state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    return lower;
}
void FromStream(Registers& state,
    const crt_stream_operations::Registers& lower)
{
    state.r = lower.r; state.r[1] = lower.sp;
    state.lr = lower.lr; state.ctr = lower.ctr;
    state.xer_so = lower.xer_so; state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.so};
    state.cr6 = {lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

raw_allocation_context::Registers ToHeap(const Registers& state)
{
    raw_allocation_context::Registers lower{};
    lower.r = state.r; lower.lr = state.lr; lower.ctr = state.ctr;
    lower.f0_bits = state.fpr_bits[0];
    lower.f1_bits = state.fpr_bits[1];
    lower.f13_bits = state.fpr_bits[13];
    lower.f30_bits = state.fpr_bits[30];
    lower.f31_bits = state.fpr_bits[31];
    lower.cached_fp_control = state.cached_fp_control;
    lower.xer_so = state.xer_so; lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so};
    lower.cr6 = {state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    return lower;
}
void FromHeap(Registers& state,
    const raw_allocation_context::Registers& lower)
{
    state.r = lower.r; state.lr = lower.lr; state.ctr = lower.ctr;
    state.fpr_bits[0] = lower.f0_bits;
    state.fpr_bits[1] = lower.f1_bits;
    state.fpr_bits[13] = lower.f13_bits;
    state.fpr_bits[30] = lower.f30_bits;
    state.fpr_bits[31] = lower.f31_bits;
    state.cached_fp_control = lower.cached_fp_control;
    state.xer_so = lower.xer_so; state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.un};
    state.cr6 = {lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.un};
}

class RawHeap final : public raw_allocation_context::PpcBoundaryServices
{
public:
    explicit RawHeap(heap_allocation_context::BoundaryServices& boundary)
        : boundary_(boundary) {}
    void CallDirect(GuestAddress entry, GuestMemory& memory,
        raw_allocation_context::Registers& state) override
    {
        if (!heap_allocation_context::Apply(entry,memory,boundary_,state))
            throw std::logic_error("unselected raw heap guest callee");
    }
private:
    heap_allocation_context::BoundaryServices& boundary_;
};

void Direct(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state,
    GuestAddress return_address)
{
    state.lr = return_address;
    if (entry == 0x823acc98u || entry == 0x823accb0u ||
        entry == 0x823ade28u)
    {
        auto lower = ToHeap(state);
        bool found = false;
        if (entry == 0x823acc98u)
        {
            RawHeap boundary(dependencies.heap_allocate);
            found = raw_allocation_context::Apply(entry,memory,boundary,lower);
        }
        else if (entry == 0x823accb0u)
            found = heap_allocation_context::Apply(entry,memory,
                dependencies.heap_allocate,lower);
        else
            found = heap_free_context::Apply(entry,memory,
                dependencies.heap_free,lower);
        if (!found) throw std::logic_error("missing accepted heap callee");
        FromHeap(state,lower);
        return;
    }
    auto lower = ToStream(state);
    const bool found = entry == 0x82b85ea8u || entry == 0x82b81b88u ?
        crt_stream_operations::Apply(entry,memory,dependencies.stream,lower):
        crt_stream_operations::ApplyAcceptedCallee(entry,memory,
            dependencies.stream,lower);
    if (!found) throw std::logic_error("missing accepted CRT stream callee");
    FromStream(state,lower);
}

void ConvertStatus(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    crt_status_error::Registers lower{};
    lower.sp = R(state, 1); lower.lr = state.lr;
    lower.r3 = R(state, 3); lower.r11 = R(state, 11);
    lower.r12 = R(state, 12); lower.r13 = R(state, 13);
    lower.xer_so = state.xer_so;
    lower.cr6 = {state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    if (!crt_status_error::Apply(0x827ca628u,memory,
        dependencies.stream.io,lower))
        throw std::logic_error("missing accepted status callee");
    R(state, 1) = lower.sp; state.lr = lower.lr;
    R(state, 3) = lower.r3; R(state, 11) = lower.r11;
    R(state, 12) = lower.r12; R(state, 13) = lower.r13;
    state.cr6 = {lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void SetPositionNative(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    memory.WriteU32(Address(R(state, 1) - 128u), W(R(state, 1)));
    R(state, 1) -= 128u;
    R(state, 7) = 14; R(state, 6) = 8;
    R(state, 5) = R(state, 1) + 88u;
    R(state, 4) = R(state, 1) + 80u;
    R(state, 31) = R(state, 3);
    state.lr = 0x82be44d0u;
    dependencies.native.NtQueryInformationFile(memory,state);
    Compare(state.cr0,state,R(state,3),0,true);
    if (!state.cr0.lt)
    {
        R(state, 11) = ReadU64(memory,Address(R(state,1)+88u));
        R(state, 7) = 20; R(state, 6) = 8;
        R(state, 5) = R(state, 1) + 96u;
        R(state, 4) = R(state, 1) + 80u;
        R(state, 3) = R(state, 31);
        WriteU64(memory,Address(R(state,1)+96u),R(state,11));
        state.lr = 0x82be44f8u;
        dependencies.native.NtSetInformationFile(memory,state);
        Compare(state.cr0,state,R(state,3),0,true);
        if (!state.cr0.lt)
        {
            R(state, 11) = ReadU64(memory,Address(R(state,1)+88u));
            R(state, 7) = 19; R(state, 6) = 8;
            R(state, 5) = R(state, 1) + 104u;
            R(state, 4) = R(state, 1) + 80u;
            R(state, 3) = R(state, 31);
            WriteU64(memory,Address(R(state,1)+104u),R(state,11));
            state.lr = 0x82be4520u;
            dependencies.native.NtSetInformationFile(memory,state);
            Compare(state.cr0,state,R(state,3),0,true);
            if (!state.cr0.lt)
                R(state, 3) = 1;
            else
            {
                ConvertStatus(memory,dependencies,state,0x82be4534u);
                R(state, 3) = 0;
            }
        }
        else
        {
            ConvertStatus(memory,dependencies,state,0x82be4534u);
            R(state, 3) = 0;
        }
    }
    else
    {
        ConvertStatus(memory,dependencies,state,0x82be4534u);
        R(state, 3) = 0;
    }
    R(state, 1) += 128u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory,Address(R(state, 1) - 16u));
}

void SetFlag(GuestMemory& memory, Registers& state)
{
    R(state, 11) = 0xffffffff83380000ull;
    R(state, 9) = WordRotateMask(R(state, 3),6,0x7c0u);
    R(state, 10) = R(state, 11) - 29312u;
    state.xer_ca = (S(R(state,3)) < 0) && ((W(R(state,3)) & 31u) != 0);
    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(S(R(state,3)) >> 5));
    Compare(state.cr6,state,R(state,4),16384u,true);
    R(state, 8) = WordRotateMask(R(state, 11),2,0xfffffffcu);
    R(state, 11) = memory.ReadU32(Address(R(state,8)+R(state,10)));
    R(state, 11) += R(state, 9);
    R(state, 7) = WordRotateMask(memory.ReadU8(Address(R(state,11)+4u)),
        0,0xffffff80u);
    if (state.cr6.eq)
    {
        R(state, 6) = memory.ReadU8(Address(R(state,11)+4u));
        R(state, 5) = UINT64_MAX - 127u;
        R(state, 6) |= R(state, 5);
        memory.WriteU8(Address(R(state,11)+4u),
            static_cast<std::uint8_t>(R(state,6)));
        R(state, 11) = memory.ReadU32(Address(R(state,8)+R(state,10)));
        R(state, 11) += R(state, 9);
        R(state, 10) = memory.ReadU8(Address(R(state,11)+40u));
        R(state, 10) &= 1u;
        memory.WriteU8(Address(R(state,11)+40u),
            static_cast<std::uint8_t>(R(state,10)));
    }
    else
    {
        Compare(state.cr6,state,R(state,4),32768u,false);
        if (state.cr6.eq)
        {
            R(state, 10) = memory.ReadU8(Address(R(state,11)+4u));
            R(state, 10) &= 0x7fu;
            memory.WriteU8(Address(R(state,11)+4u),
                static_cast<std::uint8_t>(R(state,10)));
        }
    }
    Compare(state.cr6,state,R(state,7),0,true);
    R(state, 3) = state.cr6.eq ? 32768u : 16384u;
}

void SeedOutput(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save(memory,state,29u,0x82df43f8u,112u);
    R(state, 31) = R(state, 3);
    R(state, 30) = R(state, 4);
    R(state, 29) = R(state, 5);
    Direct(0x82b86228u,memory,dependencies,state,0x82df440cu);
    Compare(state.cr6,state,R(state,3),UINT32_MAX,true);
    if (state.cr6.eq)
    {
        Direct(0x82b7fd78u,memory,dependencies,state,0x82df4418u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 9;
        R(state, 3) = UINT64_MAX;
        memory.WriteU32(Address(R(state,11)),9u);
    }
    else
    {
        R(state,6) = R(state,29); R(state,5) = 0;
        R(state,4) = R(state,30);
        Direct(0x82be2938u,memory,dependencies,state,0x82df443cu);
        R(state,30) = R(state,3);
        Compare(state.cr6,state,R(state,30),UINT32_MAX,true);
        if (state.cr6.eq)
            Direct(0x822ca100u,memory,dependencies,state,0x82df444cu);
        else R(state,3) = 0;
        Compare(state.cr6,state,R(state,3),0,false);
        if (!state.cr6.eq)
        {
            Direct(0x82b7fde8u,memory,dependencies,state,0x82df4460u);
            R(state,3) = UINT64_MAX;
        }
        else
        {
            state.xer_ca = (S(R(state,31)) < 0) &&
                ((W(R(state,31)) & 31u) != 0);
            R(state,10) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(S(R(state,31)) >> 5));
            R(state,11) = 0xffffffff83380000ull;
            R(state,9) = WordRotateMask(R(state,10),2,0xfffffffcu);
            R(state,11) -= 29312u;
            R(state,10) = WordRotateMask(R(state,31),6,0x7c0u);
            R(state,3) = R(state,30);
            R(state,11) = memory.ReadU32(Address(R(state,9)+R(state,11)));
            R(state,11) += R(state,10);
            R(state,10) = memory.ReadU8(Address(R(state,11)+4u));
            R(state,10) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(static_cast<std::int8_t>(R(state,10))));
            R(state,10) = WordRotateMask(R(state,10),0,
                0xfffffffffffffffdull);
            memory.WriteU8(Address(R(state,11)+4u),
                static_cast<std::uint8_t>(R(state,10)));
        }
    }
    Restore(memory,state,29u,112u);
}

void ReadFlagGlobal(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state,12) = state.lr;
    memory.WriteU32(Address(R(state,1)-8u),W(R(state,12)));
    memory.WriteU32(Address(R(state,1)-96u),W(R(state,1)));
    R(state,1) -= 96u;
    R(state,10) = R(state,3);
    Compare(state.cr6,state,R(state,10),0,false);
    if (state.cr6.eq)
    {
        Direct(0x82b7fd78u,memory,dependencies,state,0x82df6b84u);
        R(state,11) = R(state,3); R(state,10) = 22;
        R(state,7) = R(state,6) = R(state,5) =
            R(state,4) = R(state,3) = 0;
        memory.WriteU32(Address(R(state,11)),22u);
        Direct(0x82b7fec0u,memory,dependencies,state,0x82df6ba8u);
        R(state,3) = 22;
    }
    else
    {
        R(state,11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(-2094071808));
        R(state,3) = 0;
        R(state,11) = memory.ReadU32(Address(R(state,11)-13024u));
        memory.WriteU32(Address(R(state,10)),W(R(state,11)));
    }
    R(state,1) += 96u;
    R(state,12) = memory.ReadU32(Address(R(state,1)-8u));
    state.lr = R(state,12);
}

void TransferTail(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save(memory,state,26u,0x82df6950u,144u);
    R(state,29) = R(state,4);
    R(state,5) = 1;
    R(state,4) = 0;
    R(state,31) = R(state,3);
    R(state,27) = 0;
    Direct(0x82b85ea8u,memory,dependencies,state,0x82df696cu);
    R(state,26) = R(state,3);
    CompareDouble(state.cr6,state,R(state,26),UINT64_MAX);
    if (state.cr6.eq) goto errno_result;
    R(state,5) = 2;
    R(state,4) = 0;
    R(state,3) = R(state,31);
    Direct(0x82b85ea8u,memory,dependencies,state,0x82df6988u);
    CompareDouble(state.cr6,state,R(state,3),UINT64_MAX);
    if (state.cr6.eq) goto errno_result;
    R(state,30) = R(state,29) - R(state,3);
    CompareDouble(state.cr6,state,R(state,30),0);
    if (state.cr6.gt)
    {
        Direct(0x823acc98u,memory,dependencies,state,0x82df69a0u);
        R(state,4) = 8; R(state,5) = 4096;
        Direct(0x823accb0u,memory,dependencies,state,0x82df69acu);
        R(state,29) = R(state,3);
        Compare(state.cr0,state,R(state,29),0,true);
        if (state.cr0.eq)
        {
            Direct(0x82b7fd78u,memory,dependencies,state,0x82df69b8u);
            R(state,11) = 12;
            memory.WriteU32(Address(R(state,3)),12u);
            goto errno_result;
        }
        R(state,4) = 32768u;
        R(state,3) = R(state,31);
        state.lr = 0x82df69e0u;
        SetFlag(memory,state);
        R(state,28) = R(state,3);
        do
        {
            CompareDouble(state.cr6,state,R(state,30),4096u);
            R(state,5) = 4096;
            if (state.cr6.lt)
                R(state,5) = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(S(R(state,30))));
            R(state,4) = R(state,29);
            R(state,3) = R(state,31);
            Direct(0x82b81b88u,memory,dependencies,state,0x82df6a00u);
            Compare(state.cr6,state,R(state,3),UINT32_MAX,true);
            if (state.cr6.eq)
            {
                Direct(0x82b7fdb0u,memory,dependencies,state,0x82df6a20u);
                R(state,11) = memory.ReadU32(Address(R(state,3)));
                Compare(state.cr6,state,R(state,11),5u,false);
                if (state.cr6.eq)
                {
                    Direct(0x82b7fd78u,memory,dependencies,state,
                        0x82df6a30u);
                    R(state,11) = 13;
                    memory.WriteU32(Address(R(state,3)),13u);
                }
                R(state,27) = UINT64_MAX;
                break;
            }
            R(state,11) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(S(R(state,3))));
            R(state,30) -= R(state,11);
            CompareDouble(state.cr6,state,R(state,30),0);
        } while (state.cr6.gt);
        R(state,4) = R(state,28);
        R(state,3) = R(state,31);
        state.lr = 0x82df6a48u;
        SetFlag(memory,state);
        Direct(0x823acc98u,memory,dependencies,state,0x82df6a4cu);
        R(state,4) = 0;
        R(state,5) = R(state,29);
        Direct(0x823ade28u,memory,dependencies,state,0x82df6a58u);
    }
    else if (state.cr6.lt)
    {
        R(state,5) = 0;
        R(state,4) = R(state,29);
        R(state,3) = R(state,31);
        Direct(0x82b85ea8u,memory,dependencies,state,0x82df6a70u);
        CompareDouble(state.cr6,state,R(state,3),UINT64_MAX);
        if (state.cr6.eq) goto errno_result;
        R(state,3) = R(state,31);
        Direct(0x82b86228u,memory,dependencies,state,0x82df6a80u);
        state.lr = 0x82df6a84u;
        SetPositionNative(memory,dependencies,state);
        R(state,11) = std::countl_zero(W(R(state,3)));
        R(state,11) = WordRotateMask(R(state,11),27,1u);
        R(state,27) = 0u - R(state,11);
        CompareDouble(state.cr6,state,R(state,27),UINT64_MAX);
        if (state.cr6.eq)
        {
            Direct(0x82b7fd78u,memory,dependencies,state,0x82df6a9cu);
            R(state,11) = 13;
            memory.WriteU32(Address(R(state,3)),13u);
            Direct(0x82b7fdb0u,memory,dependencies,state,0x82df6aa8u);
            R(state,30) = R(state,3);
            Direct(0x822ca100u,memory,dependencies,state,0x82df6ab0u);
            memory.WriteU32(Address(R(state,30)),W(R(state,3)));
        }
    }
    CompareDouble(state.cr6,state,R(state,27),UINT64_MAX);
    if (state.cr6.eq) goto errno_result;
    R(state,5) = 0;
    R(state,4) = R(state,26);
    R(state,3) = R(state,31);
    Direct(0x82b85ea8u,memory,dependencies,state,0x82df6accu);
    CompareDouble(state.cr6,state,R(state,3),UINT64_MAX);
    if (state.cr6.eq) goto errno_result;
    R(state,3) = 0;
    goto done;
errno_result:
    Direct(0x82b7fd78u,memory,dependencies,state,0x82df69c4u);
    R(state,3) = memory.ReadU32(Address(R(state,3)));
done:
    Restore(memory,state,26u,144u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82df43f0u: SeedOutput(memory,dependencies,state); return true;
    case 0x82df6948u: TransferTail(memory,dependencies,state); return true;
    case 0x82df6ae0u: SetFlag(memory,state); return true;
    case 0x82df6b68u: ReadFlagGlobal(memory,dependencies,state); return true;
    case 0x82be44a8u: SetPositionNative(memory,dependencies,state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_position_routes
