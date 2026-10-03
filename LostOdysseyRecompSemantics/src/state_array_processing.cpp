#include "lo_semantics/state_array_processing.h"

#include "lo_semantics/allocation_array.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::state_array_processing
{
namespace
{
constexpr GuestAddress kCounter = 0x83315EE4u;
constexpr GuestAddress kQueueHead = 0x83315EF0u;
constexpr GuestAddress kParentFlag = 0x83315EDCu;
constexpr std::uint64_t kGlobalBase = static_cast<std::uint64_t>(
    static_cast<std::int64_t>(-2093940736));

GuestAddress Low(std::uint64_t value)
{ return static_cast<GuestAddress>(value); }

std::int32_t Signed(std::uint32_t value)
{ return std::bit_cast<std::int32_t>(value); }

std::uint64_t& R(FrameRegisters& frame, unsigned number)
{ return frame.gpr[number - 21u]; }

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, Low(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

std::uint64_t GrownCapacity(std::uint64_t count)
{
    const std::uint32_t doubled = (Low(count) << 1u) & 0xFFFFFFFEu;
    const std::uint32_t triple = Low(count + doubled);
    const auto eighth = static_cast<std::int64_t>(Signed(triple)) / 8;
    return static_cast<std::uint64_t>(eighth) + count + 32u;
}

void SaveHubFrame(GuestMemory& memory, std::uint64_t caller_sp,
                  FrameRegisters& frame)
{
    const std::uint64_t incoming_lr = frame.lr;
    for (unsigned reg = 21; reg <= 31; ++reg)
        WriteU64(memory, Low(caller_sp - (33u - reg) * 8u), R(frame, reg));
    memory.WriteU32(Low(caller_sp - 8u), Low(incoming_lr));
    frame.lr = 0x823FDAB8u; // bl __savegprlr_21 returns here.
    memory.WriteU32(Low(caller_sp - 192u), Low(caller_sp));
}

void RestoreHubFrame(GuestMemory& memory, std::uint64_t caller_sp,
                     FrameRegisters& frame)
{
    for (unsigned reg = 21; reg <= 31; ++reg)
        R(frame, reg) = ReadU64(memory,
            Low(caller_sp - (33u - reg) * 8u));
    frame.lr = memory.ReadU32(Low(caller_sp - 8u));
}

void SaveLowerFrame(GuestMemory& memory, std::uint64_t caller_sp,
                    FrameRegisters& frame, unsigned first, unsigned size)
{
    for (unsigned reg = first; reg <= 31; ++reg)
        WriteU64(memory, Low(caller_sp - (33u - reg) * 8u), R(frame, reg));
    memory.WriteU32(Low(caller_sp - 8u), Low(frame.lr));
    memory.WriteU32(Low(caller_sp - size), Low(caller_sp));
}

void RestoreLowerFrame(GuestMemory& memory, std::uint64_t caller_sp,
                       FrameRegisters& frame, unsigned first)
{
    for (unsigned reg = first; reg <= 31; ++reg)
        R(frame, reg) = ReadU64(memory,
            Low(caller_sp - (33u - reg) * 8u));
    frame.lr = memory.ReadU32(Low(caller_sp - 8u));
}

void InitializeCurrentManager(GuestMemory& memory,
    ManagerInitServices& manager_init, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    // 827C5F38's known 112-byte frame surrounds its recovered semantic API.
    memory.WriteU32(Low(caller_sp - 8u), Low(frame.lr));
    WriteU64(memory, Low(caller_sp - 24u), R(frame, 30));
    WriteU64(memory, Low(caller_sp - 16u), R(frame, 31));
    memory.WriteU32(Low(caller_sp - 112u), Low(caller_sp));
    (void)lo::semantic::gpu::InitializeManager(memory, manager_init,
                                               Low(caller_sp - 112u));
    frame.lr = memory.ReadU32(Low(caller_sp - 8u));
    R(frame, 30) = ReadU64(memory, Low(caller_sp - 24u));
    R(frame, 31) = ReadU64(memory, Low(caller_sp - 16u));
}

GuestAddress CurrentManager(GuestMemory& memory,
    ManagerInitServices& manager_init, std::uint64_t caller_sp,
    FrameRegisters& frame, GuestAddress return_address)
{
    GuestAddress manager = memory.ReadU32(Low(R(frame, 25) - 18936u));
    if (manager == 0)
    {
        frame.lr = return_address;
        InitializeCurrentManager(memory, manager_init, caller_sp, frame);
        manager = memory.ReadU32(Low(R(frame, 25) - 18936u));
    }
    return manager;
}

// Existing array helpers accept only the low32 resize result. Capture the
// original full r3 for this caller's no-release residual and keep the nested
// callback's full signed mullw byte register distinct from direct rlwinm bytes.
class LowerArrayServices final : public ArrayResizeServices
{
public:
    LowerArrayServices(GuestMemory& memory, ManagerInitServices& manager_init,
        StateArrayCallbacks& callbacks, std::uint64_t caller_sp,
        FrameRegisters frame, std::uint64_t full_bytes)
        : memory_(memory), manager_init_(manager_init), callbacks_(callbacks),
          caller_sp_(caller_sp), frame_(frame), full_bytes_(full_bytes) {}

    void InitializeManager() override
    {
        frame_.lr = 0x8229F6C0u;
        InitializeCurrentManager(memory_, manager_init_, caller_sp_, frame_);
    }

    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        if (Low(full_bytes_) != bytes)
            throw std::runtime_error("nested resize byte register changed");
        frame_.lr = 0x8229F6E0u;
        full_result_ = callbacks_.ResizeStorage(method, memory_, manager,
            old_storage, full_bytes_, argument, caller_sp_, frame_);
        ++resize_calls_;
        return Low(full_result_);
    }

    [[nodiscard]] std::uint64_t FullResult() const { return full_result_; }
    [[nodiscard]] unsigned ResizeCalls() const { return resize_calls_; }

private:
    GuestMemory& memory_;
    ManagerInitServices& manager_init_;
    StateArrayCallbacks& callbacks_;
    std::uint64_t caller_sp_;
    FrameRegisters frame_;
    std::uint64_t full_bytes_;
    std::uint64_t full_result_{};
    unsigned resize_calls_{};
};

std::uint64_t NestedBytes(GuestMemory& memory, GuestAddress array,
                          std::uint32_t element_size)
{
    const auto capacity = Signed(memory.ReadU32(array + 8u));
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(capacity) * Signed(element_size));
}

void CallResizeArray(GuestMemory& memory, ManagerInitServices& manager_init,
    StateArrayCallbacks& callbacks, std::uint64_t array_full,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    frame.lr = 0x823FDB34u;
    SaveLowerFrame(memory, caller_sp, frame, 27, 128);
    const auto nested_sp = caller_sp - 128u;
    FrameRegisters lower_frame = frame;
    R(lower_frame, 27) = 8;
    R(lower_frame, 28) = memory.ReadU32(Low(array_full));
    R(lower_frame, 29) = NestedBytes(memory, Low(array_full), 4);
    R(lower_frame, 30) = kGlobalBase;
    R(lower_frame, 31) = array_full;
    LowerArrayServices lower(memory, manager_init, callbacks, nested_sp,
                             lower_frame, R(lower_frame, 29));
    lo::semantic::gpu::ResizeArray(memory, lower, Low(array_full), 4, 8);
    RestoreLowerFrame(memory, caller_sp, frame, 27);
}

bool RemoveShrinks(std::uint32_t count, std::uint32_t capacity)
{
    const bool consider = Signed(count + count * 2u) <
        Signed(capacity * 2u) ||
        Signed((capacity - count) * 4u) >= 16384;
    return consider && (Signed(capacity - count) > 64 ||
                        Signed(count) == 0);
}

std::uint64_t CallRemoveArrayRange(GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t array_full, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    frame.lr = 0x823FDCD0u;
    SaveLowerFrame(memory, caller_sp, frame, 28, 128);
    const auto remove_sp = caller_sp - 128u;
    const GuestAddress array = Low(array_full);
    const GuestAddress storage = memory.ReadU32(array);
    const std::uint32_t count = memory.ReadU32(array + 4u);
    const std::uint32_t capacity = memory.ReadU32(array + 8u);
    const bool shrink = RemoveShrinks(count, capacity);

    FrameRegisters lower_frame = frame;
    R(lower_frame, 28) = 8;
    R(lower_frame, 29) = 0;
    R(lower_frame, 30) = 4;
    R(lower_frame, 31) = array_full;
    if (shrink)
    {
        // RemoveArrayRange calls ResizeArray after its equal-pointer move.
        // Its real savegprlr_27 spills these then writes the backchain.
        lower_frame.lr = 0x82298B9Cu;
        SaveLowerFrame(memory, remove_sp, lower_frame, 27, 128);
    }
    FrameRegisters resize_frame = lower_frame;
    R(resize_frame, 27) = 8;
    R(resize_frame, 28) = storage;
    R(resize_frame, 29) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(Signed(count)) * 4);
    R(resize_frame, 30) = kGlobalBase;
    R(resize_frame, 31) = array_full;
    LowerArrayServices lower(memory, manager_init, callbacks,
        remove_sp - 128u, resize_frame, R(resize_frame, 29));
    lo::semantic::gpu::RemoveArrayRange(memory, lower, array, 0, 0,
                                        4, 8, Low(remove_sp));

    // With first=count=0, MoveGuestMemory receives equal source/destination
    // (both zero-extended storage) and returns before CopyGuestMemory's spill.
    // RemoveArrayRange then either leaves that r3 alone or tail-calls resize.
    std::uint64_t residual = storage;
    if (shrink)
    {
        if (storage == 0 && Signed(count) == 0)
            residual = array_full; // ResizeArray early return keeps full r3.
        else if (lower.ResizeCalls() == 1)
            residual = lower.FullResult();
        else
            throw std::runtime_error("nested remove resize branch changed");
    }
    RestoreLowerFrame(memory, caller_sp, frame, 28);
    return residual;
}

