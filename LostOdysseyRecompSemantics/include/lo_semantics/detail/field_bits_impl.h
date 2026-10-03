#pragma once

#include "lo_semantics/field_bits.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace lo::semantic::field_bits
{
namespace detail
{

enum class Family : std::uint8_t
{
    ReadFieldBits,
    ReadPointerChainFieldBits,
    InsertFieldBits,
    MaskFieldFlags,
    SetFieldFlags,
};

struct Entry
{
    std::uint32_t address;
    Family family;
    std::array<std::int32_t, 3> offsets;
    std::uint64_t mask;
    std::uint8_t width;
    std::uint8_t load_count;
    std::uint8_t rotate_left;
    bool duplicate_source_word;
    bool base_r4;
};

// Exact candidate membership and field/bit parameters are recorded in
// field_bits_families.json. These are operations on fields, not PPC opcodes.
inline constexpr std::array<Entry, 153> kEntries{{
    {0x822A7DE0u, Family::ReadFieldBits, {540, 0, 0}, 0x0000000000000001ull, 32, 1, 2, true, false},
    {0x822B53B8u, Family::ReadFieldBits, {32, 0, 0}, 0x0000000000000003ull, 32, 1, 29, true, false},
    {0x822F9450u, Family::SetFieldFlags, {88, 0, 0}, 0x0000000020000000ull, 32, 1, 0, false, false},
    {0x823045D8u, Family::ReadFieldBits, {96, 0, 0}, 0x0000000000000001ull, 32, 1, 4, true, false},
    {0x8231EE58u, Family::MaskFieldFlags, {104, 0, 0}, 0x000000007fffffffull, 32, 1, 0, false, false},
    {0x8233D2E8u, Family::MaskFieldFlags, {624, 0, 0}, 0x000000007fffffffull, 32, 1, 0, false, false},
    {0x8238F128u, Family::SetFieldFlags, {124, 0, 0}, 0x0000000080000000ull, 32, 1, 0, false, false},
    {0x823C9078u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 1, true, false},
    {0x823C9398u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 6, true, false},
    {0x823D34A8u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 10, true, false},
    {0x823D7418u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 12, true, false},
    {0x823EE9D8u, Family::ReadPointerChainFieldBits, {152, 84, 0}, 0x0000000000000001ull, 32, 2, 1, true, false},
    {0x823EE9E8u, Family::ReadPointerChainFieldBits, {152, 84, 0}, 0x0000000000000001ull, 32, 2, 2, true, false},
    {0x82498F0Cu, Family::ReadFieldBits, {0, 0, 0}, 0x0000000000000001ull, 32, 1, 5, true, false},
    {0x82498F18u, Family::ReadFieldBits, {0, 0, 0}, 0x0000000000000001ull, 32, 1, 6, true, false},
    {0x82498F24u, Family::ReadFieldBits, {0, 0, 0}, 0x0000000000000001ull, 32, 1, 7, true, false},
    {0x824B1DD8u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 2, true, false},
    {0x824B1DE8u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 3, true, false},
    {0x824B1DF8u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 4, true, false},
    {0x824B1E08u, Family::ReadPointerChainFieldBits, {152, 500, 0}, 0x0000000000000001ull, 32, 2, 5, true, false},
    {0x824D56F0u, Family::ReadFieldBits, {532, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x824E7C38u, Family::SetFieldFlags, {120, 0, 0}, 0x0000000040000000ull, 32, 1, 0, false, false},
    {0x82500158u, Family::MaskFieldFlags, {232, 0, 0}, 0xffffffffbfffffffull, 32, 1, 0, true, false},
    {0x82540178u, Family::SetFieldFlags, {548, 0, 0}, 0x0000000000000020ull, 32, 1, 0, false, false},
    {0x8255C7A0u, Family::ReadFieldBits, {8, 0, 0}, 0x00000000fffffffeull, 32, 1, 1, true, false},
    {0x8256CFB8u, Family::ReadFieldBits, {140, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x825A7C88u, Family::ReadFieldBits, {500, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x825A7C98u, Family::ReadFieldBits, {500, 0, 0}, 0x0000000000000001ull, 32, 1, 2, true, false},
    {0x825A7CA8u, Family::ReadFieldBits, {500, 0, 0}, 0x0000000000000001ull, 32, 1, 9, true, false},
    {0x825BAA38u, Family::ReadFieldBits, {192, 0, 0}, 0x0000000000000001ull, 32, 1, 11, true, false},
    {0x825BB6A8u, Family::MaskFieldFlags, {192, 0, 0}, 0x000000007fffffffull, 32, 1, 0, false, false},
    {0x825F6EF0u, Family::SetFieldFlags, {76, 0, 0}, 0x0000000040000000ull, 32, 1, 0, false, false},
    {0x82600008u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x8260F4B0u, Family::ReadFieldBits, {68, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x82631820u, Family::SetFieldFlags, {348, 0, 0}, 0x0000000008000000ull, 32, 1, 0, false, false},
    {0x826467D8u, Family::SetFieldFlags, {648, 0, 0}, 0x0000000040000000ull, 32, 1, 0, false, false},
    {0x826476F8u, Family::SetFieldFlags, {800, 0, 0}, 0x0000000080000000ull, 32, 1, 0, false, false},
    {0x8266E768u, Family::ReadFieldBits, {260, 0, 0}, 0x0000000000000001ull, 32, 1, 2, true, false},
    {0x8266E850u, Family::ReadFieldBits, {224, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x8268F858u, Family::ReadFieldBits, {88, 0, 0}, 0x0000000000000001ull, 32, 1, 2, true, false},
    {0x8268F868u, Family::InsertFieldBits, {88, 0, 0}, 0x0000000040000000ull, 32, 1, 30, true, false},
    {0x826AA948u, Family::SetFieldFlags, {64, 0, 0}, 0x0000000040000000ull, 32, 1, 0, false, false},
    {0x826B8DD0u, Family::ReadFieldBits, {16, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x826D6308u, Family::MaskFieldFlags, {264, 0, 0}, 0x000000003fffffffull, 32, 1, 0, false, false},
    {0x826D6318u, Family::SetFieldFlags, {264, 0, 0}, 0x0000000080000000ull, 32, 1, 0, false, false},
    {0x826DCE70u, Family::ReadFieldBits, {8, 0, 0}, 0x00000000ffffffc0ull, 32, 1, 6, true, false},
    {0x826DCE80u, Family::ReadFieldBits, {8, 0, 0}, 0x00000000fffffff0ull, 32, 1, 4, true, false},
    {0x826DD080u, Family::ReadFieldBits, {8, 0, 0}, 0x00000000fffffff8ull, 32, 1, 3, true, false},
    {0x826DD188u, Family::ReadFieldBits, {8, 0, 0}, 0x00000000fffffffcull, 32, 1, 2, true, false},
    {0x8270BA38u, Family::SetFieldFlags, {128, 0, 0}, 0x0000000010000000ull, 32, 1, 0, false, false},
    {0x8270BCC8u, Family::ReadFieldBits, {128, 0, 0}, 0x0000000000000001ull, 32, 1, 5, true, false},
    {0x8270BCD8u, Family::InsertFieldBits, {128, 0, 0}, 0x0000000008000000ull, 32, 1, 27, true, false},
    {0x8270BDE8u, Family::ReadFieldBits, {128, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x82778A10u, Family::InsertFieldBits, {52, 0, 0}, 0x0000000080000000ull, 32, 1, 31, true, false},
    {0x82778A20u, Family::InsertFieldBits, {52, 0, 0}, 0x0000000040000000ull, 32, 1, 30, true, false},
    {0x827B2C60u, Family::ReadFieldBits, {10568, 0, 0}, 0x0000000000000007ull, 32, 1, 0, false, false},
    {0x827B2C70u, Family::ReadFieldBits, {10568, 0, 0}, 0x00000000000000ffull, 32, 1, 29, true, false},
    {0x827B2C80u, Family::ReadFieldBits, {10556, 0, 0}, 0x0000000000000001ull, 32, 1, 29, true, false},
    {0x827B2C90u, Family::ReadFieldBits, {11844, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x827B2CA0u, Family::ReadFieldBits, {11840, 0, 0}, 0x0000000000000007ull, 32, 1, 27, true, false},
    {0x827B2CB0u, Family::ReadFieldBits, {11840, 0, 0}, 0x000000000000001full, 32, 1, 0, false, false},
    {0x827B2CC0u, Family::ReadFieldBits, {11840, 0, 0}, 0x000000000000001full, 32, 1, 24, true, false},
    {0x827B2CD0u, Family::ReadFieldBits, {11840, 0, 0}, 0x0000000000000007ull, 32, 1, 11, true, false},
    {0x827B2CE0u, Family::ReadFieldBits, {11840, 0, 0}, 0x000000000000001full, 16, 1, 0, false, false},
    {0x827B2CF0u, Family::ReadFieldBits, {11840, 0, 0}, 0x000000000000001full, 8, 1, 0, false, false},
    {0x827B2D00u, Family::ReadFieldBits, {11844, 0, 0}, 0x0000000000000001ull, 32, 1, 2, true, false},
    {0x827B2D40u, Family::ReadFieldBits, {10556, 0, 0}, 0x0000000000000007ull, 32, 1, 0, false, false},
    {0x827B2E98u, Family::ReadFieldBits, {10680, 0, 0}, 0x0000000000000001ull, 32, 1, 22, true, false},
    {0x827B2F38u, Family::ReadFieldBits, {22280, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
    {0x827B2F50u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000001ull, 32, 1, 30, true, false},
    {0x827B2F60u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 28, true, false},
    {0x827B2F78u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000001ull, 32, 1, 25, true, false},
    {0x827B2F88u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 24, true, false},
    {0x827B2F98u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 21, true, false},
    {0x827B2FA8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 15, true, false},
    {0x827B2FB8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 18, true, false},
    {0x827B2FC8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 12, true, false},
    {0x827B2FD8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 9, true, false},
    {0x827B2FE8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 3, true, false},
    {0x827B2FF8u, Family::ReadFieldBits, {10548, 0, 0}, 0x0000000000000007ull, 32, 1, 6, true, false},
    {0x827B30C0u, Family::ReadFieldBits, {10564, 0, 0}, 0x000000000000003full, 32, 1, 0, false, false},
    {0x827B3128u, Family::ReadFieldBits, {10568, 0, 0}, 0x0000000000000001ull, 32, 1, 17, true, false},
    {0x827B35A0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 0, false, false},
    {0x827B35B0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 28, true, false},
    {0x827B35C0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 24, true, false},
    {0x827B35D0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 20, true, false},
    {0x827B35E0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 16, 1, 0, false, false},
    {0x827B35F0u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 12, true, false},
    {0x827B3600u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 8, 1, 0, false, false},
    {0x827B3610u, Family::ReadFieldBits, {10540, 0, 0}, 0x000000000000000full, 32, 1, 4, true, false},
    {0x827B3620u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 0, false, false},
    {0x827B3630u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 28, true, false},
    {0x827B3640u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 24, true, false},
    {0x827B3650u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 20, true, false},
    {0x827B3660u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 16, 1, 0, false, false},
    {0x827B3670u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 12, true, false},
    {0x827B3680u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 8, 1, 0, false, false},
    {0x827B3690u, Family::ReadFieldBits, {10544, 0, 0}, 0x000000000000000full, 32, 1, 4, true, false},
    {0x827B36E0u, Family::ReadFieldBits, {10572, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
    {0x827B39A0u, Family::ReadFieldBits, {10616, 0, 0}, 0x0000000000000003ull, 32, 1, 0, false, false},
    {0x827B39D8u, Family::ReadFieldBits, {10688, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
    {0x827B3A08u, Family::ReadFieldBits, {10568, 0, 0}, 0x0000000000000001ull, 32, 1, 11, true, false},
    {0x827B3A60u, Family::ReadFieldBits, {10556, 0, 0}, 0x0000000000000001ull, 32, 1, 28, true, false},
    {0x827B3B80u, Family::ReadFieldBits, {11844, 0, 0}, 0x0000000000000007ull, 32, 1, 12, true, false},
    {0x827B3BE8u, Family::ReadFieldBits, {11844, 0, 0}, 0x0000000000000007ull, 32, 1, 15, true, false},
    {0x827B3BF8u, Family::ReadFieldBits, {10560, 0, 0}, 0x0000000000000001ull, 32, 1, 29, true, false},
    {0x827B3C08u, Family::ReadFieldBits, {10560, 0, 0}, 0x0000000000000001ull, 32, 1, 30, true, false},
    {0x827B3C18u, Family::ReadFieldBits, {10560, 0, 0}, 0x0000000000000001ull, 32, 1, 27, true, false},
    {0x827B3C40u, Family::InsertFieldBits, {11844, 0, 0}, 0x000000003f800000ull, 32, 1, 23, true, false},
    {0x827B3C50u, Family::ReadFieldBits, {11844, 0, 0}, 0x000000000000007full, 32, 1, 9, true, false},
    {0x827BC610u, Family::SetFieldFlags, {10943, 0, 0}, 0x0000000000000002ull, 8, 1, 0, false, false},
    {0x82804B68u, Family::ReadFieldBits, {84, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x82804B78u, Family::ReadFieldBits, {84, 0, 0}, 0x0000000000000001ull, 32, 1, 3, true, false},
    {0x82805420u, Family::ReadPointerChainFieldBits, {152, 84, 0}, 0x0000000000000001ull, 32, 2, 3, true, false},
    {0x82834080u, Family::InsertFieldBits, {1036, 0, 0}, 0x0000000080000000ull, 32, 1, 31, true, false},
    {0x82851598u, Family::ReadFieldBits, {8, 0, 0}, 0x0000000000000001ull, 8, 1, 31, true, false},
    {0x828A27E8u, Family::InsertFieldBits, {176, 0, 0}, 0x0000000080000000ull, 32, 1, 31, true, false},
    {0x828A27F8u, Family::InsertFieldBits, {176, 0, 0}, 0x0000000040000000ull, 32, 1, 30, true, false},
    {0x828A2808u, Family::InsertFieldBits, {176, 0, 0}, 0x0000000020000000ull, 32, 1, 29, true, false},
    {0x828A2938u, Family::ReadFieldBits, {176, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x82910C10u, Family::InsertFieldBits, {88, 0, 0}, 0x0000000004000000ull, 32, 1, 26, true, false},
    {0x82910C20u, Family::InsertFieldBits, {88, 0, 0}, 0x0000000008000000ull, 32, 1, 27, true, false},
    {0x82910C30u, Family::InsertFieldBits, {88, 0, 0}, 0x0000000010000000ull, 32, 1, 28, true, false},
    {0x82910C40u, Family::InsertFieldBits, {88, 0, 0}, 0x0000000002000000ull, 32, 1, 25, true, false},
    {0x82914240u, Family::SetFieldFlags, {64, 0, 0}, 0x0000000080000000ull, 32, 1, 0, false, false},
    {0x82914250u, Family::MaskFieldFlags, {64, 0, 0}, 0x000000007fffffffull, 32, 1, 0, false, false},
    {0x829C8A08u, Family::ReadFieldBits, {92, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x829C8AB0u, Family::ReadFieldBits, {592, 0, 0}, 0x0000000000000001ull, 32, 1, 1, true, false},
    {0x829D07E0u, Family::MaskFieldFlags, {720, 0, 0}, 0xffffffffefffffffull, 32, 1, 0, true, false},
    {0x829DAE58u, Family::InsertFieldBits, {1480, 0, 0}, 0x0000000000000008ull, 32, 1, 3, true, false},
    {0x829DAE68u, Family::ReadFieldBits, {1480, 0, 0}, 0x0000000000000001ull, 32, 1, 29, true, false},
    {0x82A15310u, Family::InsertFieldBits, {1480, 0, 0}, 0x0000000000000040ull, 32, 1, 6, true, false},
    {0x82A15390u, Family::ReadFieldBits, {1480, 0, 0}, 0x0000000000000001ull, 32, 1, 23, true, false},
    {0x82A153A0u, Family::ReadFieldBits, {1480, 0, 0}, 0x0000000000000001ull, 32, 1, 24, true, false},
    {0x82A153B0u, Family::ReadFieldBits, {1480, 0, 0}, 0x0000000000000001ull, 32, 1, 25, true, false},
    {0x82A2D5D0u, Family::InsertFieldBits, {1756, 0, 0}, 0x0000000008000000ull, 32, 1, 27, true, false},
    {0x82A31880u, Family::InsertFieldBits, {1832, 0, 0}, 0x0000000080000000ull, 32, 1, 31, true, false},
    {0x82AB0320u, Family::MaskFieldFlags, {124, 0, 0}, 0x000000007fffffffull, 32, 1, 0, false, false},
    {0x82ACD580u, Family::SetFieldFlags, {100, 0, 0}, 0x0000000040000000ull, 32, 1, 0, false, true},
    {0x82BD3D70u, Family::MaskFieldFlags, {4, 0, 0}, 0xfffffffffffffff3ull, 32, 1, 0, true, false},
    {0x82BDB330u, Family::ReadFieldBits, {4, 0, 0}, 0x00000000ffffffe0ull, 32, 1, 5, true, false},
    {0x82C30F20u, Family::SetFieldFlags, {1376, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
    {0x82C311A8u, Family::SetFieldFlags, {1376, 0, 0}, 0x0000000000001000ull, 32, 1, 0, false, false},
    {0x82CC5780u, Family::ReadFieldBits, {4, 0, 0}, 0x0000000000000001ull, 32, 1, 14, true, false},
    {0x82E4E6E8u, Family::ReadFieldBits, {768, 0, 0}, 0x0000000000000001ull, 32, 1, 27, true, false},
    {0x82F5ABE8u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
    {0x82F5ABF8u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 31, true, false},
    {0x82F5AC08u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 30, true, false},
    {0x82F5AC20u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 28, true, false},
    {0x82F5AD08u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 26, true, false},
    {0x82F5AD20u, Family::ReadFieldBits, {76, 0, 0}, 0x0000000000000001ull, 32, 1, 24, true, false},
    {0x82F70A28u, Family::ReadPointerChainFieldBits, {4, 4, 76}, 0x0000000000000001ull, 32, 3, 31, true, false},
    {0x82FA6E68u, Family::ReadFieldBits, {8, 0, 0}, 0x0000000000000001ull, 32, 1, 0, false, false},
}};

inline gpu::GuestAddress FieldAddress(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<gpu::GuestAddress>(base) +
           static_cast<gpu::GuestAddress>(displacement);
}

template <typename Memory>
std::uint32_t ReadField(Memory& memory, gpu::GuestAddress address,
                        std::uint8_t width)
{
    switch (width)
    {
    case 8: return memory.ReadU8(address);
    case 16: return memory.ReadU16(address);
    default: return memory.ReadU32(address);
    }
}

template <typename Memory>
void WriteField(Memory& memory, gpu::GuestAddress address,
                std::uint8_t width, std::uint64_t value)
{
    if (width == 8)
        memory.WriteU8(address, static_cast<std::uint8_t>(value));
    else
        memory.WriteU32(address, static_cast<std::uint32_t>(value));
}

inline std::uint64_t DuplicateWord(std::uint64_t value)
{
    const auto word = static_cast<std::uint32_t>(value);
    return std::uint64_t{word} | (std::uint64_t{word} << 32);
}

inline std::uint64_t SelectBits(std::uint64_t value, const Entry& entry)
{
    const auto source = entry.duplicate_source_word ?
        std::rotl(DuplicateWord(value), entry.rotate_left) :
        std::uint64_t{static_cast<std::uint32_t>(value)};
    return source & entry.mask;
}

template <typename Memory>
void ReadBits(const Entry& entry, Registers& registers, Memory& memory)
{
    for (std::uint8_t index = 0; index < entry.load_count; ++index)
    {
        const auto base = index == 0 ? registers.r3 : registers.r11;
        registers.r11 = ReadField(memory, FieldAddress(base, entry.offsets[index]),
                                  entry.width);
    }
    registers.r3 = SelectBits(registers.r11, entry);
}

template <typename Memory>
void UpdateFlags(const Entry& entry, Registers& registers, Memory& memory)
{
    const auto base = entry.base_r4 ? registers.r4 : registers.r3;
    const auto address = FieldAddress(base, entry.offsets[0]);
    registers.r11 = ReadField(memory, address, entry.width);
    switch (entry.family)
    {
    case Family::InsertFieldBits:
        registers.r11 = (std::rotl(DuplicateWord(registers.r4), entry.rotate_left) &
                         entry.mask) | (registers.r11 & ~entry.mask);
        break;
    case Family::MaskFieldFlags:
        registers.r11 = SelectBits(registers.r11, entry);
        break;
    case Family::SetFieldFlags:
        registers.r11 |= entry.mask;
        break;
    default: break;
    }
    WriteField(memory, address, entry.width, registers.r11);
}

} // namespace detail

template <typename Memory>
bool ApplyWith(std::uint32_t address, Registers& registers, Memory& memory)
{
    using namespace detail;
    const auto found = std::lower_bound(kEntries.begin(), kEntries.end(), address,
        [](const Entry& entry, std::uint32_t sought) { return entry.address < sought; });
    if (found == kEntries.end() || found->address != address)
        return false;

    if (found->family == Family::ReadFieldBits ||
        found->family == Family::ReadPointerChainFieldBits)
        ReadBits(*found, registers, memory);
    else
        UpdateFlags(*found, registers, memory);
    return true;
}

} // namespace lo::semantic::field_bits
