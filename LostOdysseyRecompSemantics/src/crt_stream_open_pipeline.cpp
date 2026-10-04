#include "lo_semantics/crt_stream_open_pipeline.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_open_pipeline
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
std::int32_t Signed(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Word(value)); }
std::uint8_t Byte(std::uint64_t value)
{ return static_cast<std::uint8_t>(value); }
std::int8_t SignedByte(std::uint64_t value)
{ return std::bit_cast<std::int8_t>(Byte(value)); }
template<class T>
void Compare(crt_async_status_transfer::Condition& condition,
    T left, T right, std::uint8_t so)
{
    condition = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), so};
}
void Save(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 20u; index <= 31u; ++index)
        WriteU64(memory, Address(state.r[1] - 16u - 8u * (31u - index)),
            state.r[index]);
    memory.WriteU32(Address(state.r[1] - 8u), Address(state.r[12]));
}
void Restore(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 20u; index <= 31u; ++index)
        state.r[index] = ReadU64(memory,
            Address(state.r[1] - 16u - 8u * (31u - index)));
    state.r[12] = memory.ReadU32(Address(state.r[1] - 8u));
    state.lr = state.r[12];
}
void Direct(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (!ApplyLower(entry, memory, dependencies, state))
        throw std::logic_error("unselected CRT open-pipeline callee");
}
void Open(GuestMemory& memory, Dependencies dependencies, Registers& state)
{

	std::uint64_t temp = 0;
	// mflr r12
	R(state, 12) = state.lr;
	// bl 0x82b7a6c8
	state.lr = 0x82DF6248;
	Save(memory, state);
	// stwu r1,-192(r1)
	temp = R(state, 1) + std::uint64_t(-192);
	memory.WriteU32(Word(temp), Word(R(state, 1)));
	R(state, 1) = temp;
	// li r23,0
	R(state, 23) = 0;
	// mr r28,r3
	R(state, 28) = R(state, 3);
	// addi r3,r1,84
	R(state, 3) = R(state, 1) + 84;
	// mr r30,r4
	R(state, 30) = R(state, 4);
	// mr r22,r5
	R(state, 22) = R(state, 5);
	// mr r21,r6
	R(state, 21) = R(state, 6);
	// stw r23,84(r1)
	memory.WriteU32(Word(R(state, 1)) + 84, Word(R(state, 23)));
	// mr r31,r7
	R(state, 31) = R(state, 7);
	// mr r29,r8
	R(state, 29) = R(state, 8);
	// mr r27,r23
	R(state, 27) = R(state, 23);
	// bl 0x82df6b68
	state.lr = 0x82DF6278;
	Direct(0x82df6b68u, memory, dependencies, state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed(R(state, 3)), 0, state.xer_so);
	// beq 0x82df6298
	if (state.cr0.eq) goto loc_82DF6298;
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
	// bl 0x82b7ff08
	state.lr = 0x82DF6298;
	Direct(0x82b7ff08u, memory, dependencies, state);
loc_82DF6298:
	// rlwinm. r11,r21,0,16,16
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x8000;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// bne 0x82df62a4
	if (!state.cr0.eq) goto loc_82DF62A4;
	// li r27,-128
	R(state, 27) = -128;
loc_82DF62A4:
	// clrlwi r11,r21,30
	R(state, 11) = Word(R(state, 21)) & 0x3;
	// lis r20,-16384
	R(state, 20) = -1073741824;
	// lis r10,-32768
	R(state, 10) = -2147483648;
	// cmplwi cr6,r11,1
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), 1, state.xer_so);
	// blt cr6,0x82df6314
	if (state.cr6.lt) goto loc_82DF6314;
	// beq cr6,0x82df630c
	if (state.cr6.eq) goto loc_82DF630C;
	// cmplwi cr6,r11,3
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), 3, state.xer_so);
	// blt cr6,0x82df6304
	if (state.cr6.lt) goto loc_82DF6304;
