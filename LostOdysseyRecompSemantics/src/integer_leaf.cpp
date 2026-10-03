#include "lo_semantics/integer_leaf.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace lo::semantic::integer_leaf
{
namespace
{

enum class Family : std::uint8_t
{
    Constant,
    ConstantWithR11,
    OffsetR3,
    CopyR4,
    CopyR7,
    CopyR10,
    ScaledIndex88,
    EqualR3R5Low32,
    EqualR4R3Low32,
    EqualR4R3Low32WithFlag,
    ZeroR3Low32,
    EqualR3ConstantLow32,
    NotEqualR3ConstantLow32,
    NonzeroR4Low32,
    MaskR6Bit13,
    LowByteR4,
    ScaledIndex36,
    ScaledIndexStaticBase,
    ScaledMaskedIndex,
};

struct Entry
{
    std::uint32_t address;
    Family family;
    std::uint64_t value;
    std::uint64_t auxiliary;
};

// The address table is generated from the reviewed exact-body map. Constants
// are folded here; the implementation below describes operations, not PPC steps.
constexpr std::array<Entry, 296> kEntries{{
    {0x822A03C8u, Family::ConstantWithR11, 0xffffffff83214af0ull, 0xffffffff83210000ull},
    {0x822CC4B0u, Family::ConstantWithR11, 0xffffffff83243618ull, 0xffffffff83240000ull},
    {0x822D1128u, Family::ConstantWithR11, 0xffffffff831ebe70ull, 0xffffffff831f0000ull},
    {0x82341C40u, Family::ConstantWithR11, 0xffffffff83238a28ull, 0xffffffff83240000ull},
    {0x823688E8u, Family::ConstantWithR11, 0xffffffff83243620ull, 0xffffffff83240000ull},
    {0x823731B0u, Family::Constant, 0x00000000000003eeull, 0x0000000000000000ull},
    {0x8237BE50u, Family::ConstantWithR11, 0xffffffff83243610ull, 0xffffffff83240000ull},
    {0x82389B78u, Family::ConstantWithR11, 0xffffffff832ca0e8ull, 0xffffffff832ca0e0ull},
    {0x823C93A8u, Family::ConstantWithR11, 0xffffffff832ecb6cull, 0xffffffff832f0000ull},
    {0x823CB2B0u, Family::OffsetR3, 0x0000000000000060ull, 0x0000000000000000ull},
    {0x823CB8A8u, Family::ConstantWithR11, 0xffffffff8336c930ull, 0xffffffff83370000ull},
    {0x823D7788u, Family::ConstantWithR11, 0xffffffff83247220ull, 0xffffffff83240000ull},
    {0x823DAF70u, Family::ScaledIndex88, 0x00000000000001c0ull, 0x0000000000000000ull},
    {0x823E0208u, Family::ConstantWithR11, 0xffffffff8336ca40ull, 0xffffffff83370000ull},
    {0x823E1FE0u, Family::ConstantWithR11, 0xffffffff8336ca10ull, 0xffffffff83370000ull},
    {0x823F3810u, Family::ConstantWithR11, 0xffffffff8336d128ull, 0xffffffff83370000ull},
    {0x823F3970u, Family::ConstantWithR11, 0xffffffff8336cba8ull, 0xffffffff83370000ull},
    {0x824120F0u, Family::ConstantWithR11, 0xffffffff821914acull, 0xffffffff82190000ull},
    {0x82419480u, Family::OffsetR3, 0xffffffffffffff10ull, 0x0000000000000000ull},
    {0x824800A0u, Family::ConstantWithR11, 0xffffffff83316000ull, 0xffffffff83310000ull},
    {0x82485AE0u, Family::ConstantWithR11, 0xffffffff821a8e94ull, 0xffffffff821b0000ull},
    {0x82485C08u, Family::ConstantWithR11, 0xffffffff821a8eb0ull, 0xffffffff821b0000ull},
    {0x82486BF8u, Family::ConstantWithR11, 0xffffffff821a94ccull, 0xffffffff821b0000ull},
    {0x82498F30u, Family::Constant, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x8249B780u, Family::ConstantWithR11, 0xffffffff821aa97cull, 0xffffffff821b0000ull},
    {0x8249B790u, Family::ConstantWithR11, 0xffffffff821aa960ull, 0xffffffff821b0000ull},
    {0x824B2020u, Family::ConstantWithR11, 0xffffffff8336ab3cull, 0xffffffff83370000ull},
    {0x824B2138u, Family::ConstantWithR11, 0xffffffff8336ab44ull, 0xffffffff83370000ull},
    {0x824B21F0u, Family::ConstantWithR11, 0xffffffff8336ab4cull, 0xffffffff83370000ull},
    {0x824B2258u, Family::ConstantWithR11, 0xffffffff8336ab54ull, 0xffffffff83370000ull},
    {0x824B22D0u, Family::ConstantWithR11, 0xffffffff8336ab5cull, 0xffffffff83370000ull},
    {0x824B23F8u, Family::ConstantWithR11, 0xffffffff8336ab64ull, 0xffffffff83370000ull},
    {0x824B24F8u, Family::ConstantWithR11, 0xffffffff8336ab6cull, 0xffffffff83370000ull},
    {0x824B2678u, Family::ConstantWithR11, 0xffffffff8336ab74ull, 0xffffffff83370000ull},
    {0x824B2740u, Family::ConstantWithR11, 0xffffffff8336ab7cull, 0xffffffff83370000ull},
    {0x824B2840u, Family::ConstantWithR11, 0xffffffff8336ab84ull, 0xffffffff83370000ull},
    {0x824B2900u, Family::ConstantWithR11, 0xffffffff8336ab8cull, 0xffffffff83370000ull},
    {0x824B2A08u, Family::ConstantWithR11, 0xffffffff8336ab94ull, 0xffffffff83370000ull},
    {0x824B2A78u, Family::ConstantWithR11, 0xffffffff8336ab9cull, 0xffffffff83370000ull},
    {0x824B2BB0u, Family::ConstantWithR11, 0xffffffff8336aba4ull, 0xffffffff83370000ull},
    {0x824B2DB0u, Family::ConstantWithR11, 0xffffffff8336abacull, 0xffffffff83370000ull},
    {0x824B2F08u, Family::ConstantWithR11, 0xffffffff8336abb4ull, 0xffffffff83370000ull},
    {0x824B3178u, Family::ConstantWithR11, 0xffffffff8336abbcull, 0xffffffff83370000ull},
    {0x824B32F8u, Family::ConstantWithR11, 0xffffffff8336abc4ull, 0xffffffff83370000ull},
    {0x824B3488u, Family::ConstantWithR11, 0xffffffff8336abccull, 0xffffffff83370000ull},
    {0x824B3578u, Family::ConstantWithR11, 0xffffffff8336abd4ull, 0xffffffff83370000ull},
    {0x824B5124u, Family::ConstantWithR11, 0xffffffff821ac0a8ull, 0xffffffff821b0000ull},
    {0x824B5130u, Family::ConstantWithR11, 0xffffffff821ac0b8ull, 0xffffffff821b0000ull},
    {0x824B513Cu, Family::ConstantWithR11, 0xffffffff821ac0c8ull, 0xffffffff821b0000ull},
    {0x824B5148u, Family::ConstantWithR11, 0xffffffff821ac0d8ull, 0xffffffff821b0000ull},
    {0x824B5154u, Family::ConstantWithR11, 0xffffffff821ac0e4ull, 0xffffffff821b0000ull},
    {0x824B5160u, Family::ConstantWithR11, 0xffffffff821ac0f8ull, 0xffffffff821b0000ull},
    {0x824B516Cu, Family::ConstantWithR11, 0xffffffff821ac110ull, 0xffffffff821b0000ull},
    {0x824C1520u, Family::ConstantWithR11, 0xffffffff821ad480ull, 0xffffffff821b0000ull},
    {0x824C1610u, Family::ConstantWithR11, 0xffffffff821ad498ull, 0xffffffff821b0000ull},
    {0x824C1718u, Family::ConstantWithR11, 0xffffffff821ad4b4ull, 0xffffffff821b0000ull},
    {0x824D4E70u, Family::EqualR3R5Low32, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x824D9250u, Family::ConstantWithR11, 0xffffffff8218afb8ull, 0xffffffff82190000ull},
    {0x824E1AC0u, Family::ZeroR3Low32, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x824FD308u, Family::OffsetR3, 0x0000000000000098ull, 0x0000000000000000ull},
    {0x824FD310u, Family::OffsetR3, 0x000000000000009cull, 0x0000000000000000ull},
    {0x824FD318u, Family::OffsetR3, 0x00000000000000a0ull, 0x0000000000000000ull},
    {0x824FD320u, Family::OffsetR3, 0x00000000000000a4ull, 0x0000000000000000ull},
    {0x82521C00u, Family::EqualR4R3Low32, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x8255C540u, Family::Constant, 0x00000000000003efull, 0x0000000000000000ull},
    {0x82582938u, Family::OffsetR3, 0xffffffffffffffc0ull, 0x0000000000000000ull},
    {0x825929F0u, Family::ConstantWithR11, 0xffffffff8336c310ull, 0xffffffff83370000ull},
    {0x825945C0u, Family::ConstantWithR11, 0xffffffff821d3100ull, 0xffffffff821d0000ull},
    {0x82594E98u, Family::ConstantWithR11, 0xffffffff821d311cull, 0xffffffff821d0000ull},
    {0x825A08E8u, Family::ConstantWithR11, 0xffffffff821d44e0ull, 0xffffffff821d0000ull},
    {0x825A0A10u, Family::ConstantWithR11, 0xffffffff821d4524ull, 0xffffffff821d0000ull},
    {0x825A0B80u, Family::ConstantWithR11, 0xffffffff821d45a8ull, 0xffffffff821d0000ull},
    {0x825A0EC0u, Family::ConstantWithR11, 0xffffffff821d4640ull, 0xffffffff821d0000ull},
    {0x825A9968u, Family::ConstantWithR11, 0xffffffff821da294ull, 0xffffffff821e0000ull},
    {0x825A9A78u, Family::ConstantWithR11, 0xffffffff821da2c4ull, 0xffffffff821e0000ull},
    {0x825A9B48u, Family::ConstantWithR11, 0xffffffff821da2f8ull, 0xffffffff821e0000ull},
    {0x825AFD30u, Family::ConstantWithR11, 0xffffffff821da5fcull, 0xffffffff821e0000ull},
    {0x825AFD40u, Family::ConstantWithR11, 0xffffffff821da628ull, 0xffffffff821e0000ull},
    {0x825AFD98u, Family::ConstantWithR11, 0xffffffff821da658ull, 0xffffffff821e0000ull},
    {0x825B0100u, Family::ConstantWithR11, 0xffffffff821da688ull, 0xffffffff821e0000ull},
    {0x825B0380u, Family::ConstantWithR11, 0xffffffff821da6acull, 0xffffffff821e0000ull},
    {0x825B04F8u, Family::ConstantWithR11, 0xffffffff821da6d4ull, 0xffffffff821e0000ull},
    {0x825B07E0u, Family::ConstantWithR11, 0xffffffff821da6f4ull, 0xffffffff821e0000ull},
    {0x825B09C0u, Family::ConstantWithR11, 0xffffffff821da728ull, 0xffffffff821e0000ull},
    {0x825B0B00u, Family::ConstantWithR11, 0xffffffff821da758ull, 0xffffffff821e0000ull},
    {0x825B0C50u, Family::ConstantWithR11, 0xffffffff821da784ull, 0xffffffff821e0000ull},
    {0x825B2AF0u, Family::ConstantWithR11, 0xffffffff821da914ull, 0xffffffff821e0000ull},
    {0x825B2BF0u, Family::ConstantWithR11, 0xffffffff821da938ull, 0xffffffff821e0000ull},
    {0x825D2FA0u, Family::Constant, 0x0000000000989680ull, 0x0000000000000000ull},
    {0x825D32B8u, Family::CopyR7, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x825D9C18u, Family::ConstantWithR11, 0xffffffff821dce70ull, 0xffffffff821e0000ull},
    {0x825D9C58u, Family::ConstantWithR11, 0xffffffff821dce94ull, 0xffffffff821e0000ull},
    {0x825DC428u, Family::ConstantWithR11, 0xffffffff821dd060ull, 0xffffffff821e0000ull},
    {0x825DC638u, Family::ConstantWithR11, 0xffffffff821dd088ull, 0xffffffff821e0000ull},
    {0x825EDB48u, Family::CopyR4, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x825F3798u, Family::Constant, 0x0000000000000002ull, 0x0000000000000000ull},
    {0x826293C8u, Family::Constant, 0x0000000000000014ull, 0x0000000000000000ull},
    {0x8262C5F8u, Family::ConstantWithR11, 0xffffffff83243640ull, 0xffffffff83240000ull},
    {0x82631B10u, Family::OffsetR3, 0xfffffffffffffd70ull, 0x0000000000000000ull},
    {0x8264E488u, Family::OffsetR3, 0xffffffffffffffc4ull, 0x0000000000000000ull},
    {0x82656DE0u, Family::OffsetR3, 0xffffffffffffffa8ull, 0x0000000000000000ull},
    {0x82656DE8u, Family::OffsetR3, 0xffffffffffffffa4ull, 0x0000000000000000ull},
    {0x82657970u, Family::OffsetR3, 0xffffffffffffff88ull, 0x0000000000000000ull},
    {0x82657978u, Family::OffsetR3, 0xffffffffffffff84ull, 0x0000000000000000ull},
    {0x826587D0u, Family::OffsetR3, 0xffffffffffffff9cull, 0x0000000000000000ull},
    {0x8266BB80u, Family::OffsetR3, 0x0000000000000038ull, 0x0000000000000000ull},
    {0x82681590u, Family::ConstantWithR11, 0xffffffff82189cccull, 0xffffffff82190000ull},
    {0x8269CB60u, Family::OffsetR3, 0xfffffffffffffda0ull, 0x0000000000000000ull},
    {0x8269CB68u, Family::OffsetR3, 0xfffffffffffffd9cull, 0x0000000000000000ull},
    {0x826A0CD0u, Family::OffsetR3, 0xffffffffffffff90ull, 0x0000000000000000ull},
    {0x826A0D80u, Family::OffsetR3, 0xffffffffffffff94ull, 0x0000000000000000ull},
    {0x826AF218u, Family::ConstantWithR11, 0xffffffff821feebcull, 0xffffffff82200000ull},
    {0x826B5F48u, Family::Constant, 0x0000000000000020ull, 0x0000000000000000ull},
    {0x826BE700u, Family::Constant, 0x00000000000003f1ull, 0x0000000000000000ull},
    {0x826C02B8u, Family::OffsetR3, 0xffffffffffffff6cull, 0x0000000000000000ull},
    {0x826C3478u, Family::MaskR6Bit13, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x826D6C00u, Family::ConstantWithR11, 0xffffffff822025e8ull, 0xffffffff82200000ull},
    {0x826D8638u, Family::Constant, 0x00000000000000fcull, 0x0000000000000000ull},
    {0x826DCDF8u, Family::Constant, 0x0000000000000028ull, 0x0000000000000000ull},
    {0x826DCE18u, Family::Constant, 0x0000000000000030ull, 0x0000000000000000ull},
    {0x826DCE40u, Family::Constant, 0x0000000000000038ull, 0x0000000000000000ull},
    {0x826DCE60u, Family::Constant, 0x0000000000000040ull, 0x0000000000000000ull},
    {0x826DEE00u, Family::OffsetR3, 0x0000000000000008ull, 0x0000000000000000ull},
    {0x826E0718u, Family::ConstantWithR11, 0xffffffff8336c33cull, 0xffffffff83370000ull},
    {0x826E8548u, Family::Constant, 0x0000000000000003ull, 0x0000000000000000ull},
    {0x826ECA30u, Family::ScaledIndex88, 0x00000000000001d0ull, 0x0000000000000000ull},
    {0x826ECA90u, Family::OffsetR3, 0x000000000000016cull, 0x0000000000000000ull},
    {0x826F6850u, Family::Constant, 0x0000000000000024ull, 0x0000000000000000ull},
    {0x826F7670u, Family::ConstantWithR11, 0xffffffff82207244ull, 0xffffffff82200000ull},
    {0x826F7778u, Family::Constant, 0x0000000000000010ull, 0x0000000000000000ull},
    {0x8270AE90u, Family::ConstantWithR11, 0xffffffff8220afb0ull, 0xffffffff82210000ull},
    {0x8270B4F0u, Family::ConstantWithR11, 0xffffffff8220b008ull, 0xffffffff82210000ull},
    {0x8270B538u, Family::ConstantWithR11, 0xffffffff8220b030ull, 0xffffffff82210000ull},
    {0x82713428u, Family::ConstantWithR11, 0xffffffff8218a1dcull, 0xffffffff82190000ull},
    {0x8271E1D8u, Family::ConstantWithR11, 0xffffffff8220e2fcull, 0xffffffff82210000ull},
    {0x8272CE50u, Family::ConstantWithR11, 0xffffffff8218c21cull, 0xffffffff82190000ull},
    {0x82736960u, Family::Constant, 0x0000000000000009ull, 0x0000000000000000ull},
    {0x82746C68u, Family::ConstantWithR11, 0xffffffff821a83d0ull, 0xffffffff821b0000ull},
    {0x827494D0u, Family::ConstantWithR11, 0xffffffff8221291cull, 0xffffffff82210000ull},
    {0x8274AA48u, Family::ConstantWithR11, 0xffffffff821ad0ccull, 0xffffffff821b0000ull},
    {0x827551C0u, Family::ConstantWithR11, 0xffffffff8336c988ull, 0xffffffff83370000ull},
    {0x827551D0u, Family::OffsetR3, 0xfffffffffffffef0ull, 0x0000000000000000ull},
    {0x82755A08u, Family::ConstantWithR11, 0xffffffff82213338ull, 0xffffffff82210000ull},
    {0x82757468u, Family::ConstantWithR11, 0xffffffff821e88f0ull, 0xffffffff821f0000ull},
    {0x8275CF38u, Family::ConstantWithR11, 0xffffffff8336c9e0ull, 0xffffffff83370000ull},
    {0x8275CF48u, Family::OffsetR3, 0xffffffffffffff00ull, 0x0000000000000000ull},
    {0x82760460u, Family::ConstantWithR11, 0xffffffff822136acull, 0xffffffff82210000ull},
    {0x827610F0u, Family::EqualR3ConstantLow32, 0xfffffffffffffffdull, 0x0000000000000000ull},
    {0x82777ED0u, Family::ConstantWithR11, 0xffffffff82214c7cull, 0xffffffff82210000ull},
    {0x82778798u, Family::ConstantWithR11, 0xffffffff8336ca70ull, 0xffffffff83370000ull},
    {0x82782340u, Family::ConstantWithR11, 0xffffffff83243638ull, 0xffffffff83240000ull},
    {0x82782408u, Family::ConstantWithR11, 0xffffffff83243630ull, 0xffffffff83240000ull},
    {0x82782468u, Family::ConstantWithR11, 0xffffffff83243628ull, 0xffffffff83240000ull},
    {0x82794508u, Family::ConstantWithR11, 0xffffffff822153c8ull, 0xffffffff82210000ull},
    {0x82796AA0u, Family::ConstantWithR11, 0xffffffff8336cb78ull, 0xffffffff83370000ull},
    {0x82796AB0u, Family::ConstantWithR11, 0xffffffff8336cb4cull, 0xffffffff83370000ull},
    {0x82796AC0u, Family::ConstantWithR11, 0xffffffff8336cb20ull, 0xffffffff83370000ull},
    {0x82796AD0u, Family::ConstantWithR11, 0xffffffff8336caf4ull, 0xffffffff83370000ull},
    {0x82796AE0u, Family::ConstantWithR11, 0xffffffff8336cac8ull, 0xffffffff83370000ull},
    {0x82796B30u, Family::ConstantWithR11, 0xffffffff8336ca9cull, 0xffffffff83370000ull},
    {0x82799BB8u, Family::Constant, 0x00000000000003f0ull, 0x0000000000000000ull},
    {0x827C57A8u, Family::ConstantWithR11, 0xffffffff8201ffa8ull, 0xffffffff82020000ull},
    {0x827CE740u, Family::NotEqualR3ConstantLow32, 0xfffffffffffffffeull, 0x0000000000000000ull},
    {0x827CE758u, Family::NotEqualR3ConstantLow32, 0xfffffffffffffffdull, 0x0000000000000000ull},
    {0x827D8318u, Family::ConstantWithR11, 0xffffffff820268b4ull, 0xffffffff82020000ull},
    {0x82806FB0u, Family::ConstantWithR11, 0xffffffff82035738ull, 0xffffffff82030000ull},
    {0x828138C0u, Family::ConstantWithR11, 0xffffffff82037b70ull, 0xffffffff82030000ull},
    {0x828138D0u, Family::ConstantWithR11, 0xffffffff82037ba8ull, 0xffffffff82030000ull},
    {0x828138E0u, Family::ConstantWithR11, 0xffffffff82037be0ull, 0xffffffff82030000ull},
    {0x82813BA0u, Family::ConstantWithR11, 0xffffffff82037c38ull, 0xffffffff82030000ull},
    {0x82849380u, Family::Constant, 0xffffffffffffffffull, 0x0000000000000000ull},
    {0x8291C3E8u, Family::Constant, 0x00000000000000e0ull, 0x0000000000000000ull},
    {0x8292BAD0u, Family::ConstantWithR11, 0xffffffff831ebd40ull, 0xffffffff831f0000ull},
    {0x82930020u, Family::ConstantWithR11, 0xffffffff831ebddcull, 0xffffffff831f0000ull},
    {0x82935568u, Family::ConstantWithR11, 0xffffffff831ebf10ull, 0xffffffff831f0000ull},
    {0x829370E8u, Family::Constant, 0x000000000000001cull, 0x0000000000000000ull},
    {0x82938280u, Family::Constant, 0x00000000000000a0ull, 0x0000000000000000ull},
    {0x8293CD40u, Family::Constant, 0x0000000000000008ull, 0x0000000000000000ull},
    {0x8293CD48u, Family::Constant, 0x0000000000000004ull, 0x0000000000000000ull},
    {0x8293DF38u, Family::Constant, 0x0000000000000018ull, 0x0000000000000000ull},
    {0x82940178u, Family::Constant, 0x0000000000000070ull, 0x0000000000000000ull},
    {0x82941FE0u, Family::Constant, 0x0000000000000005ull, 0x0000000000000000ull},
    {0x829664E8u, Family::Constant, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82972120u, Family::ConstantWithR11, 0xffffffff82080148ull, 0xffffffff82080000ull},
    {0x829DAB98u, Family::ConstantWithR11, 0xffffffff8218afb8ull, 0xffffffff82190000ull},
    {0x829E5538u, Family::Constant, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82A680C0u, Family::ConstantWithR11, 0xffffffff820a8550ull, 0xffffffff820b0000ull},
    {0x82A69018u, Family::ConstantWithR11, 0xffffffff820a890cull, 0xffffffff820b0000ull},
    {0x82AB0100u, Family::ConstantWithR11, 0xffffffff832cb550ull, 0xffffffff832ca0e0ull},
    {0x82AF40C8u, Family::LowByteR4, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82B02E70u, Family::CopyR10, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82B8B878u, Family::Constant, 0x000000000000064aull, 0x0000000000000000ull},
    {0x82B8B880u, Family::Constant, 0x00000000000003e8ull, 0x0000000000000000ull},
    {0x82B927D0u, Family::EqualR4R3Low32WithFlag, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82B93A98u, Family::OffsetR3, 0x0000000000000008ull, 0x0000000000000000ull},
    {0x82B93AA0u, Family::OffsetR3, 0x000000000000000cull, 0x0000000000000000ull},
    {0x82B96250u, Family::Constant, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82B970D0u, Family::OffsetR3, 0x0000000000000024ull, 0x0000000000000000ull},
    {0x82B9B4C8u, Family::Constant, 0x0000000000000008ull, 0x0000000000000000ull},
    {0x82B9B598u, Family::Constant, 0x0000000000000014ull, 0x0000000000000000ull},
    {0x82B9CB60u, Family::ConstantWithR11, 0xffffffff832dc180ull, 0xffffffff832e0000ull},
    {0x82BC5190u, Family::OffsetR3, 0xffffffffffffff58ull, 0x0000000000000000ull},
    {0x82BDBEB0u, Family::NonzeroR4Low32, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82BDE0D8u, Family::ConstantWithR11, 0xffffffff83216680ull, 0xffffffff83210000ull},
    {0x82BDE0E8u, Family::ConstantWithR11, 0xffffffff832166e0ull, 0xffffffff83210000ull},
    {0x82BDE0F8u, Family::ConstantWithR11, 0xffffffff83216740ull, 0xffffffff83210000ull},
    {0x82C460E8u, Family::Constant, 0xffffffff80004005ull, 0x0000000000000000ull},
    {0x82CB7CF0u, Family::Constant, 0x00000000000f4240ull, 0x0000000000000000ull},
    {0x82CEEB98u, Family::Constant, 0xffffffff80004001ull, 0x0000000000000000ull},
    {0x82CF5F90u, Family::OffsetR3, 0x000000000000007cull, 0x0000000000000000ull},
    {0x82CF5FC8u, Family::OffsetR3, 0x000000000000011cull, 0x0000000000000000ull},
    {0x82CF5FE8u, Family::OffsetR3, 0x00000000000000ccull, 0x0000000000000000ull},
    {0x82CFA668u, Family::OffsetR3, 0x0000000000000048ull, 0x0000000000000000ull},
    {0x82CFCB50u, Family::Constant, 0xffffffff80500002ull, 0x0000000000000000ull},
    {0x82CFD040u, Family::OffsetR3, 0x000000000000002cull, 0x0000000000000000ull},
    {0x82DF6928u, Family::ConstantWithR11, 0xffffffff8321ffb8ull, 0xffffffff83220000ull},
    {0x82DF6938u, Family::ConstantWithR11, 0xffffffff8321ff08ull, 0xffffffff83220000ull},
    {0x82DF6DE8u, Family::ConstantWithR11, 0xffffffff8321ffc4ull, 0xffffffff83220000ull},
    {0x82DF7370u, Family::Constant, 0x0000000000000064ull, 0x0000000000000000ull},
    {0x82DFBB78u, Family::OffsetR3, 0x00000000000000bcull, 0x0000000000000000ull},
    {0x82E4E6D8u, Family::OffsetR3, 0x0000000000000288ull, 0x0000000000000000ull},
    {0x82E4E6E0u, Family::OffsetR3, 0x00000000000004bcull, 0x0000000000000000ull},
    {0x82E73898u, Family::OffsetR3, 0xfffffffffffffffcull, 0x0000000000000000ull},
    {0x82E7D208u, Family::OffsetR3, 0x0000000000000090ull, 0x0000000000000000ull},
    {0x82EAF1F8u, Family::OffsetR3, 0x0000000000000010ull, 0x0000000000000000ull},
    {0x82EAF200u, Family::Constant, 0x0000000000000006ull, 0x0000000000000000ull},
    {0x82EAF208u, Family::ScaledIndex36, 0x0000000000000070ull, 0x0000000000000000ull},
    {0x82EAF220u, Family::OffsetR3, 0x0000000000000070ull, 0x0000000000000000ull},
    {0x82EAF228u, Family::Constant, 0x000000000000000cull, 0x0000000000000000ull},
    {0x82EAF230u, Family::ConstantWithR11, 0xffffffff83220208ull, 0xffffffff83220000ull},
    {0x82EAF240u, Family::ConstantWithR11, 0xffffffff83220220ull, 0xffffffff83220000ull},
    {0x82EAF250u, Family::ConstantWithR11, 0xffffffff83220280ull, 0xffffffff83220000ull},
    {0x82EF7C08u, Family::OffsetR3, 0x00000000000000c8ull, 0x0000000000000000ull},
    {0x82F018F0u, Family::Constant, 0x0000000000000080ull, 0x0000000000000000ull},
    {0x82F01930u, Family::OffsetR3, 0x00000000000000b0ull, 0x0000000000000000ull},
    {0x82F27258u, Family::ConstantWithR11, 0xffffffff832205f0ull, 0xffffffff83220000ull},
    {0x82F27268u, Family::ConstantWithR11, 0xffffffff83220650ull, 0xffffffff83220000ull},
    {0x82F27278u, Family::ConstantWithR11, 0xffffffff83220680ull, 0xffffffff83220000ull},
    {0x82F27288u, Family::ConstantWithR11, 0xffffffff83220710ull, 0xffffffff83220000ull},
    {0x82F278D8u, Family::ConstantWithR11, 0xffffffff832207a0ull, 0xffffffff83220000ull},
    {0x82F278E8u, Family::ScaledIndexStaticBase, 0xffffffff83220800ull, 0x0000000000000000ull},
    {0x82F2E8F8u, Family::ConstantWithR11, 0xffffffff832208a0ull, 0xffffffff83220000ull},
    {0x82F4C898u, Family::ScaledMaskedIndex, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x82F51D78u, Family::OffsetR3, 0x000000000000001cull, 0x0000000000000000ull},
    {0x82F51E00u, Family::OffsetR3, 0x0000000000000028ull, 0x0000000000000000ull},
    {0x82F5AC18u, Family::Constant, 0x0000000040000000ull, 0x0000000000000000ull},
    {0x82F5C370u, Family::OffsetR3, 0x0000000000000064ull, 0x0000000000000000ull},
    {0x82F5C378u, Family::OffsetR3, 0x0000000000000034ull, 0x0000000000000000ull},
    {0x82F5E0F8u, Family::OffsetR3, 0x0000000000000044ull, 0x0000000000000000ull},
    {0x82F5E120u, Family::OffsetR3, 0x0000000000000050ull, 0x0000000000000000ull},
    {0x82F5E148u, Family::OffsetR3, 0x000000000000008cull, 0x0000000000000000ull},
    {0x82F5ED10u, Family::OffsetR3, 0x000000000000000cull, 0x0000000000000000ull},
    {0x82F5F018u, Family::OffsetR3, 0x000000000000010cull, 0x0000000000000000ull},
    {0x82F8B020u, Family::ConstantWithR11, 0xffffffff832ed884ull, 0xffffffff832f0000ull},
    {0x82F8B030u, Family::ScaledIndex36, 0x0000000000000068ull, 0x0000000000000000ull},
    {0x82F8B048u, Family::OffsetR3, 0x0000000000000068ull, 0x0000000000000000ull},
    {0x82FA6E60u, Family::OffsetR3, 0x0000000000000080ull, 0x0000000000000000ull},
    {0x8303CEF0u, Family::Constant, 0x000000000000000dull, 0x0000000000000000ull},
    {0x83081550u, Family::ConstantWithR11, 0xffffffff82101148ull, 0xffffffff82100000ull},
    {0x83081560u, Family::ConstantWithR11, 0xffffffff82179680ull, 0xffffffff82180000ull},
    {0x83088850u, Family::ConstantWithR11, 0xffffffff8217cc08ull, 0xffffffff82180000ull},
    {0x830888D0u, Family::ConstantWithR11, 0xffffffff82217240ull, 0xffffffff82210000ull},
    {0x83088938u, Family::ConstantWithR11, 0xffffffff8217cce0ull, 0xffffffff82180000ull},
    {0x83088948u, Family::ConstantWithR11, 0xffffffff8217ccecull, 0xffffffff82180000ull},
    {0x83088998u, Family::ConstantWithR11, 0xffffffff8216c820ull, 0xffffffff82170000ull},
    {0x830889E8u, Family::ConstantWithR11, 0xffffffff8217cdc8ull, 0xffffffff82180000ull},
    {0x83088A40u, Family::ConstantWithR11, 0xffffffff8217ce38ull, 0xffffffff82180000ull},
    {0x83088B00u, Family::ConstantWithR11, 0xffffffff8217ceb0ull, 0xffffffff82180000ull},
    {0x83088BC8u, Family::ConstantWithR11, 0xffffffff8217cf28ull, 0xffffffff82180000ull},
    {0x83088C18u, Family::ConstantWithR11, 0xffffffff8217cfa0ull, 0xffffffff82180000ull},
    {0x83088C68u, Family::ConstantWithR11, 0xffffffff8217d018ull, 0xffffffff82180000ull},
    {0x83088CE8u, Family::ConstantWithR11, 0xffffffff8217d088ull, 0xffffffff82180000ull},
    {0x83088D38u, Family::ConstantWithR11, 0xffffffff8217d0f8ull, 0xffffffff82180000ull},
    {0x83088D48u, Family::ConstantWithR11, 0xffffffff8217d170ull, 0xffffffff82180000ull},
    {0x83088D58u, Family::ConstantWithR11, 0xffffffff8217d1e8ull, 0xffffffff82180000ull},
    {0x83088D68u, Family::ConstantWithR11, 0xffffffff8217d260ull, 0xffffffff82180000ull},
    {0x83088DC8u, Family::ConstantWithR11, 0xffffffff8217d2d8ull, 0xffffffff82180000ull},
    {0x83088DD8u, Family::ConstantWithR11, 0xffffffff82171944ull, 0xffffffff82170000ull},
    {0x83088FC8u, Family::ConstantWithR11, 0xffffffff8217d420ull, 0xffffffff82180000ull},
    {0x83089060u, Family::ConstantWithR11, 0xffffffff8217d4a0ull, 0xffffffff82180000ull},
    {0x830890E8u, Family::ConstantWithR11, 0xffffffff8217d510ull, 0xffffffff82180000ull},
    {0x83089170u, Family::ConstantWithR11, 0xffffffff8217d580ull, 0xffffffff82180000ull},
    {0x830891F8u, Family::ConstantWithR11, 0xffffffff8217d5f8ull, 0xffffffff82180000ull},
    {0x83089280u, Family::ConstantWithR11, 0xffffffff8217d670ull, 0xffffffff82180000ull},
    {0x8308AAC8u, Family::ConstantWithR11, 0xffffffff8217d8dcull, 0xffffffff82180000ull},
    {0x8308B328u, Family::ConstantWithR11, 0xffffffff8217d930ull, 0xffffffff82180000ull},
    {0x8308B378u, Family::ConstantWithR11, 0xffffffff8217d968ull, 0xffffffff82180000ull},
    {0x8308B418u, Family::ConstantWithR11, 0xffffffff8217d9a4ull, 0xffffffff82180000ull},
    {0x8308B478u, Family::ConstantWithR11, 0xffffffff8217d9e0ull, 0xffffffff82180000ull},
    {0x8308B4D8u, Family::ConstantWithR11, 0xffffffff8217da18ull, 0xffffffff82180000ull},
    {0x8308B4E8u, Family::ConstantWithR11, 0xffffffff8217da54ull, 0xffffffff82180000ull},
    {0x8308B4F8u, Family::ConstantWithR11, 0xffffffff8217da90ull, 0xffffffff82180000ull},
    {0x8308B508u, Family::ConstantWithR11, 0xffffffff8217dad4ull, 0xffffffff82180000ull},
    {0x8308B518u, Family::ConstantWithR11, 0xffffffff8217db10ull, 0xffffffff82180000ull},
    {0x8308B690u, Family::ConstantWithR11, 0xffffffff8217db4cull, 0xffffffff82180000ull},
    {0x8308B838u, Family::ConstantWithR11, 0xffffffff8217db8cull, 0xffffffff82180000ull},
}};

[[nodiscard]] std::uint64_t LowWordShift(std::uint64_t value, unsigned bits) noexcept
{
    return static_cast<std::uint32_t>(static_cast<std::uint32_t>(value) << bits);
}

[[nodiscard]] std::uint64_t ZeroCount(std::uint64_t value) noexcept
{
    return std::countl_zero(static_cast<std::uint32_t>(value));
}

void Execute(const Entry& entry, Registers& r) noexcept
{
    switch (entry.family)
    {
    case Family::Constant:
        r.r3 = entry.value;
        break;
    case Family::ConstantWithR11:
        r.r11 = entry.auxiliary;
        r.r3 = entry.value;
        break;
    case Family::OffsetR3:
        r.r3 += entry.value;
        break;
    case Family::CopyR4:
        r.r3 = r.r4;
        break;
    case Family::CopyR7:
        r.r3 = r.r7;
        break;
    case Family::CopyR10:
        r.r3 = r.r10;
        break;
    case Family::ScaledIndex88:
        r.r11 = r.r4 * 88u + r.r3;
        r.r3 = r.r11 + entry.value;
        break;
    case Family::EqualR3R5Low32:
        r.r11 = ZeroCount(r.r3 - r.r5);
        r.r3 = r.r11 == 32u;
        break;
    case Family::EqualR4R3Low32:
        r.r11 = ZeroCount(r.r4 - r.r3);
        r.r3 = r.r11 == 32u;
        break;
    case Family::EqualR4R3Low32WithFlag:
        r.r11 = ZeroCount(r.r4 - r.r3) == 32u;
        r.r3 = r.r11;
        break;
    case Family::ZeroR3Low32:
        r.r11 = ZeroCount(r.r3);
        r.r3 = r.r11 == 32u;
        break;
    case Family::EqualR3ConstantLow32:
        r.r11 = ZeroCount(r.r3 + entry.value);
        r.r3 = r.r11 == 32u;
        break;
    case Family::NotEqualR3ConstantLow32:
        r.r11 = ZeroCount(r.r3 + entry.value) == 32u;
        r.r3 = r.r11 ^ 1u;
        break;
    case Family::NonzeroR4Low32:
        r.r11 = ZeroCount(r.r4) == 32u;
        r.r3 = r.r11 ^ 1u;
        break;
    case Family::MaskR6Bit13:
        r.r3 = static_cast<std::uint32_t>(r.r6) & 0x2000u;
        break;
    case Family::LowByteR4:
        r.r3 = static_cast<std::uint8_t>(r.r4);
        break;
    case Family::ScaledIndex36:
        r.r11 = LowWordShift(r.r4, 3) + r.r4;
        r.r11 = LowWordShift(r.r11, 2) + r.r3;
        r.r3 = r.r11 + entry.value;
        break;
    case Family::ScaledIndexStaticBase:
        r.r10 = LowWordShift(r.r3, 1) + r.r3;
        r.r11 = entry.value;
        r.r10 = LowWordShift(r.r10, 2);
        r.r3 = r.r10 + r.r11;
        break;
    case Family::ScaledMaskedIndex:
        r.r11 = static_cast<std::uint32_t>(r.r4) & 0xfffffu;
        r.r11 += 25u;
        r.r10 = LowWordShift(r.r11, 1);
        r.r11 = LowWordShift(r.r11 + r.r10, 2);
        r.r3 += r.r11;
        break;
    }
}

} // namespace

bool Apply(std::uint32_t address, Registers& registers) noexcept
{
    const auto it = std::lower_bound(kEntries.begin(), kEntries.end(), address,
        [](const Entry& entry, std::uint32_t key) { return entry.address < key; });
    if (it == kEntries.end() || it->address != address)
        return false;
    Execute(*it, registers);
    return true;
}

} // namespace lo::semantic::integer_leaf
