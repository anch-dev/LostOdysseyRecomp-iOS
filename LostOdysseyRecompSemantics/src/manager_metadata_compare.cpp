#include "lo_semantics/manager_metadata_compare.h"

#include "lo_semantics/allocation_failure.h"

#include <stdexcept>

namespace lo::semantic::gpu::manager_metadata_compare
{
namespace
{
class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory, CrtThreadDataServices& services,
        InvalidParameterCall& call) : memory_(memory), services_(services), call_(call) {}

    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall state{call_.thread_environment};
        const auto data = GetCrtThreadData(memory_, services_, state);
        call_.thread_environment = state.thread_environment;
        return data;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("errno lookup cannot output a message"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("errno lookup cannot bug-check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("errno lookup cannot invoke the new handler"); }
private:
    GuestMemory& memory_;
    CrtThreadDataServices& services_;
    InvalidParameterCall& call_;
};

std::uint32_t FoldAscii(std::uint16_t unit)
{
    return unit >= 65u && unit <= 90u ? unit + 32u : unit;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services, InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    std::uint64_t& lr, std::uint64_t& result)
{
    if (address != 0x822971E0u)
        return false;
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(lr));
    memory.WriteU32(sp - 96u, sp);
    auto& registers = call.arguments;
    if (static_cast<GuestAddress>(registers[0]) == 0 ||
        static_cast<GuestAddress>(registers[1]) == 0)
    {
        lr = 0x822971F8u;
        ErrorAddressServices errors(memory, thread_services, call);
        const std::uint64_t error_address = GetAllocationErrorAddress(errors);
        registers[7] = 22;
        for (std::size_t index = 0; index < 5; ++index)
            registers[index] = 0;
        memory.WriteU32(static_cast<GuestAddress>(error_address), 22);
        lr = 0x8229721Cu;
        (void)ReportInvalidParameter(memory, invalid_services, call);
        registers[0] = 0x7FFFFFFFu;
    }
    else
    {
        for (;;)
        {
            const std::uint32_t left = FoldAscii(memory.ReadU16(
                static_cast<GuestAddress>(registers[0])));
            const std::uint32_t right = FoldAscii(memory.ReadU16(
                static_cast<GuestAddress>(registers[1])));
            registers[6] = left; // r9 retains the left folded code unit.
            registers[7] = right;
            registers[0] += 2u;
            registers[1] += 2u;
            if (left != 0)
            {
                registers[5] = right;
                if (left == right)
                    continue;
            }
            registers[7] = left;
            registers[0] = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(left) - static_cast<std::int64_t>(right));
            break;
        }
    }
    result = registers[0];
    lr = memory.ReadU32(sp - 8u);
    return true;
}
} // namespace lo::semantic::gpu::manager_metadata_compare