loc_82DF62C4:
	// bl 0x82b7fdb0
	state.lr = 0x82DF62C8;
	Direct(0x82b7fdb0u, memory, dependencies, state);
	// li r11,-1
	R(state, 11) = -1;
	// stw r23,0(r3)
	memory.WriteU32(Word(R(state, 3)) + 0, Word(R(state, 23)));
	// stw r11,0(r30)
	memory.WriteU32(Word(R(state, 30)) + 0, Word(R(state, 11)));
	// bl 0x82b7fd78
	state.lr = 0x82DF62D8;
	Direct(0x82b7fd78u, memory, dependencies, state);
	// mr r11,r3
	R(state, 11) = R(state, 3);
	// li r10,22
	R(state, 10) = 22;
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
	memory.WriteU32(Word(R(state, 11)) + 0, Word(R(state, 10)));
	// bl 0x82b7fec0
	state.lr = 0x82DF62FC;
	Direct(0x82b7fec0u, memory, dependencies, state);
	// li r3,22
	R(state, 3) = 22;
	// b 0x82df674c
	goto loc_82DF674C;
loc_82DF6304:
	// mr r25,r20
	R(state, 25) = R(state, 20);
	// b 0x82df6318
	goto loc_82DF6318;
loc_82DF630C:
	// lis r25,16384
	R(state, 25) = 1073741824;
	// b 0x82df6318
	goto loc_82DF6318;
loc_82DF6314:
	// mr r25,r10
	R(state, 25) = R(state, 10);
loc_82DF6318:
	// cmpwi cr6,r31,16
	Compare<int32_t>(state.cr6, Signed(R(state, 31)), 16, state.xer_so);
	// beq cr6,0x82df6368
	if (state.cr6.eq) goto loc_82DF6368;
	// cmpwi cr6,r31,32
	Compare<int32_t>(state.cr6, Signed(R(state, 31)), 32, state.xer_so);
	// beq cr6,0x82df6360
	if (state.cr6.eq) goto loc_82DF6360;
	// cmpwi cr6,r31,48
	Compare<int32_t>(state.cr6, Signed(R(state, 31)), 48, state.xer_so);
	// beq cr6,0x82df6358
	if (state.cr6.eq) goto loc_82DF6358;
	// cmpwi cr6,r31,64
	Compare<int32_t>(state.cr6, Signed(R(state, 31)), 64, state.xer_so);
	// beq cr6,0x82df6350
	if (state.cr6.eq) goto loc_82DF6350;
	// cmpwi cr6,r31,128
	Compare<int32_t>(state.cr6, Signed(R(state, 31)), 128, state.xer_so);
	// bne cr6,0x82df62c4
	if (!state.cr6.eq) goto loc_82DF62C4;
	// subf r11,r25,r10
	R(state, 11) = R(state, 10) - R(state, 25);
	// cntlzw r11,r11
	R(state, 11) = Word(R(state, 11)) == 0 ? 32 : std::countl_zero(Word(R(state, 11)));
	// rlwinm r24,r11,27,31,31
	R(state, 24) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 27) & 0x1;
	// b 0x82df636c
	goto loc_82DF636C;
loc_82DF6350:
	// li r24,3
	R(state, 24) = 3;
	// b 0x82df636c
	goto loc_82DF636C;
loc_82DF6358:
	// li r24,2
	R(state, 24) = 2;
	// b 0x82df636c
	goto loc_82DF636C;
loc_82DF6360:
	// li r24,1
	R(state, 24) = 1;
	// b 0x82df636c
	goto loc_82DF636C;
loc_82DF6368:
	// mr r24,r23
	R(state, 24) = R(state, 23);
