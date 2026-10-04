#include "lo_semantics/crt_stream_read_routes.h"

#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_read_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& s, unsigned index) { return s.r[index]; }
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }

void Compare(crt_async_status_transfer::Condition& condition,
    Registers& state,
    std::uint64_t left, std::uint64_t right, bool signed_words)
{
    const auto x = W(left), y = W(right);
    if (signed_words)
    {
        const auto a = S(x), b = S(y);
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), state.xer_so};
    }
    else condition = {std::uint8_t(x < y), std::uint8_t(x > y),
        std::uint8_t(x == y), state.xer_so};
}

void Save(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 18u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
}
void Restore(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 18u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 16u - 8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

crt_stream_operations::Registers ToStream(const Registers& state)
{
    crt_stream_operations::Registers lower{};
    lower.r = state.r; lower.sp = state.r[1];
    lower.lr = state.lr; lower.ctr = state.ctr;
    lower.xer_so = state.xer_so; lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    return lower;
}
void FromStream(Registers& state,
    const crt_stream_operations::Registers& lower)
{
    state.r = lower.r; state.r[1] = lower.sp;
    state.lr = lower.lr; state.ctr = lower.ctr;
    state.xer_so = lower.xer_so; state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq, lower.cr0.so};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq, lower.cr6.so};
}
raw_allocation_context::Registers ToRaw(const Registers& state)
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
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    return lower;
}
void FromRaw(Registers& state,
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
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq, lower.cr0.un};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq, lower.cr6.un};
}
class RawHeap final : public raw_allocation_context::PpcBoundaryServices
{
public:
    explicit RawHeap(heap_allocation_context::BoundaryServices& boundary)
        : boundary_(boundary) {}
    void CallDirect(GuestAddress entry, GuestMemory& memory,
        raw_allocation_context::Registers& state) override
    {
        if (!heap_allocation_context::Apply(entry, memory, boundary_, state))
            throw std::logic_error("unselected CRT raw allocation callee");
    }
private:
    heap_allocation_context::BoundaryServices& boundary_;
};

void Direct(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    if (entry == 0x82be2dd8u)
    {
        if (!crt_async_status_transfer::Apply(entry, memory,
                deps.async_native, state))
            throw std::logic_error("missing async read body");
        return;
    }
    if (entry == 0x823acbd0u)
    {
        auto lower = ToRaw(state);
        RawHeap adapter(deps.heap);
        if (!raw_allocation_context::Apply(entry, memory, adapter, lower))
            throw std::logic_error("missing CRT raw allocation body");
        FromRaw(state, lower);
        return;
    }
    auto lower = ToStream(state);
    if (entry == 0x823addc0u)
    {
        if (!crt_free_context::Apply(entry, memory, deps.free_lower, lower))
            throw std::logic_error("missing CRT free body");
    }
    else if (entry == 0x8229c560u)
    {
        if (!crt_utf8_conversion_routes::Apply(entry, memory,
                deps.conversion_native, lower))
            throw std::logic_error("missing UTF8 conversion body");
    }
    else if (entry == 0x82b85ea8u)
    {
        if (!crt_stream_operations::Apply(entry, memory, deps.stream, lower))
            throw std::logic_error("missing CRT stream seek body");
    }
    else if (!crt_stream_operations::ApplyAcceptedCallee(entry, memory,
            deps.stream, lower))
        throw std::logic_error("unselected CRT read direct callee");
    FromStream(state, lower);
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a),std::uint8_t(v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a),W(v))
#define PPC_STORE_U64(a,v) WriteU64(memory,Address(a),(v))

