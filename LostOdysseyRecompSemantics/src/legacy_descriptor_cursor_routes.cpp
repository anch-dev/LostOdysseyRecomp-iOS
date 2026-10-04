#include "lo_semantics/legacy_descriptor_cursor_routes.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::legacy_descriptor_cursor_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
std::uint64_t& R(Registers& s, unsigned n)
{ return n == 1u ? s.integer.sp : s.integer.r[n]; }
std::int32_t Signed32(std::uint64_t x)
{ return std::bit_cast<std::int32_t>(Address(x)); }
std::uint8_t LowByte(std::uint64_t x)
{ return static_cast<std::uint8_t>(x); }
template<class T>
void Compare(const Registers& s, crt_stream_operations::Condition& c, T a, T b)
{ c={std::uint8_t(a<b),std::uint8_t(a>b),std::uint8_t(a==b),s.integer.xer_so}; }
void Save28(GuestMemory& memory, Registers& state)
{
    for(unsigned n=28u;n<32u;++n)
        WriteU64(memory,Address(R(state,1)-8u*(33u-n)),R(state,n));
    memory.WriteU32(Address(R(state,1)-8u),Address(R(state,12)));
}
void Restore28(GuestMemory& memory, Registers& state)
{
    for(unsigned n=28u;n<32u;++n)
        R(state,n)=ReadU64(memory,Address(R(state,1)-8u*(33u-n)));
    R(state,12)=memory.ReadU32(Address(R(state,1)-8u));
    state.integer.lr=R(state,12);
}
void Execute8302B2F8(GuestMemory&,GuestBoundaryServices&,Registers&);
void Execute8302B5C8(GuestMemory&,GuestBoundaryServices&,Registers&);
void Execute8302B620(GuestMemory&,GuestBoundaryServices&,Registers&);
void Execute83053770(GuestMemory&,GuestBoundaryServices&,Registers&);
void Execute82F99F48(GuestMemory&,GuestBoundaryServices&,Registers&);
void Direct(GuestAddress entry,GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    if(entry==0x82b7a0b0u)
    { if(!memory_copy_context::Apply(entry,memory,state))throw std::logic_error("selected copy missing");return; }
    switch(entry)
    {
    case 0x8302b2f8u: Execute8302B2F8(memory,services,state);return;
    case 0x8302b5c8u: Execute8302B5C8(memory,services,state);return;
    case 0x8302b620u: Execute8302B620(memory,services,state);return;
    case 0x83053770u: Execute83053770(memory,services,state);return;
    case 0x82f99f48u: Execute82F99F48(memory,services,state);return;
    default: services.Call(entry,memory,state);return;
    }
}
void Execute8302B2F8(GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    std::uint64_t temporary=0;
	// mflr r12
	R(state,12) = state.integer.lr;
	// bl 0x82b7a6e8
	state.integer.lr = 0x8302B300;
	Save28(memory,state);
	// stwu r1,-144(r1)
	temporary = R(state,1) + std::uint64_t(-144);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// mr r31,r3
	R(state,31) = R(state,3);
	// mr r30,r4
	R(state,30) = R(state,4);
	// li r29,1
	R(state,29) = 1;
	// li r28,0
	R(state,28) = 0;
loc_8302B314:
	// cmplwi cr6,r30,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,30)),0);
	// bne cr6,0x8302b33c
	if (!state.integer.cr6.eq) goto loc_8302B33C;
	// lwz r11,28(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 28));
	// clrlwi. r10,r11,31
	R(state,10) = Address(R(state,11)) & 0x1;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,10)),0);
	// bne 0x8302b508
	if (!state.integer.cr0.eq) goto loc_8302B508;
	// cmplwi r11,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,11)),0);
	// beq 0x8302b508
	if (state.integer.cr0.eq) goto loc_8302B508;
	// mr r3,r31
	R(state,3) = R(state,31);
	// bl 0x83028fe0
	state.integer.lr = 0x8302B338;
	Direct(0x83028fe0u,memory,services,state);
	// mr r30,r3
	R(state,30) = R(state,3);