loc_82DF636C:
	// rlwinm r11,r21,0,21,23
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x700;
	// cmpwi cr6,r11,1024
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 1024, state.xer_so);
	// bgt cr6,0x82df63b4
	if (state.cr6.gt) goto loc_82DF63B4;
	// beq cr6,0x82df63ac
	if (state.cr6.eq) goto loc_82DF63AC;
	// cmpwi cr6,r11,0
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 0, state.xer_so);
	// beq cr6,0x82df63ac
	if (state.cr6.eq) goto loc_82DF63AC;
	// cmpwi cr6,r11,256
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 256, state.xer_so);
	// beq cr6,0x82df63a4
	if (state.cr6.eq) goto loc_82DF63A4;
	// cmpwi cr6,r11,512
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 512, state.xer_so);
	// beq cr6,0x82df6424
	if (state.cr6.eq) goto loc_82DF6424;
	// cmpwi cr6,r11,768
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 768, state.xer_so);
	// bne cr6,0x82df62c4
	if (!state.cr6.eq) goto loc_82DF62C4;
	// li r31,2
	R(state, 31) = 2;
	// b 0x82df63d0
	goto loc_82DF63D0;
loc_82DF63A4:
	// li r31,4
	R(state, 31) = 4;
	// b 0x82df63d0
	goto loc_82DF63D0;
loc_82DF63AC:
	// li r31,3
	R(state, 31) = 3;
	// b 0x82df63d0
	goto loc_82DF63D0;
loc_82DF63B4:
	// cmpwi cr6,r11,1280
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 1280, state.xer_so);
	// beq cr6,0x82df63cc
	if (state.cr6.eq) goto loc_82DF63CC;
	// cmpwi cr6,r11,1536
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 1536, state.xer_so);
	// beq cr6,0x82df6424
	if (state.cr6.eq) goto loc_82DF6424;
	// cmpwi cr6,r11,1792
	Compare<int32_t>(state.cr6, Signed(R(state, 11)), 1792, state.xer_so);
	// bne cr6,0x82df62c4
	if (!state.cr6.eq) goto loc_82DF62C4;
loc_82DF63CC:
	// li r31,1
	R(state, 31) = 1;
loc_82DF63D0:
	// rlwinm. r11,r21,0,23,23
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x100;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// li r26,128
	R(state, 26) = 128;
	// beq 0x82df63f4
	if (state.cr0.eq) goto loc_82DF63F4;
	// lis r11,-31955
	R(state, 11) = -2094202880;
	// lwz r11,15036(r11)
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + 15036);
	// andc r11,r29,r11
	R(state, 11) = R(state, 29) & ~R(state, 11);
	// rlwinm. r11,r11,0,24,24
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 0) & 0x80;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// bne 0x82df63f4
	if (!state.cr0.eq) goto loc_82DF63F4;
	// li r26,1
	R(state, 26) = 1;
loc_82DF63F4:
	// rlwinm. r11,r21,0,25,25
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x40;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6408
	if (state.cr0.eq) goto loc_82DF6408;
	// oris r26,r26,1024
	R(state, 26) = R(state, 26) | 67108864;
	// oris r25,r25,1
	R(state, 25) = R(state, 25) | 65536;
	// ori r24,r24,4
	R(state, 24) = R(state, 24) | 4;
loc_82DF6408:
	// rlwinm. r11,r21,0,19,19
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x1000;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6414
	if (state.cr0.eq) goto loc_82DF6414;
	// ori r26,r26,256
	R(state, 26) = R(state, 26) | 256;
loc_82DF6414:
	// rlwinm. r11,r21,0,26,26
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x20;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df642c
	if (state.cr0.eq) goto loc_82DF642C;
	// oris r26,r26,2048
	R(state, 26) = R(state, 26) | 134217728;
	// b 0x82df6438
	goto loc_82DF6438;
loc_82DF6424:
	// li r31,5
	R(state, 31) = 5;
	// b 0x82df63d0
	goto loc_82DF63D0;
loc_82DF642C:
	// rlwinm. r11,r21,0,27,27
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x10;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6438
	if (state.cr0.eq) goto loc_82DF6438;
	// oris r26,r26,4096
	R(state, 26) = R(state, 26) | 268435456;