void ReadBody(GuestMemory& memory, Dependencies deps, Registers& state)
{
// Generated instruction-order translation of the authoritative 396 PPC
// instructions follows. All direct calls route through accepted lower bodies.

	// mflr r12
	R(state, 12) = state.lr;
	// bl 0x82b7a6c0
	state.lr = 0x82B85450;
	Save(memory, state);
	// stwu r1,-208(r1)
	const auto old_sp = W(R(state, 1));
	R(state, 1) -= 208u;
	PPC_STORE_U32(W(R(state, 1)), old_sp);
	// mr r21,r3
	R(state, 21) = R(state, 3);
	// mr r31,r5
	R(state, 31) = R(state, 5);
	// mr r18,r4
	R(state, 18) = R(state, 4);
	// li r19,-2
	R(state, 19) = -2;
	// mr r20,r31
	R(state, 20) = R(state, 31);
	// cmpwi cr6,r21,-2
	Compare(state.cr6, state, S(R(state, 21)), -2, true);
	// bne cr6,0x82b85494
	if (!state.cr6.eq) goto loc_82B85494;
	// bl 0x82b7fdb0
	state.lr = 0x82B85474;
	Direct(0x82b7fdb0u, memory, deps, state);
	// li r11,0
	R(state, 11) = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
	// bl 0x82b7fd78
	state.lr = 0x82B85480;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r10,9
	R(state, 10) = 9;
loc_82B85484:
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// li r3,-1
	R(state, 3) = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(W(R(state, 11)) + 0, W(R(state, 10)));
	// b 0x82b85a70
	goto loc_82B85A70;
loc_82B85494:
	// cmpwi cr6,r21,0
	Compare(state.cr6, state, S(R(state, 21)), 0, true);
	// blt cr6,0x82b854ac
	if (state.cr6.lt) goto loc_82B854AC;
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// lwz r11,-29336(r11)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 11)) + -29336);
	// cmplw cr6,r21,r11
	Compare(state.cr6, state, W(R(state, 21)), W(R(state, 11)), false);
	// blt cr6,0x82b854e8
	if (state.cr6.lt) goto loc_82B854E8;
loc_82B854AC:
	// bl 0x82b7fdb0
	state.lr = 0x82B854B0;
	Direct(0x82b7fdb0u, memory, deps, state);
	// li r11,0
	R(state, 11) = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
	// bl 0x82b7fd78
	state.lr = 0x82B854BC;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r10,9
	R(state, 10) = 9;
loc_82B854C0:
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// li r7,0
	R(state, 7) = 0;
	// li r6,0
	R(state, 6) = 0;
	// li r5,0
	R(state, 5) = 0;
	// li r4,0
	R(state, 4) = 0;
	// li r3,0
	R(state, 3) = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(W(R(state, 11)) + 0, W(R(state, 10)));
	// bl 0x82b7fec0
	state.lr = 0x82B854E0;
	Direct(0x82b7fec0u, memory, deps, state);
	// li r3,-1
	R(state, 3) = -1;
	// b 0x82b85a70
	goto loc_82B85A70;
loc_82B854E8:
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// rlwinm r28,r21,6,21,25
	R(state, 28) = std::rotl(W(R(state, 21)) | (R(state, 21) << 32), 6) & 0x7C0;
	// addi r29,r11,-29312
	R(state, 29) = R(state, 11) + -29312;
	// srawi r11,r21,5
	state.xer_ca = (S(R(state, 21)) < 0) && ((W(R(state, 21)) & 0x1F) != 0);
	R(state, 11) = S(R(state, 21)) >> 5;
	// rlwinm r27,r11,2,0,29
	R(state, 27) = std::rotl(W(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// extsb r10,r10
	R(state, 10) = static_cast<std::int8_t>(R(state, 10));
	// clrlwi. r9,r10,31
	R(state, 9) = W(R(state, 10)) & 0x1;
	Compare(state.cr0, state, S(R(state, 9)), 0, true);
	// beq 0x82b854ac
	if (state.cr0.eq) goto loc_82B854AC;
	// lis r9,32767
	R(state, 9) = 2147418112;
	// ori r9,r9,65535
	R(state, 9) = R(state, 9) | 65535;
	// cmplw cr6,r31,r9
	Compare(state.cr6, state, W(R(state, 31)), W(R(state, 9)), false);
	// ble cr6,0x82b8553c
	if (!state.cr6.gt) goto loc_82B8553C;
	// bl 0x82b7fdb0
	state.lr = 0x82B85528;
	Direct(0x82b7fdb0u, memory, deps, state);
	// li r11,0
	R(state, 11) = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
loc_82B85530:
	// bl 0x82b7fd78
	state.lr = 0x82B85534;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r10,22
	R(state, 10) = 22;
	// b 0x82b854c0
	goto loc_82B854C0;
loc_82B8553C:
	// li r26,0
	R(state, 26) = 0;
	// cmplwi cr6,r31,0
	Compare(state.cr6, state, W(R(state, 31)), 0, false);
	// mr r30,r26
	R(state, 30) = R(state, 26);
	// beq cr6,0x82b85a6c
	if (state.cr6.eq) goto loc_82B85A6C;
	// rlwinm. r10,r10,0,30,30
	R(state, 10) = std::rotl(W(R(state, 10)) | (R(state, 10) << 32), 0) & 0x2;
	Compare(state.cr0, state, S(R(state, 10)), 0, true);
	// bne 0x82b85a6c
	if (!state.cr0.eq) goto loc_82B85A6C;
	// cmplwi cr6,r18,0
	Compare(state.cr6, state, W(R(state, 18)), 0, false);
	// bne cr6,0x82b85568
	if (!state.cr6.eq) goto loc_82B85568;
loc_82B8555C:
	// bl 0x82b7fdb0
	state.lr = 0x82B85560;
	Direct(0x82b7fdb0u, memory, deps, state);
	// stw r26,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 26)));
	// b 0x82b85530
	goto loc_82B85530;
loc_82B85568:
	// lbz r11,40(r11)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 11)) + 40);
	// rotlwi r11,r11,24
	R(state, 11) = std::rotl(W(R(state, 11)), 24);
	// srawi r11,r11,25
	state.xer_ca = (S(R(state, 11)) < 0) && ((W(R(state, 11)) & 0x1FFFFFF) != 0);
	R(state, 11) = S(R(state, 11)) >> 25;
	// extsb r22,r11
	R(state, 22) = static_cast<std::int8_t>(R(state, 11));
	// cmpwi cr6,r22,1
	Compare(state.cr6, state, S(R(state, 22)), 1, true);
	// beq cr6,0x82b855a0
	if (state.cr6.eq) goto loc_82B855A0;
	// cmpwi cr6,r22,2
	Compare(state.cr6, state, S(R(state, 22)), 2, true);
	// bne cr6,0x82b85598
	if (!state.cr6.eq) goto loc_82B85598;
	// not r11,r31
	R(state, 11) = ~R(state, 31);
	// clrlwi. r11,r11,31
	R(state, 11) = W(R(state, 11)) & 0x1;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82b8555c
	if (state.cr0.eq) goto loc_82B8555C;
	// rlwinm r31,r31,0,0,30
	R(state, 31) = std::rotl(W(R(state, 31)) | (R(state, 31) << 32), 0) & 0xFFFFFFFE;
loc_82B85598:
	// mr r23,r18
	R(state, 23) = R(state, 18);
	// b 0x82b85600
	goto loc_82B85600;
loc_82B855A0:
	// not r11,r31
	R(state, 11) = ~R(state, 31);
	// clrlwi. r11,r11,31
	R(state, 11) = W(R(state, 11)) & 0x1;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82b8555c
	if (state.cr0.eq) goto loc_82B8555C;
	// rlwinm r31,r31,31,1,31
	R(state, 31) = std::rotl(W(R(state, 31)) | (R(state, 31) << 32), 31) & 0x7FFFFFFF;
	// cmplwi cr6,r31,4
	Compare(state.cr6, state, W(R(state, 31)), 4, false);
	// bge cr6,0x82b855bc
	if (!state.cr6.lt) goto loc_82B855BC;
	// li r31,4
	R(state, 31) = 4;
loc_82B855BC:
	// mr r3,r31
	R(state, 3) = R(state, 31);
	// bl 0x823acbd0
	state.lr = 0x82B855C4;
	Direct(0x823acbd0u, memory, deps, state);
	// mr. r23,r3
	R(state, 23) = R(state, 3);
	Compare(state.cr0, state, S(R(state, 23)), 0, true);
	// bne 0x82b855e4
	if (!state.cr0.eq) goto loc_82B855E4;
	// bl 0x82b7fd78
	state.lr = 0x82B855D0;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r11,12
	R(state, 11) = 12;
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
	// bl 0x82b7fdb0
	state.lr = 0x82B855DC;
	Direct(0x82b7fdb0u, memory, deps, state);
	// li r10,8
	R(state, 10) = 8;
	// b 0x82b85484
	goto loc_82B85484;
loc_82B855E4:
	// li r5,1
	R(state, 5) = 1;
	// li r4,0
	R(state, 4) = 0;
	// mr r3,r21
	R(state, 3) = R(state, 21);
	// bl 0x82b85ea8
	state.lr = 0x82B855F4;
	Direct(0x82b85ea8u, memory, deps, state);
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// std r3,48(r11)
	PPC_STORE_U64(W(R(state, 11)) + 48, R(state, 3));
loc_82B85600:
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// mr r4,r23
	R(state, 4) = R(state, 23);
	// li r24,10
	R(state, 24) = 10;
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// andi. r10,r10,72
	R(state, 10) = R(state, 10) & 72;
	Compare(state.cr0, state, S(R(state, 10)), 0, true);
	// cmpwi r10,0
	Compare(state.cr0, state, S(R(state, 10)), 0, true);
	// beq 0x82b856d0
	if (state.cr0.eq) goto loc_82B856D0;
	// lbz r11,5(r11)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 11)) + 5);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// cmplwi cr6,r31,0
	Compare(state.cr6, state, W(R(state, 31)), 0, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// stb r11,0(r23)
	PPC_STORE_U8(W(R(state, 23)) + 0, static_cast<std::uint8_t>(R(state, 11)));
	// addi r4,r23,1
	R(state, 4) = R(state, 23) + 1;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// li r30,1
	R(state, 30) = 1;
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// cmpwi cr6,r22,0
	Compare(state.cr6, state, S(R(state, 22)), 0, true);
	// stb r24,5(r11)
	PPC_STORE_U8(W(R(state, 11)) + 5, static_cast<std::uint8_t>(R(state, 24)));
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r11,41(r11)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 11)) + 41);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// cmplwi cr6,r31,0
	Compare(state.cr6, state, W(R(state, 31)), 0, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// stb r11,0(r4)
	PPC_STORE_U8(W(R(state, 4)) + 0, static_cast<std::uint8_t>(R(state, 11)));
	// li r30,2
	R(state, 30) = 2;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// cmpwi cr6,r22,1
	Compare(state.cr6, state, S(R(state, 22)), 1, true);
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// addi r4,r4,1
	R(state, 4) = R(state, 4) + 1;
	// stb r24,41(r11)
	PPC_STORE_U8(W(R(state, 11)) + 41, static_cast<std::uint8_t>(R(state, 24)));
	// bne cr6,0x82b856d0
	if (!state.cr6.eq) goto loc_82B856D0;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r11,42(r11)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 11)) + 42);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// cmplwi cr6,r31,0
	Compare(state.cr6, state, W(R(state, 31)), 0, false);
	// beq cr6,0x82b856d0
	if (state.cr6.eq) goto loc_82B856D0;
	// stb r11,0(r4)
	PPC_STORE_U8(W(R(state, 4)) + 0, static_cast<std::uint8_t>(R(state, 11)));
	// li r30,3
	R(state, 30) = 3;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// addi r4,r4,1
	R(state, 4) = R(state, 4) + 1;
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// stb r24,42(r11)
	PPC_STORE_U8(W(R(state, 11)) + 42, static_cast<std::uint8_t>(R(state, 24)));