loc_8302B33C:
	// lwz r11,4(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 4));
	// cmpwi cr6,r11,1
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,11)),1);
	// beq cr6,0x8302b468
	if (state.integer.cr6.eq) goto loc_8302B468;
	// cmpwi cr6,r11,6
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,11)),6);
	// beq cr6,0x8302b410
	if (state.integer.cr6.eq) goto loc_8302B410;
	// cmpwi cr6,r11,8
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,11)),8);
	// bne cr6,0x8302b514
	if (!state.integer.cr6.eq) goto loc_8302B514;
	// lwz r10,20(r30)
	R(state,10) = memory.ReadU32(Address(Address(R(state,30)) + 20));
	// addi r11,r30,20
	R(state,11) = R(state,30) + 20;
	// cmplwi cr6,r10,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),0);
	// beq cr6,0x8302b574
	if (state.integer.cr6.eq) goto loc_8302B574;
	// lwz r30,16(r30)
	R(state,30) = memory.ReadU32(Address(Address(R(state,30)) + 16));
	// cmplwi r30,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,30)),0);
	// beq 0x8302b588
	if (state.integer.cr0.eq) goto loc_8302B588;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// cmplwi cr6,r11,1
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,11)),1);
	// ble cr6,0x8302b500
	if (!state.integer.cr6.gt) goto loc_8302B500;
	// addi r11,r11,-2
	R(state,11) = R(state,11) + -2;
	// lwz r10,12(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 12));
	// addi r3,r31,24
	R(state,3) = R(state,31) + 24;
	// stw r30,80(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 80), Address(R(state,30)));
	// stb r29,84(r1)
	memory.WriteU8(Address(Address(R(state,1)) + 84), LowByte(R(state,29)));
	// stw r11,88(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 88), Address(R(state,11)));
	// lwz r11,16(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 16));
	// stw r10,92(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 92), Address(R(state,10)));
	// lwz r10,8(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// stw r11,100(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 100), Address(R(state,11)));
	// lwz r11,4(r3)
	R(state,11) = memory.ReadU32(Address(Address(R(state,3)) + 4));
	// stw r10,96(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 96), Address(R(state,10)));
	// clrlwi. r11,r11,31
	R(state,11) = Address(R(state,11)) & 0x1;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,11)),0);
	// bne 0x8302b3dc
	if (!state.integer.cr0.eq) goto loc_8302B3DC;
	// lwz r11,0(r3)
	R(state,11) = memory.ReadU32(Address(Address(R(state,3)) + 0));
	// rlwinm r11,r11,0,0,30
	R(state,11) = WordRotateMask(R(state,11),0,0xFFFFFFFEull);
	// addic. r11,r11,-4
	state.integer.xer_ca = Address(R(state,11)) > 3;
	R(state,11) = R(state,11) + -4;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,11)),0);
	// beq 0x8302b3dc
	if (state.integer.cr0.eq) goto loc_8302B3DC;
	// lwz r10,8(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 8));
	// lwz r9,12(r11)
	R(state,9) = memory.ReadU32(Address(Address(R(state,11)) + 12));
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// cmplw cr6,r10,r9
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),Address(R(state,9)));
	// ble cr6,0x8302b3e8
	if (!state.integer.cr6.gt) goto loc_8302B3E8;
loc_8302B3DC:
	// li r4,1
	R(state,4) = 1;
	// bl 0x83026518
	state.integer.lr = 0x8302B3E4;
	Direct(0x83026518u,memory,services,state);
	// mr r11,r3
	R(state,11) = R(state,3);
loc_8302B3E8:
	// lwz r10,8(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 8));
	// addi r4,r1,80
	R(state,4) = R(state,1) + 80;
	// li r5,24
	R(state,5) = 24;
	// mulli r9,r10,24
	R(state,9) = R(state,10) * 24;
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// add r9,r9,r11
	R(state,9) = R(state,9) + R(state,11);
	// addi r3,r9,16
	R(state,3) = R(state,9) + 16;
	// stw r10,8(r11)
	memory.WriteU32(Address(Address(R(state,11)) + 8), Address(R(state,10)));
	// bl 0x82b7a0b0
	state.integer.lr = 0x8302B40C;
	Direct(0x82b7a0b0u,memory,services,state);
	// b 0x8302b500
	goto loc_8302B500;