std::uint64_t DrainInitialQueue(GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t full_sp, FrameRegisters& frame, std::uint64_t head)
{
    const std::uint64_t array_full = full_sp + 80u;
    while (Low(head) != 0)
    {
        R(frame, 30) = R(frame, 31);
        R(frame, 31) += 1u;
        const bool grow = Signed(Low(R(frame, 31))) >
                          Signed(Low(R(frame, 27)));
        memory.WriteU32(Low(full_sp + 84u), Low(R(frame, 31)));
        if (grow)
        {
            memory.WriteU32(Low(full_sp + 88u),
                            Low(GrownCapacity(R(frame, 31))));
            CallResizeArray(memory, manager_init, callbacks, array_full,
                            full_sp, frame);
            R(frame, 27) = memory.ReadU32(Low(full_sp + 88u));
            R(frame, 31) = memory.ReadU32(Low(full_sp + 84u));
            R(frame, 29) = memory.ReadU32(Low(full_sp + 80u));
            head = memory.ReadU32(Low(R(frame, 28) + 24304u));
        }
        const std::uint64_t slot =
            static_cast<std::uint32_t>(Low(R(frame, 30)) * 4u) + R(frame, 29);
        if (Low(slot) != 0)
        {
            memory.WriteU32(Low(slot), Low(head));
            head = memory.ReadU32(Low(R(frame, 28) + 24304u));
        }
        head = memory.ReadU32(Low(head) + 32u);
        memory.WriteU32(Low(R(frame, 28) + 24304u), Low(head));
    }
    return head;
}