loc_82B856D0:
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// li r7,0
	R(state, 7) = 0;
	// addi r6,r1,84
	R(state, 6) = R(state, 1) + 84;
	// mr r5,r31
	R(state, 5) = R(state, 31);
	// lwzx r3,r28,r11
	R(state, 3) = PPC_LOAD_U32(W(R(state, 28)) + W(R(state, 11)));
	// bl 0x82be2dd8
	state.lr = 0x82B856E8;
	Direct(0x82be2dd8u, memory, deps, state);
	// cmpwi r3,0
	Compare(state.cr0, state, S(R(state, 3)), 0, true);
	// beq 0x82b85a38
	if (state.cr0.eq) goto loc_82B85A38;
	// lwz r10,84(r1)
	R(state, 10) = PPC_LOAD_U32(W(R(state, 1)) + 84);
	// cmpwi cr6,r10,0
	Compare(state.cr6, state, S(R(state, 10)), 0, true);
	// blt cr6,0x82b85a38
	if (state.cr6.lt) goto loc_82B85A38;
	// cmplw cr6,r10,r31
	Compare(state.cr6, state, W(R(state, 10)), W(R(state, 31)), false);
	// bgt cr6,0x82b85a38
	if (state.cr6.gt) goto loc_82B85A38;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r30,r10,r30
	R(state, 30) = R(state, 10) + R(state, 30);
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r9,4(r11)
	R(state, 9) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// rlwinm. r9,r9,0,0,24
	R(state, 9) = std::rotl(W(R(state, 9)) | (R(state, 9) << 32), 0) & 0xFFFFFF80;
	Compare(state.cr0, state, S(R(state, 9)), 0, true);
	// beq 0x82b859f0
	if (state.cr0.eq) goto loc_82B859F0;
	// cmpwi cr6,r10,0
	Compare(state.cr6, state, S(R(state, 10)), 0, true);
	// beq cr6,0x82b8573c
	if (state.cr6.eq) goto loc_82B8573C;
	// lbz r10,0(r23)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 23)) + 0);
	// cmplwi cr6,r10,10
	Compare(state.cr6, state, W(R(state, 10)), 10, false);
	// bne cr6,0x82b8573c
	if (!state.cr6.eq) goto loc_82B8573C;
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// ori r10,r10,4
	R(state, 10) = R(state, 10) | 4;
	// b 0x82b85748
	goto loc_82B85748;