loc_8302B410:
	// lwz r11,16(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 16));
	// cmpwi cr6,r11,1
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,11)),1);
	// bne cr6,0x8302b59c
	if (!state.integer.cr6.eq) goto loc_8302B59C;
	// lwz r11,24(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 24));
	// lwz r10,4(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 4));
	// cmpwi cr6,r10,11
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,10)),11);
	// bne cr6,0x8302b5b0
	if (!state.integer.cr6.eq) goto loc_8302B5B0;
	// lwz r10,8(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// cmplwi cr6,r10,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),0);
	// bne cr6,0x8302b444
	if (!state.integer.cr6.eq) goto loc_8302B444;
	// lwz r10,60(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 60));
	// stw r28,20(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 20), Address(R(state,28)));
	// stw r10,8(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 8), Address(R(state,10)));
loc_8302B444:
	// lwz r10,12(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 12));
	// cmplwi cr6,r10,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),0);
	// bne cr6,0x8302b458
	if (!state.integer.cr6.eq) goto loc_8302B458;
	// lwz r10,64(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 64));
	// stw r10,12(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 12), Address(R(state,10)));
loc_8302B458:
	// lwz r10,72(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 72));
	// stw r10,16(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 16), Address(R(state,10)));
	// lwz r30,48(r11)
	R(state,30) = memory.ReadU32(Address(Address(R(state,11)) + 48));
	// b 0x8302b314
	goto loc_8302B314;
loc_8302B468:
	// lwz r11,12(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 12));
	// cmplwi r11,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,11)),0);
	// beq 0x8302b4fc
	if (state.integer.cr0.eq) goto loc_8302B4FC;
	// stw r11,80(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 80), Address(R(state,11)));
	// addi r3,r31,24
	R(state,3) = R(state,31) + 24;
	// lwz r11,8(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 8));
	// lwz r10,12(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 12));
	// stw r28,88(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 88), Address(R(state,28)));
	// stb r28,84(r1)
	memory.WriteU8(Address(Address(R(state,1)) + 84), LowByte(R(state,28)));
	// stw r11,96(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 96), Address(R(state,11)));
	// lwz r11,16(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 16));
	// stw r10,92(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 92), Address(R(state,10)));
	// stw r11,100(r1)
	memory.WriteU32(Address(Address(R(state,1)) + 100), Address(R(state,11)));
	// lwz r11,4(r3)
	R(state,11) = memory.ReadU32(Address(Address(R(state,3)) + 4));
	// clrlwi. r11,r11,31
	R(state,11) = Address(R(state,11)) & 0x1;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,11)),0);
	// bne 0x8302b4cc
	if (!state.integer.cr0.eq) goto loc_8302B4CC;
	// lwz r11,0(r3)
	R(state,11) = memory.ReadU32(Address(Address(R(state,3)) + 0));
	// rlwinm r11,r11,0,0,30
	R(state,11) = WordRotateMask(R(state,11),0,0xFFFFFFFEull);
	// addic. r11,r11,-4
	state.integer.xer_ca = Address(R(state,11)) > 3;
	R(state,11) = R(state,11) + -4;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,11)),0);
	// beq 0x8302b4cc
	if (state.integer.cr0.eq) goto loc_8302B4CC;
	// lwz r10,8(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 8));
	// lwz r9,12(r11)
	R(state,9) = memory.ReadU32(Address(Address(R(state,11)) + 12));
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// cmplw cr6,r10,r9
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),Address(R(state,9)));
	// ble cr6,0x8302b4d8
	if (!state.integer.cr6.gt) goto loc_8302B4D8;
loc_8302B4CC:
	// li r4,1
	R(state,4) = 1;
	// bl 0x83026518
	state.integer.lr = 0x8302B4D4;
	Direct(0x83026518u,memory,services,state);
	// mr r11,r3
	R(state,11) = R(state,3);