loc_82DF6438:
	// bl 0x82b86420
	state.lr = 0x82DF643C;
	Direct(0x82b86420u, memory, dependencies, state);
	// cmpwi cr6,r3,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 3)), -1, state.xer_so);
	// stw r3,0(r30)
	memory.WriteU32(Word(R(state, 30)) + 0, Word(R(state, 3)));
	// bne cr6,0x82df6470
	if (!state.cr6.eq) goto loc_82DF6470;
	// bl 0x82b7fdb0
	state.lr = 0x82DF644C;
	Direct(0x82b7fdb0u, memory, dependencies, state);
	// li r11,-1
	R(state, 11) = -1;
	// stw r23,0(r3)
	memory.WriteU32(Word(R(state, 3)) + 0, Word(R(state, 23)));
	// stw r11,0(r30)
	memory.WriteU32(Word(R(state, 30)) + 0, Word(R(state, 11)));
	// bl 0x82b7fd78
	state.lr = 0x82DF645C;
	Direct(0x82b7fd78u, memory, dependencies, state);
	// li r11,24
	R(state, 11) = 24;
	// stw r11,0(r3)
	memory.WriteU32(Word(R(state, 3)) + 0, Word(R(state, 11)));
	// bl 0x82b7fd78
	state.lr = 0x82DF6468;
	Direct(0x82b7fd78u, memory, dependencies, state);
	// lwz r3,0(r3)
	R(state, 3) = memory.ReadU32(Word(R(state, 3)) + 0);
	// b 0x82df674c
	goto loc_82DF674C;
loc_82DF6470:
	// li r11,1
	R(state, 11) = 1;
	// li r9,0
	R(state, 9) = 0;
	// mr r8,r26
	R(state, 8) = R(state, 26);
	// mr r7,r31
	R(state, 7) = R(state, 31);
	// li r6,0
	R(state, 6) = 0;
	// mr r5,r24
	R(state, 5) = R(state, 24);
	// stw r11,0(r28)
	memory.WriteU32(Word(R(state, 28)) + 0, Word(R(state, 11)));
	// mr r4,r25
	R(state, 4) = R(state, 25);
	// mr r3,r22
	R(state, 3) = R(state, 22);
	// bl 0x82be2be0
	state.lr = 0x82DF6498;
	Direct(0x82be2be0u, memory, dependencies, state);
	// mr r28,r3
	R(state, 28) = R(state, 3);
	// cmpwi cr6,r28,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 28)), -1, state.xer_so);
	// bne cr6,0x82df652c
	if (!state.cr6.eq) goto loc_82DF652C;
	// rlwinm r11,r25,0,0,1
	R(state, 11) = std::rotl(Word(R(state, 25)) | (R(state, 25) << 32), 0) & 0xC0000000;
	// cmplw cr6,r11,r20
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), Word(R(state, 20)), state.xer_so);
	// bne cr6,0x82df64e8
	if (!state.cr6.eq) goto loc_82DF64E8;
	// clrlwi. r11,r21,31
	R(state, 11) = Word(R(state, 21)) & 0x1;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df64e8
	if (state.cr0.eq) goto loc_82DF64E8;
	// clrlwi r25,r25,1
	R(state, 25) = Word(R(state, 25)) & 0x7FFFFFFF;
	// li r9,0
	R(state, 9) = 0;
	// mr r8,r26
	R(state, 8) = R(state, 26);
	// mr r7,r31
	R(state, 7) = R(state, 31);
	// li r6,0
	R(state, 6) = 0;
	// mr r5,r24
	R(state, 5) = R(state, 24);
	// mr r3,r22
	R(state, 3) = R(state, 22);
	// mr r4,r25
	R(state, 4) = R(state, 25);
	// bl 0x82be2be0
	state.lr = 0x82DF64DC;
	Direct(0x82be2be0u, memory, dependencies, state);
	// mr r28,r3
	R(state, 28) = R(state, 3);
	// cmpwi cr6,r28,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 28)), -1, state.xer_so);
	// bne cr6,0x82df652c
	if (!state.cr6.eq) goto loc_82DF652C;