loc_82B8573C:
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// extsb r10,r10
	R(state, 10) = static_cast<std::int8_t>(R(state, 10));
	// rlwinm r10,r10,0,30,28
	R(state, 10) = std::rotl(W(R(state, 10)) | (R(state, 10) << 32), 0) & 0xFFFFFFFFFFFFFFFB;
loc_82B85748:
	// add r25,r23,r30
	R(state, 25) = R(state, 23) + R(state, 30);
	// stb r10,4(r11)
	PPC_STORE_U8(W(R(state, 11)) + 4, static_cast<std::uint8_t>(R(state, 10)));
	// mr r31,r23
	R(state, 31) = R(state, 23);
	// mr r30,r23
	R(state, 30) = R(state, 23);
	// cmplw cr6,r23,r25
	Compare(state.cr6, state, W(R(state, 23)), W(R(state, 25)), false);
	// bge cr6,0x82b858a8
	if (!state.cr6.lt) goto loc_82B858A8;
	// li r26,13
	R(state, 26) = 13;
loc_82B85764:
	// lbz r11,0(r30)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 30)) + 0);
	// extsb r10,r11
	R(state, 10) = static_cast<std::int8_t>(R(state, 11));
	// cmpwi cr6,r10,26
	Compare(state.cr6, state, S(R(state, 10)), 26, true);
	// beq cr6,0x82b85878
	if (state.cr6.eq) goto loc_82B85878;
	// cmpwi cr6,r10,13
	Compare(state.cr6, state, S(R(state, 10)), 13, true);
	// beq cr6,0x82b85788
	if (state.cr6.eq) goto loc_82B85788;
	// addi r30,r30,1
	R(state, 30) = R(state, 30) + 1;
