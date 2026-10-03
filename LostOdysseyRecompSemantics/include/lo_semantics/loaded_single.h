#pragma once

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu
{

// A single-precision memory value loaded into an FPR and stored again without
// arithmetic. PowerPC Programming Environments Rev. 1, Appendix D.6/D.7:
// https://www.nxp.com/docs/en/user-guide/MPCFPE.pdf (printed D-18 through D-20).
// Format movement preserves the NaN signaling bit; a host arithmetic cast can
// quiet it. This type does not model stfs of an arbitrary double or FP arithmetic.
class LoadedSingle
{
public:
    [[nodiscard]] static constexpr LoadedSingle FromWord(std::uint32_t word)
    {
        return LoadedSingle(word);
    }

    [[nodiscard]] constexpr std::uint64_t FprBits() const
    {
        const std::uint64_t sign = std::uint64_t{word_ & 0x80000000u} << 32;
        const std::uint32_t exponent = (word_ >> 23) & 0xffu;
        const std::uint32_t fraction = word_ & 0x007fffffu;
        if (exponent == 0 && fraction != 0)
        {
            const unsigned leading = std::bit_width(fraction) - 1;
            const std::uint64_t normalized = std::uint64_t{fraction} << (52 - leading);
            return sign | (std::uint64_t{874 + leading} << 52) |
                (normalized & 0x000fffffffffffffull);
        }
        const std::uint64_t widened_exponent = exponent == 0xffu ? 0x7ffu :
            (exponent == 0 ? 0u : exponent + 896u);
        return sign | (widened_exponent << 52) | (std::uint64_t{fraction} << 29);
    }

    [[nodiscard]] double FprValue() const
    {
        return std::bit_cast<double>(FprBits());
    }

    [[nodiscard]] constexpr std::uint32_t StoreWord() const { return word_; }

private:
    explicit constexpr LoadedSingle(std::uint32_t word) : word_(word) {}
    std::uint32_t word_;
};

} // namespace lo::semantic::gpu