loc_82DF64E8:
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// lis r10,-31944
	R(state, 10) = -2093481984;
	// rlwinm r9,r11,6,21,25
	R(state, 9) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// addi r10,r10,-29312
	R(state, 10) = R(state, 10) + -29312;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r10
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 10)));
	// add r11,r11,r9
	R(state, 11) = R(state, 11) + R(state, 9);
	// lbz r10,4(r11)
	R(state, 10) = memory.ReadU8(Word(R(state, 11)) + 4);
	// extsb r10,r10
	R(state, 10) = SignedByte(R(state, 10));
	// rlwinm r10,r10,0,0,30
	R(state, 10) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 0) & 0xFFFFFFFE;
	// stb r10,4(r11)
	memory.WriteU8(Word(R(state, 11)) + 4, Byte(R(state, 10)));
	// bl 0x822ca100
	state.lr = 0x82DF651C;
	Direct(0x822ca100u, memory, dependencies, state);
	// bl 0x82b7fde8
	state.lr = 0x82DF6520;
	Direct(0x82b7fde8u, memory, dependencies, state);
loc_82DF6520:
	// bl 0x82b7fd78
	state.lr = 0x82DF6524;
	Direct(0x82b7fd78u, memory, dependencies, state);
	// lwz r23,0(r3)
	R(state, 23) = memory.ReadU32(Word(R(state, 3)) + 0);
	// b 0x82df6748
	goto loc_82DF6748;
loc_82DF652C:
	// mr r4,r28
	R(state, 4) = R(state, 28);
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// bl 0x82b86108
	state.lr = 0x82DF6538;
	Direct(0x82b86108u, memory, dependencies, state);
	// lwz r10,0(r30)
	R(state, 10) = memory.ReadU32(Word(R(state, 30)) + 0);
	// lis r11,-31944
	R(state, 11) = -2093481984;
	// rlwinm r9,r10,6,21,25
	R(state, 9) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 6) & 0x7C0;
	// srawi r10,r10,5
	state.xer_ca = (Signed(R(state, 10)) < 0) && ((Word(R(state, 10)) & 0x1F) != 0);
	R(state, 10) = Signed(R(state, 10)) >> 5;
	// addi r31,r11,-29312
	R(state, 31) = R(state, 11) + -29312;
	// rlwinm r10,r10,2,0,29
	R(state, 10) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 2) & 0xFFFFFFFC;
	// ori r11,r27,1
	R(state, 11) = R(state, 27) | 1;
	// extsb r11,r11
	R(state, 11) = SignedByte(R(state, 11));
	// lwzx r10,r10,r31
	R(state, 10) = memory.ReadU32(Word(R(state, 10)) + Word(R(state, 31)));
	// add r10,r10,r9
	R(state, 10) = R(state, 10) + R(state, 9);
	// stb r11,4(r10)
	memory.WriteU8(Word(R(state, 10)) + 4, Byte(R(state, 11)));
	// lwz r10,0(r30)
	R(state, 10) = memory.ReadU32(Word(R(state, 30)) + 0);
	// rlwinm r9,r10,6,21,25
	R(state, 9) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 6) & 0x7C0;
	// srawi r10,r10,5
	state.xer_ca = (Signed(R(state, 10)) < 0) && ((Word(R(state, 10)) & 0x1F) != 0);
	R(state, 10) = Signed(R(state, 10)) >> 5;
	// rlwinm r10,r10,2,0,29
	R(state, 10) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 2) & 0xFFFFFFFC;
	// andi. r29,r11,72
	R(state, 29) = R(state, 11) & 72;
	Compare<int32_t>(state.cr0, Signed(R(state, 29)), 0, state.xer_so);
	// cmpwi r29,0
	Compare<int32_t>(state.cr0, Signed(R(state, 29)), 0, state.xer_so);
	// lwzx r10,r10,r31
	R(state, 10) = memory.ReadU32(Word(R(state, 10)) + Word(R(state, 31)));
	// add r10,r10,r9
	R(state, 10) = R(state, 10) + R(state, 9);
	// lbz r9,40(r10)
	R(state, 9) = memory.ReadU8(Word(R(state, 10)) + 40);
	// clrlwi r9,r9,31
	R(state, 9) = Word(R(state, 9)) & 0x1;
	// stb r9,40(r10)
	memory.WriteU8(Word(R(state, 10)) + 40, Byte(R(state, 9)));
	// bne 0x82df6630
	if (!state.cr0.eq) goto loc_82DF6630;
	// rlwinm. r11,r11,0,24,24
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 0) & 0x80;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6630
	if (state.cr0.eq) goto loc_82DF6630;
	// rlwinm. r11,r21,0,30,30
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x2;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6630
	if (state.cr0.eq) goto loc_82DF6630;
	// li r5,2
	R(state, 5) = 2;
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// li r4,-1
	R(state, 4) = -1;
	// bl 0x82df43f0
	state.lr = 0x82DF65B4;
	Direct(0x82df43f0u, memory, dependencies, state);
	// mr r27,r3
	R(state, 27) = R(state, 3);
	// cmpwi cr6,r27,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 27)), -1, state.xer_so);
	// bne cr6,0x82df65dc
	if (!state.cr6.eq) goto loc_82DF65DC;
	// bl 0x82b7fdb0
	state.lr = 0x82DF65C4;
	Direct(0x82b7fdb0u, memory, dependencies, state);
	// lwz r11,0(r3)
	R(state, 11) = memory.ReadU32(Word(R(state, 3)) + 0);
	// cmplwi cr6,r11,131
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), 131, state.xer_so);
	// beq cr6,0x82df6630
	if (state.cr6.eq) goto loc_82DF6630;