loc_82B85780:
	// stb r11,0(r31)
	PPC_STORE_U8(W(R(state, 31)) + 0, static_cast<std::uint8_t>(R(state, 11)));
	// b 0x82b85868
	goto loc_82B85868;
loc_82B85788:
	// addi r10,r25,-1
	R(state, 10) = R(state, 25) + -1;
	// cmplw cr6,r30,r10
	Compare(state.cr6, state, W(R(state, 30)), W(R(state, 10)), false);
	// bge cr6,0x82b857b8
	if (!state.cr6.lt) goto loc_82B857B8;
	// lbz r9,1(r30)
	R(state, 9) = PPC_LOAD_U8(W(R(state, 30)) + 1);
	// addi r10,r30,1
	R(state, 10) = R(state, 30) + 1;
	// cmplwi cr6,r9,10
	Compare(state.cr6, state, W(R(state, 9)), 10, false);
	// bne cr6,0x82b857b0
	if (!state.cr6.eq) goto loc_82B857B0;
	// addi r30,r30,2
	R(state, 30) = R(state, 30) + 2;
loc_82B857A8:
	// stb r24,0(r31)
	PPC_STORE_U8(W(R(state, 31)) + 0, static_cast<std::uint8_t>(R(state, 24)));
	// b 0x82b85868
	goto loc_82B85868;
loc_82B857B0:
	// mr r30,r10
	R(state, 30) = R(state, 10);
	// b 0x82b85780
	goto loc_82B85780;
loc_82B857B8:
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// li r7,0
	R(state, 7) = 0;
	// addi r6,r1,84
	R(state, 6) = R(state, 1) + 84;
	// li r5,1
	R(state, 5) = 1;
	// addi r4,r1,80
	R(state, 4) = R(state, 1) + 80;
	// addi r30,r30,1
	R(state, 30) = R(state, 30) + 1;
	// lwzx r3,r28,r11
	R(state, 3) = PPC_LOAD_U32(W(R(state, 28)) + W(R(state, 11)));
	// bl 0x82be2dd8
	state.lr = 0x82B857D8;
	Direct(0x82be2dd8u, memory, deps, state);
	// cmpwi r3,0
	Compare(state.cr0, state, S(R(state, 3)), 0, true);
	// bne 0x82b857ec
	if (!state.cr0.eq) goto loc_82B857EC;
	// bl 0x822ca100
	state.lr = 0x82B857E4;
	Direct(0x822ca100u, memory, deps, state);
	// cmplwi r3,0
	Compare(state.cr0, state, W(R(state, 3)), 0, false);
	// bne 0x82b85864
	if (!state.cr0.eq) goto loc_82B85864;
loc_82B857EC:
	// lwz r11,84(r1)
	R(state, 11) = PPC_LOAD_U32(W(R(state, 1)) + 84);
	// cmpwi cr6,r11,0
	Compare(state.cr6, state, S(R(state, 11)), 0, true);
	// beq cr6,0x82b85864
	if (state.cr6.eq) goto loc_82B85864;
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r11,4(r11)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// andi. r11,r11,72
	R(state, 11) = R(state, 11) & 72;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// cmpwi r11,0
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82b85834
	if (state.cr0.eq) goto loc_82B85834;
	// lbz r11,80(r1)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 1)) + 80);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b857a8
	if (state.cr6.eq) goto loc_82B857A8;
	// stb r26,0(r31)
	PPC_STORE_U8(W(R(state, 31)) + 0, static_cast<std::uint8_t>(R(state, 26)));
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// lbz r10,80(r1)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 1)) + 80);
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// stb r10,5(r11)
	PPC_STORE_U8(W(R(state, 11)) + 5, static_cast<std::uint8_t>(R(state, 10)));
	// b 0x82b85868
	goto loc_82B85868;