loc_8302B4D8:
	// lwz r10,8(r11)
	R(state,10) = memory.ReadU32(Address(Address(R(state,11)) + 8));
	// addi r4,r1,80
	R(state,4) = R(state,1) + 80;
	// li r5,24
	R(state,5) = 24;
	// mulli r9,r10,24
	R(state,9) = R(state,10) * 24;
	// addi r10,r10,1
	R(state,10) = R(state,10) + 1;
	// add r9,r9,r11
	R(state,9) = R(state,9) + R(state,11);
	// addi r3,r9,16
	R(state,3) = R(state,9) + 16;
	// stw r10,8(r11)
	memory.WriteU32(Address(Address(R(state,11)) + 8), Address(R(state,10)));
	// bl 0x82b7a0b0
	state.integer.lr = 0x8302B4FC;
	Direct(0x82b7a0b0u,memory,services,state);
loc_8302B4FC:
	// lwz r30,8(r30)
	R(state,30) = memory.ReadU32(Address(Address(R(state,30)) + 8));
loc_8302B500:
	// stb r29,32(r31)
	memory.WriteU8(Address(Address(R(state,31)) + 32), LowByte(R(state,29)));
	// b 0x8302b314
	goto loc_8302B314;
loc_8302B508:
	// li r3,0
	R(state,3) = 0;
loc_8302B50C:
	// addi r1,r1,144
	R(state,1) = R(state,1) + 144;
	// b 0x82b7a738
	Restore28(memory,state);
	return;
loc_8302B514:
	// lwz r11,4(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 4));
	// cmpwi cr6,r11,9
	Compare<std::int32_t>(state,state.integer.cr6,Signed32(R(state,11)),9);
	// beq cr6,0x8302b534
	if (state.integer.cr6.eq) goto loc_8302B534;
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B534;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B534:
	// stw r30,0(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 0), Address(R(state,30)));
	// lwz r10,28(r30)
	R(state,10) = memory.ReadU32(Address(Address(R(state,30)) + 28));
	// cmplwi r10,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,10)),0);
	// beq 0x8302b560
	if (state.integer.cr0.eq) goto loc_8302B560;
	// lwz r11,32(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 32));
	// cmplwi r11,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,11)),0);
	// beq 0x8302b560
	if (state.integer.cr0.eq) goto loc_8302B560;
	// mullw r11,r11,r10
	R(state,11) = std::int64_t(Signed32(R(state,11))) * std::int64_t(Signed32(R(state,10)));
	// li r3,1
	R(state,3) = 1;
	// stw r11,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,11)));
	// b 0x8302b50c
	goto loc_8302B50C;
loc_8302B560:
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B574;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B574:
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B588;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B588:
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B59C;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B59C:
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B5B0;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B5B0:
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B5C4;
	Direct(0x82f99f48u,memory,services,state);
}
void Execute8302B5C8(GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    std::uint64_t temporary=0;
	// mflr r12
	R(state,12) = state.integer.lr;
	// stw r12,-8(r1)
	memory.WriteU32(Address(Address(R(state,1)) + -8), Address(R(state,12)));
	// std r31,-16(r1)
	WriteU64(memory,Address(Address(R(state,1)) + -16), R(state,31));
	// stwu r1,-96(r1)
	temporary = R(state,1) + std::uint64_t(-96);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// mr r31,r3
	R(state,31) = R(state,3);
	// lwz r11,28(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 28));
	// clrlwi. r10,r11,31
	R(state,10) = Address(R(state,11)) & 0x1;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,10)),0);
	// bne 0x8302b5f0
	if (!state.integer.cr0.eq) goto loc_8302B5F0;
	// cmplwi r11,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,11)),0);
	// bne 0x8302b5f8
	if (!state.integer.cr0.eq) goto loc_8302B5F8;
loc_8302B5F0:
	// li r3,0
	R(state,3) = 0;
	// b 0x8302b60c
	goto loc_8302B60C;
loc_8302B5F8:
	// mr r3,r31
	R(state,3) = R(state,31);
	// bl 0x83028fe0
	state.integer.lr = 0x8302B600;
	Direct(0x83028fe0u,memory,services,state);
	// mr r4,r3
	R(state,4) = R(state,3);
	// mr r3,r31
	R(state,3) = R(state,31);
	// bl 0x8302b2f8
	state.integer.lr = 0x8302B60C;
	Direct(0x8302b2f8u,memory,services,state);