loc_82DF65D0:
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// bl 0x82b87a38
	state.lr = 0x82DF65D8;
	Direct(0x82b87a38u, memory, dependencies, state);
	// b 0x82df6520
	goto loc_82DF6520;
loc_82DF65DC:
	// li r5,1
	R(state, 5) = 1;
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// addi r4,r1,80
	R(state, 4) = R(state, 1) + 80;
	// stb r23,80(r1)
	memory.WriteU8(Word(R(state, 1)) + 80, Byte(R(state, 23)));
	// bl 0x82b85448
	state.lr = 0x82DF65F0;
	Direct(0x82b85448u, memory, dependencies, state);
	// cmpwi r3,0
	Compare<int32_t>(state.cr0, Signed(R(state, 3)), 0, state.xer_so);
	// bne 0x82df6618
	if (!state.cr0.eq) goto loc_82DF6618;
	// lbz r11,80(r1)
	R(state, 11) = memory.ReadU8(Word(R(state, 1)) + 80);
	// cmplwi cr6,r11,26
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), 26, state.xer_so);
	// bne cr6,0x82df6618
	if (!state.cr6.eq) goto loc_82DF6618;
	// extsw r4,r27
	R(state, 4) = Signed(R(state, 27));
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// bl 0x82df6948
	state.lr = 0x82DF6610;
	Direct(0x82df6948u, memory, dependencies, state);
	// cmpwi cr6,r3,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 3)), -1, state.xer_so);
	// beq cr6,0x82df65d0
	if (state.cr6.eq) goto loc_82DF65D0;
loc_82DF6618:
	// li r5,0
	R(state, 5) = 0;
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// li r4,0
	R(state, 4) = 0;
	// bl 0x82df43f0
	state.lr = 0x82DF6628;
	Direct(0x82df43f0u, memory, dependencies, state);
	// cmpwi cr6,r3,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 3)), -1, state.xer_so);
	// beq cr6,0x82df65d0
	if (state.cr6.eq) goto loc_82DF65D0;