void ProcessQueuedItems(GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t full_sp, FrameRegisters& frame)
{
    if (Signed(Low(R(frame, 31))) <= 0)
        return;
    R(frame, 23) = R(frame, 21);
    R(frame, 24) = R(frame, 21);
    for (;;)
    {
        const GuestAddress item = memory.ReadU32(
            Low(R(frame, 24)) + Low(R(frame, 29)));
        const auto marker = memory.ReadU32(item + 4u);
        std::uint64_t head = 0;
        if (Signed(marker) == -1)
        {
            const GuestAddress vtable = memory.ReadU32(item);
            const GuestAddress method = memory.ReadU32(vtable + 124u) & ~3u;
            frame.lr = 0x823FDBA0u;
            callbacks.VisitItem(method, memory, item, full_sp, frame);
            head = memory.ReadU32(Low(R(frame, 28) + 24304u));
        }
        if (Low(head) != 0)
        {
            do
            {
                R(frame, 26) = R(frame, 31);
                R(frame, 31) += 1u;
                const bool grow = Signed(Low(R(frame, 31))) >
                                  Signed(Low(R(frame, 27)));
                if (grow)
                {
                    R(frame, 27) = GrownCapacity(R(frame, 31));
                    if (Low(R(frame, 29)) != 0 ||
                        Signed(Low(R(frame, 27))) != 0)
                    {
                        const GuestAddress manager = CurrentManager(memory,
                            manager_init, full_sp, frame, 0x823FDBF8u);
                        const GuestAddress vtable = memory.ReadU32(manager);
                        const GuestAddress method =
                            memory.ReadU32(vtable + 8u) & ~3u;
                        R(frame, 30) =
                            static_cast<std::uint32_t>(Low(R(frame, 27)) * 4u);
                        frame.lr = 0x823FDC18u;
                        const std::uint64_t resized = callbacks.ResizeStorage(
                            method, memory, manager, R(frame, 29),
                            R(frame, 30), 8,
                            full_sp, frame);
                        head = memory.ReadU32(Low(R(frame, 28) + 24304u));
                        R(frame, 29) = resized;
                    }
                }
                const std::uint64_t slot =
                    static_cast<std::uint32_t>(Low(R(frame, 26)) * 4u) +
                    R(frame, 29);
                if (Low(slot) != 0)
                {
                    memory.WriteU32(Low(slot), Low(head));
                    head = memory.ReadU32(Low(R(frame, 28) + 24304u));
                }
                head = memory.ReadU32(Low(head) + 32u);
                memory.WriteU32(Low(R(frame, 28) + 24304u), Low(head));
            } while (Low(head) != 0);
        }
        R(frame, 23) += 1u;
        R(frame, 24) += 4u;
        if (Signed(Low(R(frame, 23))) >= Signed(Low(R(frame, 31))))
            break;
    }
    memory.WriteU32(Low(full_sp + 80u), Low(R(frame, 29)));
    memory.WriteU32(Low(full_sp + 88u), Low(R(frame, 27)));
}