loc_8302B60C:
	// addi r1,r1,96
	R(state,1) = R(state,1) + 96;
	// lwz r12,-8(r1)
	R(state,12) = memory.ReadU32(Address(Address(R(state,1)) + -8));
	// mtlr r12
	state.integer.lr = R(state,12);
	// ld r31,-16(r1)
	R(state,31) = ReadU64(memory,Address(Address(R(state,1)) + -16));
	// blr
	return;
}
void Execute8302B620(GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    std::uint64_t temporary=0;
	// mflr r12
	R(state,12) = state.integer.lr;
	// stw r12,-8(r1)
	memory.WriteU32(Address(Address(R(state,1)) + -8), Address(R(state,12)));
	// std r31,-16(r1)
	WriteU64(memory,Address(Address(R(state,1)) + -16), R(state,31));
	// stwu r1,-96(r1)
	temporary = R(state,1) + std::uint64_t(-96);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// mr r31,r3
	R(state,31) = R(state,3);
	// li r11,0
	R(state,11) = 0;
	// lwz r10,4(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// stb r11,32(r31)
	memory.WriteU8(Address(Address(R(state,31)) + 32), LowByte(R(state,11)));
	// cmplwi cr6,r10,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,10)),0);
	// bne cr6,0x8302b668
	if (!state.integer.cr6.eq) goto loc_8302B668;
	// bl 0x8302b5c8
	state.integer.lr = 0x8302B64C;
	Direct(0x8302b5c8u,memory,services,state);
	// clrlwi. r11,r3,24
	R(state,11) = Address(R(state,3)) & 0xFF;
	Compare<std::int32_t>(state,state.integer.cr0,Signed32(R(state,11)),0);
	// bne 0x8302b668
	if (!state.integer.cr0.eq) goto loc_8302B668;
	// rlwinm r11,r31,0,0,19
	R(state,11) = WordRotateMask(R(state,31),0,0xFFFFF000ull);
	// li r4,4801
	R(state,4) = 4801;
	// lwz r11,0(r11)
	R(state,11) = memory.ReadU32(Address(Address(R(state,11)) + 0));
	// lwz r3,148(r11)
	R(state,3) = memory.ReadU32(Address(Address(R(state,11)) + 148));
	// bl 0x82f99f48
	state.integer.lr = 0x8302B668;
	Direct(0x82f99f48u,memory,services,state);
loc_8302B668:
	// lwz r11,4(r31)
	R(state,11) = memory.ReadU32(Address(Address(R(state,31)) + 4));
	// lwz r10,0(r31)
	R(state,10) = memory.ReadU32(Address(Address(R(state,31)) + 0));
	// addi r11,r11,-1
	R(state,11) = R(state,11) + -1;
	// stw r11,4(r31)
	memory.WriteU32(Address(Address(R(state,31)) + 4), Address(R(state,11)));
	// lwz r3,20(r10)
	R(state,3) = memory.ReadU32(Address(Address(R(state,10)) + 20));
	// addi r1,r1,96
	R(state,1) = R(state,1) + 96;
	// lwz r12,-8(r1)
	R(state,12) = memory.ReadU32(Address(Address(R(state,1)) + -8));
	// mtlr r12
	state.integer.lr = R(state,12);
	// ld r31,-16(r1)
	R(state,31) = ReadU64(memory,Address(Address(R(state,1)) + -16));
	// blr
	return;
}
void Execute83053770(GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    std::uint64_t temporary=0;
	// mflr r12
	R(state,12) = state.integer.lr;
	// stw r12,-8(r1)
	memory.WriteU32(Address(Address(R(state,1)) + -8), Address(R(state,12)));
	// std r30,-24(r1)
	WriteU64(memory,Address(Address(R(state,1)) + -24), R(state,30));
	// std r31,-16(r1)
	WriteU64(memory,Address(Address(R(state,1)) + -16), R(state,31));
	// stwu r1,-112(r1)
	temporary = R(state,1) + std::uint64_t(-112);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// mr r30,r3
	R(state,30) = R(state,3);
	// mr r31,r4
	R(state,31) = R(state,4);
	// b 0x830537c0
	goto loc_830537C0;
loc_83053790:
	// lwz r11,4(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 4));
	// cmplwi r11,0
	Compare<std::uint32_t>(state,state.integer.cr0,Address(R(state,11)),0);
	// beq 0x830537b4
	if (state.integer.cr0.eq) goto loc_830537B4;
	// cmplw cr6,r11,r31
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,11)),Address(R(state,31)));
	// bge cr6,0x830537cc
	if (!state.integer.cr6.lt) goto loc_830537CC;
	// li r10,0
	R(state,10) = 0;
	// subf r31,r11,r31
	R(state,31) = R(state,31) - R(state,11);
	// stw r10,4(r30)
	memory.WriteU32(Address(Address(R(state,30)) + 4), Address(R(state,10)));
	// b 0x830537c0
	goto loc_830537C0;