loc_82B85834:
	// cmplw cr6,r31,r23
	Compare(state.cr6, state, W(R(state, 31)), W(R(state, 23)), false);
	// bne cr6,0x82b85848
	if (!state.cr6.eq) goto loc_82B85848;
	// lbz r11,80(r1)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 1)) + 80);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b857a8
	if (state.cr6.eq) goto loc_82B857A8;
loc_82B85848:
	// li r5,1
	R(state, 5) = 1;
	// li r4,-1
	R(state, 4) = -1;
	// mr r3,r21
	R(state, 3) = R(state, 21);
	// bl 0x82b85ea8
	state.lr = 0x82B85858;
	Direct(0x82b85ea8u, memory, deps, state);
	// lbz r11,80(r1)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 1)) + 80);
	// cmplwi cr6,r11,10
	Compare(state.cr6, state, W(R(state, 11)), 10, false);
	// beq cr6,0x82b8586c
	if (state.cr6.eq) goto loc_82B8586C;
loc_82B85864:
	// stb r26,0(r31)
	PPC_STORE_U8(W(R(state, 31)) + 0, static_cast<std::uint8_t>(R(state, 26)));
loc_82B85868:
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
loc_82B8586C:
	// cmplw cr6,r30,r25
	Compare(state.cr6, state, W(R(state, 30)), W(R(state, 25)), false);
	// blt cr6,0x82b85764
	if (state.cr6.lt) goto loc_82B85764;
	// b 0x82b858a8
	goto loc_82B858A8;
loc_82B85878:
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// rlwinm. r10,r10,0,25,25
	R(state, 10) = std::rotl(W(R(state, 10)) | (R(state, 10) << 32), 0) & 0x40;
	Compare(state.cr0, state, S(R(state, 10)), 0, true);
	// bne 0x82b8589c
	if (!state.cr0.eq) goto loc_82B8589C;
	// lbz r10,4(r11)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 11)) + 4);
	// ori r10,r10,2
	R(state, 10) = R(state, 10) | 2;
	// stb r10,4(r11)
	PPC_STORE_U8(W(R(state, 11)) + 4, static_cast<std::uint8_t>(R(state, 10)));
	// b 0x82b858a8
	goto loc_82B858A8;
loc_82B8589C:
	// lbz r11,0(r30)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 30)) + 0);
	// stb r11,0(r31)
	PPC_STORE_U8(W(R(state, 31)) + 0, static_cast<std::uint8_t>(R(state, 11)));
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
loc_82B858A8:
	// subf r30,r23,r31
	R(state, 30) = R(state, 31) - R(state, 23);
	// cmpwi cr6,r22,1
	Compare(state.cr6, state, S(R(state, 22)), 1, true);
	// bne cr6,0x82b859f0
	if (!state.cr6.eq) goto loc_82B859F0;
	// cmpwi cr6,r30,0
	Compare(state.cr6, state, S(R(state, 30)), 0, true);
	// beq cr6,0x82b859f0
	if (state.cr6.eq) goto loc_82B859F0;
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// lbz r10,0(r31)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 31)) + 0);
	// rlwinm. r11,r10,0,0,24
	R(state, 11) = std::rotl(W(R(state, 10)) | (R(state, 10) << 32), 0) & 0xFFFFFF80;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// bne 0x82b858d4
	if (!state.cr0.eq) goto loc_82B858D4;
	// addi r31,r31,1
	R(state, 31) = R(state, 31) + 1;
	// b 0x82b859b8
	goto loc_82B859B8;
loc_82B858D4:
	// lis r11,-31967
	R(state, 11) = -2094989312;
	// li r8,1
	R(state, 8) = 1;
	// addi r11,r11,23376
	R(state, 11) = R(state, 11) + 23376;
	// b 0x82b85900
	goto loc_82B85900;
loc_82B858E4:
	// cmpwi cr6,r8,4
	Compare(state.cr6, state, S(R(state, 8)), 4, true);
	// bgt cr6,0x82b8590c
	if (state.cr6.gt) goto loc_82B8590C;
	// cmplw cr6,r31,r23
	Compare(state.cr6, state, W(R(state, 31)), W(R(state, 23)), false);
	// blt cr6,0x82b8590c
	if (state.cr6.lt) goto loc_82B8590C;
	// addi r31,r31,-1
	R(state, 31) = R(state, 31) + -1;
	// addi r8,r8,1
	R(state, 8) = R(state, 8) + 1;
	// lbz r10,0(r31)
	R(state, 10) = PPC_LOAD_U8(W(R(state, 31)) + 0);