loc_82DF6630:
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// cmpwi cr6,r29,0
	Compare<int32_t>(state.cr6, Signed(R(state, 29)), 0, state.xer_so);
	// rlwinm r10,r11,6,21,25
	R(state, 10) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r31
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 31)));
	// add r11,r11,r10
	R(state, 11) = R(state, 11) + R(state, 10);
	// lbz r10,40(r11)
	R(state, 10) = memory.ReadU8(Word(R(state, 11)) + 40);
	// clrlwi r10,r10,31
	R(state, 10) = Word(R(state, 10)) & 0x1;
	// stb r10,40(r11)
	memory.WriteU8(Word(R(state, 11)) + 40, Byte(R(state, 10)));
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// rlwinm r10,r11,6,21,25
	R(state, 10) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r31
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 31)));
	// add r11,r11,r10
	R(state, 11) = R(state, 11) + R(state, 10);
	// lbz r10,40(r11)
	R(state, 10) = memory.ReadU8(Word(R(state, 11)) + 40);
	// extsb r10,r10
	R(state, 10) = SignedByte(R(state, 10));
	// rlwinm r10,r10,0,0,30
	R(state, 10) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 0) & 0xFFFFFFFE;
	// stb r10,40(r11)
	memory.WriteU8(Word(R(state, 11)) + 40, Byte(R(state, 10)));
	// bne cr6,0x82df66b0
	if (!state.cr6.eq) goto loc_82DF66B0;
	// rlwinm. r11,r21,0,28,28
	R(state, 11) = std::rotl(Word(R(state, 21)) | (R(state, 21) << 32), 0) & 0x8;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df66b0
	if (state.cr0.eq) goto loc_82DF66B0;
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// rlwinm r10,r11,6,21,25
	R(state, 10) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r31
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 31)));
	// add r11,r11,r10
	R(state, 11) = R(state, 11) + R(state, 10);
	// lbz r10,4(r11)
	R(state, 10) = memory.ReadU8(Word(R(state, 11)) + 4);
	// ori r10,r10,32
	R(state, 10) = R(state, 10) | 32;
	// stb r10,4(r11)
	memory.WriteU8(Word(R(state, 11)) + 4, Byte(R(state, 10)));
loc_82DF66B0:
	// rlwinm r11,r25,0,0,1
	R(state, 11) = std::rotl(Word(R(state, 25)) | (R(state, 25) << 32), 0) & 0xC0000000;
	// cmplw cr6,r11,r20
	Compare<uint32_t>(state.cr6, Word(R(state, 11)), Word(R(state, 20)), state.xer_so);
	// bne cr6,0x82df6748
	if (!state.cr6.eq) goto loc_82DF6748;
	// clrlwi. r11,r21,31
	R(state, 11) = Word(R(state, 21)) & 0x1;
	Compare<int32_t>(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
	// beq 0x82df6748
	if (state.cr0.eq) goto loc_82DF6748;
	// mr r3,r28
	R(state, 3) = R(state, 28);
	// bl 0x82be1b80
	state.lr = 0x82DF66CC;
	Direct(0x82be1b80u, memory, dependencies, state);
	// li r9,0
	R(state, 9) = 0;
	// mr r8,r26
	R(state, 8) = R(state, 26);
	// li r7,3
	R(state, 7) = 3;
	// li r6,0
	R(state, 6) = 0;
	// mr r5,r24
	R(state, 5) = R(state, 24);
	// clrlwi r4,r25,1
	R(state, 4) = Word(R(state, 25)) & 0x7FFFFFFF;
	// mr r3,r22
	R(state, 3) = R(state, 22);
	// bl 0x82be2be0
	state.lr = 0x82DF66EC;
	Direct(0x82be2be0u, memory, dependencies, state);
	// cmpwi cr6,r3,-1
	Compare<int32_t>(state.cr6, Signed(R(state, 3)), -1, state.xer_so);
	// bne cr6,0x82df6730
	if (!state.cr6.eq) goto loc_82DF6730;
	// bl 0x822ca100
	state.lr = 0x82DF66F8;
	Direct(0x822ca100u, memory, dependencies, state);
	// bl 0x82b7fde8
	state.lr = 0x82DF66FC;
	Direct(0x82b7fde8u, memory, dependencies, state);
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// rlwinm r10,r11,6,21,25
	R(state, 10) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r31
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 31)));
	// add r11,r11,r10
	R(state, 11) = R(state, 11) + R(state, 10);
	// lbz r10,4(r11)
	R(state, 10) = memory.ReadU8(Word(R(state, 11)) + 4);
	// extsb r10,r10
	R(state, 10) = SignedByte(R(state, 10));
	// rlwinm r10,r10,0,0,30
	R(state, 10) = std::rotl(Word(R(state, 10)) | (R(state, 10) << 32), 0) & 0xFFFFFFFE;
	// stb r10,4(r11)
	memory.WriteU8(Word(R(state, 11)) + 4, Byte(R(state, 10)));
	// lwz r3,0(r30)
	R(state, 3) = memory.ReadU32(Word(R(state, 30)) + 0);
	// bl 0x82b86190
	state.lr = 0x82DF672C;
	Direct(0x82b86190u, memory, dependencies, state);
	// b 0x82df6520
	goto loc_82DF6520;