loc_830537B4:
	// mr r3,r30
	R(state,3) = R(state,30);
	// bl 0x8302b620
	state.integer.lr = 0x830537BC;
	Direct(0x8302b620u,memory,services,state);
	// addi r31,r31,-1
	R(state,31) = R(state,31) + -1;
loc_830537C0:
	// cmplwi cr6,r31,0
	Compare<std::uint32_t>(state,state.integer.cr6,Address(R(state,31)),0);
	// bne cr6,0x83053790
	if (!state.integer.cr6.eq) goto loc_83053790;
	// b 0x830537d8
	goto loc_830537D8;
loc_830537CC:
	// lwz r11,4(r30)
	R(state,11) = memory.ReadU32(Address(Address(R(state,30)) + 4));
	// subf r11,r31,r11
	R(state,11) = R(state,11) - R(state,31);
	// stw r11,4(r30)
	memory.WriteU32(Address(Address(R(state,30)) + 4), Address(R(state,11)));
loc_830537D8:
	// addi r1,r1,112
	R(state,1) = R(state,1) + 112;
	// lwz r12,-8(r1)
	R(state,12) = memory.ReadU32(Address(Address(R(state,1)) + -8));
	// mtlr r12
	state.integer.lr = R(state,12);
	// ld r30,-24(r1)
	R(state,30) = ReadU64(memory,Address(Address(R(state,1)) + -24));
	// ld r31,-16(r1)
	R(state,31) = ReadU64(memory,Address(Address(R(state,1)) + -16));
	// blr
	return;
}
void Execute82F99F48(GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
    std::uint64_t temporary=0;
	// mflr r12
	R(state,12) = state.integer.lr;
	// stw r12,-8(r1)
	memory.WriteU32(Address(Address(R(state,1)) + -8), Address(R(state,12)));
	// std r5,32(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 32), R(state,5));
	// std r6,40(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 40), R(state,6));
	// std r7,48(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 48), R(state,7));
	// std r8,56(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 56), R(state,8));
	// std r9,64(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 64), R(state,9));
	// std r10,72(r1)
	WriteU64(memory,Address(Address(R(state,1)) + 72), R(state,10));
	// stwu r1,-96(r1)
	temporary = R(state,1) + std::uint64_t(-96);
	memory.WriteU32(Address(Address(temporary)), Address(R(state,1)));
	R(state,1) = temporary;
	// addi r11,r1,80
	R(state,11) = R(state,1) + 80;
	// addi r10,r1,128
	R(state,10) = R(state,1) + 128;
	// stw r10,0(r11)
	memory.WriteU32(Address(Address(R(state,11)) + 0), Address(R(state,10)));
	// lwz r5,80(r1)
	R(state,5) = memory.ReadU32(Address(Address(R(state,1)) + 80));
	// bl 0x82f99d98
	state.integer.lr = 0x82F99F80;
	Direct(0x82f99d98u,memory,services,state);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestBoundaryServices& services,Registers& state)
{
 switch(entry)
 {
 case 0x8302b2f8u:Execute8302B2F8(memory,services,state);return true;
 case 0x8302b5c8u:Execute8302B5C8(memory,services,state);return true;
 case 0x8302b620u:Execute8302B620(memory,services,state);return true;
 case 0x83053770u:Execute83053770(memory,services,state);return true;
 default:return false;
 }
}
}