loc_82B85900:
	// lbzx r10,r10,r11
	R(state, 10) = PPC_LOAD_U8(W(R(state, 10)) + W(R(state, 11)));
	// cmplwi cr6,r10,0
	Compare(state.cr6, state, W(R(state, 10)), 0, false);
	// beq cr6,0x82b858e4
	if (state.cr6.eq) goto loc_82B858E4;
loc_82B8590C:
	// lbz r9,0(r31)
	R(state, 9) = PPC_LOAD_U8(W(R(state, 31)) + 0);
	// mr r10,r9
	R(state, 10) = R(state, 9);
	// lbzx r11,r10,r11
	R(state, 11) = PPC_LOAD_U8(W(R(state, 10)) + W(R(state, 11)));
	// extsb. r11,r11
	R(state, 11) = static_cast<std::int8_t>(R(state, 11));
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// bne 0x82b85930
	if (!state.cr0.eq) goto loc_82B85930;
	// bl 0x82b7fd78
	state.lr = 0x82B85924;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r11,42
	R(state, 11) = 42;
loc_82B85928:
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
	// b 0x82b859ec
	goto loc_82B859EC;
loc_82B85930:
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// cmpw cr6,r11,r8
	Compare(state.cr6, state, S(R(state, 11)), S(R(state, 8)), true);
	// bne cr6,0x82b85944
	if (!state.cr6.eq) goto loc_82B85944;
	// add r31,r8,r31
	R(state, 31) = R(state, 8) + R(state, 31);
	// b 0x82b859b8
	goto loc_82B859B8;
loc_82B85944:
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// add r10,r28,r11
	R(state, 10) = R(state, 28) + R(state, 11);
	// lbz r11,4(r10)
	R(state, 11) = PPC_LOAD_U8(W(R(state, 10)) + 4);
	// andi. r11,r11,72
	R(state, 11) = R(state, 11) & 72;
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// cmpwi r11,0
	Compare(state.cr0, state, S(R(state, 11)), 0, true);
	// beq 0x82b859a4
	if (state.cr0.eq) goto loc_82B859A4;
	// addi r11,r31,1
	R(state, 11) = R(state, 31) + 1;
	// stb r9,5(r10)
	PPC_STORE_U8(W(R(state, 10)) + 5, static_cast<std::uint8_t>(R(state, 9)));
	// cmpwi cr6,r8,2
	Compare(state.cr6, state, S(R(state, 8)), 2, true);
	// blt cr6,0x82b85980
	if (state.cr6.lt) goto loc_82B85980;
	// lwzx r10,r27,r29
	R(state, 10) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// lbz r9,0(r11)
	R(state, 9) = PPC_LOAD_U8(W(R(state, 11)) + 0);
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// add r10,r28,r10
	R(state, 10) = R(state, 28) + R(state, 10);
	// stb r9,41(r10)
	PPC_STORE_U8(W(R(state, 10)) + 41, static_cast<std::uint8_t>(R(state, 9)));
loc_82B85980:
	// cmpwi cr6,r8,3
	Compare(state.cr6, state, S(R(state, 8)), 3, true);
	// bne cr6,0x82b8599c
	if (!state.cr6.eq) goto loc_82B8599C;
	// lwzx r10,r27,r29
	R(state, 10) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// lbz r9,0(r11)
	R(state, 9) = PPC_LOAD_U8(W(R(state, 11)) + 0);
	// addi r11,r11,1
	R(state, 11) = R(state, 11) + 1;
	// add r10,r28,r10
	R(state, 10) = R(state, 28) + R(state, 10);
	// stb r9,42(r10)
	PPC_STORE_U8(W(R(state, 10)) + 42, static_cast<std::uint8_t>(R(state, 9)));
loc_82B8599C:
	// subf r31,r8,r11
	R(state, 31) = R(state, 11) - R(state, 8);
	// b 0x82b859b8
	goto loc_82B859B8;
loc_82B859A4:
	// neg r11,r8
	R(state, 11) = -R(state, 8);
	// li r5,1
	R(state, 5) = 1;
	// extsw r4,r11
	R(state, 4) = S(R(state, 11));
	// mr r3,r21
	R(state, 3) = R(state, 21);
	// bl 0x82b85ea8
	state.lr = 0x82B859B8;
	Direct(0x82b85ea8u, memory, deps, state);
