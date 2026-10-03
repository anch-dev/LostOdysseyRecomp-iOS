#pragma once

#include <cstdint>

namespace lo::semantic::leaf
{

// Exact generated-body family: PPC_FUNC_PROLOGUE(); blr.
// The incoming full 64-bit r3 is returned without changing any guest state.
[[nodiscard]] std::uint64_t PreserveR3(std::uint64_t r3) noexcept;

// Two exact generated-body families: li r3,1; blr and
// li r3,1; mr r8,r8; blr. The latter self-move preserves the full r8.
[[nodiscard]] std::uint64_t ReturnOne() noexcept;

} // namespace lo::semantic::leaf
