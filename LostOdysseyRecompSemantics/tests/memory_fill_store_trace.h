#pragma once

#include <cstdint>
#include <vector>

struct FillStore
{
    std::uint32_t address, width, value;
    bool operator==(const FillStore&) const = default;
};
inline std::vector<FillStore> fill_stores;

inline void FillStore8(std::uint8_t* base, std::uint32_t address, std::uint8_t value)
{
    fill_stores.push_back({address, 1, value});
    *reinterpret_cast<volatile std::uint8_t*>(base + address) = value;
}
inline void FillStore32(std::uint8_t* base, std::uint32_t address, std::uint32_t value)
{
    fill_stores.push_back({address, 4, value});
    *reinterpret_cast<volatile std::uint32_t*>(base + address) = __builtin_bswap32(value);
}

// The pinned original PPC and recovered adapter use these identical store
// observers. Addresses, widths, values and order are compared separately from bytes.
#define PPC_STORE_U8(x, y) FillStore8(base, std::uint32_t(x), std::uint8_t(y))
#define PPC_STORE_U32(x, y) FillStore32(base, std::uint32_t(x), std::uint32_t(y))