std::uint64_t ProcessStateQueue(GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveHubFrame(memory, caller_sp, frame);
    const std::uint64_t sp = caller_sp - 192u;
    R(frame, 22) = kGlobalBase;
    R(frame, 21) = 0;
    R(frame, 28) = kGlobalBase;
    R(frame, 29) = R(frame, 21);
    R(frame, 27) = R(frame, 21);
    const std::uint64_t new_counter =
        std::uint64_t{memory.ReadU32(kCounter)} + 1u;
    R(frame, 31) = R(frame, 21);
    memory.WriteU32(Low(sp + 80u), Low(R(frame, 29)));
    memory.WriteU32(Low(sp + 88u), Low(R(frame, 27)));
    memory.WriteU32(kCounter, Low(new_counter));
    const std::uint64_t head = memory.ReadU32(kQueueHead);
    (void)DrainInitialQueue(memory, manager_init, callbacks, sp, frame, head);
    R(frame, 25) = kGlobalBase;
    ProcessQueuedItems(memory, manager_init, callbacks, sp, frame);

    memory.WriteU32(Low(sp + 84u), Low(R(frame, 21)));
    if (Signed(Low(R(frame, 27))) != 0)
    {
        memory.WriteU32(Low(sp + 88u), Low(R(frame, 21)));
        if (Low(R(frame, 29)) != 0)
        {
            const GuestAddress manager = CurrentManager(memory, manager_init,
                sp, frame, 0x823FDC88u);
            const GuestAddress vtable = memory.ReadU32(manager);
            const GuestAddress method = memory.ReadU32(vtable + 8u) & ~3u;
            frame.lr = 0x823FDCA8u;
            const std::uint64_t resized = callbacks.ResizeStorage(method,
                memory, manager, R(frame, 29), 0, 8, sp, frame);
            memory.WriteU32(Low(sp + 80u), Low(resized));
        }
    }
    const std::uint32_t counter =
        memory.ReadU32(Low(R(frame, 22) + 24292u));
    memory.WriteU32(Low(R(frame, 22) + 24292u), counter - 1u);

    const std::uint64_t residual = CallRemoveArrayRange(memory, manager_init,
        callbacks, sp + 80u, sp, frame);
    R(frame, 31) = memory.ReadU32(Low(sp + 80u));
    std::uint64_t result = residual;
    if (Low(R(frame, 31)) != 0)
    {
        const GuestAddress manager = CurrentManager(memory, manager_init,
            sp, frame, 0x823FDCECu);
        const GuestAddress vtable = memory.ReadU32(manager);
        const GuestAddress method = memory.ReadU32(vtable + 12u) & ~3u;
        frame.lr = 0x823FDD04u;
        result = callbacks.ReleaseStorage(method, memory, manager,
            R(frame, 31), sp, frame);
    }
    RestoreHubFrame(memory, caller_sp, frame);
    return result;
}

