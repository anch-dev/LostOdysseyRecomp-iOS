#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace lo::semantic::gpu
{

using GuestAddress = std::uint32_t;

// A bounded window of guest memory. Guest addresses are offsets in the Xbox
// address space, never pointers into the host process.
class GuestMemory
{
public:
    GuestMemory(GuestAddress base_address, std::span<std::uint8_t> bytes)
        : base_address_(base_address), bytes_(bytes)
    {
        if (static_cast<std::uint64_t>(base_address) + bytes.size() > (std::uint64_t{1} << 32))
            throw std::invalid_argument("guest memory window exceeds the 32-bit address space");
    }

    [[nodiscard]] std::uint8_t ReadU8(GuestAddress address) const
    {
        return bytes_[Offset(address, 1)];
    }

    [[nodiscard]] std::uint32_t ReadU32(GuestAddress address) const
    {
        const auto offset = Offset(address, 4);
        return (std::uint32_t{bytes_[offset]} << 24) |
               (std::uint32_t{bytes_[offset + 1]} << 16) |
               (std::uint32_t{bytes_[offset + 2]} << 8) |
               std::uint32_t{bytes_[offset + 3]};
    }

    void WriteU8(GuestAddress address, std::uint8_t value)
    {
        bytes_[Offset(address, 1)] = value;
    }

    void WriteU32(GuestAddress address, std::uint32_t value)
    {
        const auto offset = Offset(address, 4);
        bytes_[offset] = static_cast<std::uint8_t>(value >> 24);
        bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 16);
        bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 8);
        bytes_[offset + 3] = static_cast<std::uint8_t>(value);
    }

private:
    [[nodiscard]] std::size_t Offset(GuestAddress address, std::size_t width) const
    {
        if (address < base_address_)
            throw std::out_of_range("guest address is outside the mapped window");

        const auto offset = static_cast<std::size_t>(address - base_address_);
        if (offset > bytes_.size() || width > bytes_.size() - offset)
            throw std::out_of_range("guest access exceeds the mapped window");
        return offset;
    }

    GuestAddress base_address_;
    std::span<std::uint8_t> bytes_;
};

} // namespace lo::semantic::gpu