loc_82B859B8:
	// subf r31,r23,r31
	R(state, 31) = R(state, 31) - R(state, 23);
	// lis r3,0
	R(state, 3) = 0;
	// rlwinm r8,r20,31,1,31
	R(state, 8) = std::rotl(W(R(state, 20)) | (R(state, 20) << 32), 31) & 0x7FFFFFFF;
	// mr r7,r18
	R(state, 7) = R(state, 18);
	// mr r5,r23
	R(state, 5) = R(state, 23);
	// li r4,0
	R(state, 4) = 0;
	// ori r3,r3,65001
	R(state, 3) = R(state, 3) | 65001;
	// mr r6,r31
	R(state, 6) = R(state, 31);
	// bl 0x8229c560
	state.lr = 0x82B859DC;
	Direct(0x8229c560u, memory, deps, state);
	// mr. r30,r3
	R(state, 30) = R(state, 3);
	Compare(state.cr0, state, S(R(state, 30)), 0, true);
	// bne 0x82b85a14
	if (!state.cr0.eq) goto loc_82B85A14;
	// bl 0x822ca100
	state.lr = 0x82B859E8;
	Direct(0x822ca100u, memory, deps, state);
loc_82B859E8:
	// bl 0x82b7fde8
	state.lr = 0x82B859EC;
	Direct(0x82b7fde8u, memory, deps, state);
loc_82B859EC:
	// li r19,-1
	R(state, 19) = -1;
loc_82B859F0:
	// cmplw cr6,r23,r18
	Compare(state.cr6, state, W(R(state, 23)), W(R(state, 18)), false);
	// beq cr6,0x82b85a00
	if (state.cr6.eq) goto loc_82B85A00;
	// mr r3,r23
	R(state, 3) = R(state, 23);
	// bl 0x823addc0
	state.lr = 0x82B85A00;
	Direct(0x823addc0u, memory, deps, state);
loc_82B85A00:
	// cmpwi cr6,r19,-2
	Compare(state.cr6, state, S(R(state, 19)), -2, true);
	// mr r3,r30
	R(state, 3) = R(state, 30);
	// beq cr6,0x82b85a70
	if (state.cr6.eq) goto loc_82B85A70;
	// mr r3,r19
	R(state, 3) = R(state, 19);
	// b 0x82b85a70
	goto loc_82B85A70;
loc_82B85A14:
	// subf r10,r30,r31
	R(state, 10) = R(state, 31) - R(state, 30);
	// lwzx r11,r27,r29
	R(state, 11) = PPC_LOAD_U32(W(R(state, 27)) + W(R(state, 29)));
	// rlwinm r30,r30,1,0,30
	R(state, 30) = std::rotl(W(R(state, 30)) | (R(state, 30) << 32), 1) & 0xFFFFFFFE;
	// cntlzw r10,r10
	R(state, 10) = W(R(state, 10)) == 0 ? 32 : std::countl_zero(W(R(state, 10)));
	// add r11,r28,r11
	R(state, 11) = R(state, 28) + R(state, 11);
	// rlwinm r10,r10,27,31,31
	R(state, 10) = std::rotl(W(R(state, 10)) | (R(state, 10) << 32), 27) & 0x1;
	// xori r10,r10,1
	R(state, 10) = R(state, 10) ^ 1;
	// stw r10,56(r11)
	PPC_STORE_U32(W(R(state, 11)) + 56, W(R(state, 10)));
	// b 0x82b859f0
	goto loc_82B859F0;
loc_82B85A38:
	// bl 0x822ca100
	state.lr = 0x82B85A3C;
	Direct(0x822ca100u, memory, deps, state);
	// cmplwi cr6,r3,5
	Compare(state.cr6, state, W(R(state, 3)), 5, false);
	// bne cr6,0x82b85a5c
	if (!state.cr6.eq) goto loc_82B85A5C;
	// bl 0x82b7fd78
	state.lr = 0x82B85A48;
	Direct(0x82b7fd78u, memory, deps, state);
	// li r11,9
	R(state, 11) = 9;
	// stw r11,0(r3)
	PPC_STORE_U32(W(R(state, 3)) + 0, W(R(state, 11)));
	// bl 0x82b7fdb0
	state.lr = 0x82B85A54;
	Direct(0x82b7fdb0u, memory, deps, state);
	// li r11,5
	R(state, 11) = 5;
	// b 0x82b85928
	goto loc_82B85928;
loc_82B85A5C:
	// cmplwi cr6,r3,109
	Compare(state.cr6, state, W(R(state, 3)), 109, false);
	// bne cr6,0x82b859e8
	if (!state.cr6.eq) goto loc_82B859E8;
	// mr r19,r26
	R(state, 19) = R(state, 26);
	// b 0x82b859f0
	goto loc_82B859F0;
loc_82B85A6C:
	// li r3,0
	R(state, 3) = 0;
loc_82B85A70:
	// addi r1,r1,208
	R(state, 1) = R(state, 1) + 208;
	// b 0x82b7a710
	Restore(memory, state);
	return;

}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82b85448u) return false;
    ReadBody(memory, dependencies, registers);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_read_routes