loc_82DF6730:
	// lwz r11,0(r30)
	R(state, 11) = memory.ReadU32(Word(R(state, 30)) + 0);
	// rlwinm r10,r11,6,21,25
	R(state, 10) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 6) & 0x7C0;
	// srawi r11,r11,5
	state.xer_ca = (Signed(R(state, 11)) < 0) && ((Word(R(state, 11)) & 0x1F) != 0);
	R(state, 11) = Signed(R(state, 11)) >> 5;
	// rlwinm r11,r11,2,0,29
	R(state, 11) = std::rotl(Word(R(state, 11)) | (R(state, 11) << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r31
	R(state, 11) = memory.ReadU32(Word(R(state, 11)) + Word(R(state, 31)));
	// stwx r3,r11,r10
	memory.WriteU32(Word(R(state, 11)) + Word(R(state, 10)), Word(R(state, 3)));
loc_82DF6748:
	// mr r3,r23
	R(state, 3) = R(state, 23);
loc_82DF674C:
	// addi r1,r1,192
	R(state, 1) = R(state, 1) + 192;
	// b 0x82b7a718
	Restore(memory, state);
	return;

}
} // namespace

bool ApplyLower(GuestAddress entry, GuestMemory& memory,
    Dependencies deps, Registers& state)
{
    switch (entry)
    {
    case 0x82df6b68u: case 0x82df43f0u: case 0x82df6948u:
        return crt_stream_position_routes::Apply(entry, memory,
            deps.position, state);
    case 0x82b85448u:
        return crt_stream_read_routes::Apply(entry, memory, deps.read, state);
    default: break;
    }
    auto lower = crt_context_adapter::ToStream(state);
    bool applied;
    switch (entry)
    {
    case 0x82b86108u: case 0x82b86420u: case 0x82be2be0u:
        applied = crt_stream_open_routes_context::Apply(entry, memory,
            deps.stream, deps.open, lower);
        break;
    case 0x82b87a38u: case 0x82be1b80u: case 0x82b86190u:
        applied = crt_stream_close_error::Apply(entry, memory,
            {deps.stream, deps.close}, lower);
        break;
    case 0x82b7ff08u:
        applied = crt_float_environment::Apply(entry, memory,
            deps.failure, lower);
        break;
    default:
        applied = crt_stream_operations::ApplyAcceptedCallee(entry, memory,
            deps.stream, lower);
        break;
    }
    if (applied) crt_context_adapter::FromStream(state, lower);
    return applied;
}

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82df6240u) return false;
    Open(memory, dependencies, state);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_open_pipeline
