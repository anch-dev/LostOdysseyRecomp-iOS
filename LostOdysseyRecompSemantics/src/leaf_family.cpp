#include "lo_semantics/leaf_family.h"

namespace lo::semantic::leaf
{

std::uint64_t PreserveR3(std::uint64_t r3) noexcept
{
    return r3;
}

std::uint64_t ReturnOne() noexcept
{
    return 1;
}

} // namespace lo::semantic::leaf