std::uint64_t InitializePackage(GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t object, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress caller = Low(caller_sp);
    memory.WriteU32(caller - 8u, Low(frame.lr));
    WriteU64(memory, caller - 24u, R(frame, 30));
    WriteU64(memory, caller - 16u, R(frame, 31));
    memory.WriteU32(caller - 112u, caller);
    const std::uint64_t sp = caller_sp - 112u;
    R(frame, 31) = object;
    R(frame, 30) = 0;
    memory.WriteU32(Low(R(frame, 31)), 0x82003AC8u);
    for (GuestAddress offset : {112u, 116u, 120u, 128u, 132u, 136u})
        memory.WriteU32(Low(R(frame, 31)) + offset, Low(R(frame, 30)));

    const auto flags = ReadU64(memory, Low(R(frame, 31)) + 8u);
    if ((Low(flags) & 0x200u) == 0)
    {
        if (memory.ReadU32(Low(R(frame, 31)) + 72u) == 0 &&
            memory.ReadU32(Low(R(frame, 31)) + 40u) == 0)
        {
            memory.WriteU32(Low(R(frame, 31)) + 72u, 1);
            memory.WriteU32(kParentFlag, 1);
            frame.lr = 0x82407B54u;
            (void)ProcessStateQueue(memory, manager_init, callbacks,
                                    sp, frame);
        }
        memory.WriteU32(Low(R(frame, 31)) + 60u, Low(R(frame, 30)));
        memory.WriteU32(Low(R(frame, 31)) + 84u, Low(R(frame, 30)));
    }
    const std::uint64_t result = R(frame, 31);
    memory.WriteU32(Low(R(frame, 31)) + 152u, Low(R(frame, 30)));
    frame.lr = memory.ReadU32(caller - 8u);
    R(frame, 30) = ReadU64(memory, caller - 24u);
    R(frame, 31) = ReadU64(memory, caller - 16u);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x823FDAB0u:
        result = ProcessStateQueue(memory, manager_init, callbacks,
                                   caller_sp, frame);
        return true;
    case 0x82407AD8u:
        result = InitializePackage(memory, manager_init, callbacks,
                                   incoming_r3, caller_sp, frame);
        return true;
    case 0x8240A7C0u:
        result = Low(incoming_r3) == 0 ? incoming_r3 :
            InitializePackage(memory, manager_init, callbacks,
                              incoming_r3, caller_sp, frame);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::state_array_processing
