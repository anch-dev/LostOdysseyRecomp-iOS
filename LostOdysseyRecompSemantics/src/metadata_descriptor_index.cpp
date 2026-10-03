#include "lo_semantics/metadata_descriptor_index.h"

#include "lo_semantics/registered_metadata_words.h"

#include <bit>

namespace lo::semantic::gpu::metadata_descriptor_index
{
namespace
{
constexpr GuestAddress kRebuild = 0x82523c48u;
constexpr GuestAddress kAppend = 0x8256b910u;
constexpr GuestAddress kInsert = 0x826bd860u;
constexpr GuestAddress kInitialize = 0x82408d28u;
constexpr GuestAddress kFeatureGlobal = 0x8330b62cu;

enum class FrameKind { Single31, Helper28, Helper29, Double3031 };

std::int32_t Signed(std::uint32_t word)
{
    return std::bit_cast<std::int32_t>(word);
}

void Write64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void EnterFrame(GuestMemory& memory, std::uint64_t caller_sp,
    std::uint32_t size, FrameKind kind, const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (kind == FrameKind::Single31 || kind == FrameKind::Double3031)
        memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    if (kind == FrameKind::Helper28)
        Write64(memory, sp - 40u, frame.r28);
    if (kind == FrameKind::Helper28 || kind == FrameKind::Helper29)
        Write64(memory, sp - 32u, frame.r29);
    if (kind != FrameKind::Single31)
        Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    if (kind == FrameKind::Helper28 || kind == FrameKind::Helper29)
        memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - size, sp);
}

void LeaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameKind kind, FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (kind == FrameKind::Single31 || kind == FrameKind::Double3031)
        frame.lr = memory.ReadU32(sp - 8u);
    if (kind == FrameKind::Helper28)
        frame.r28 = Read64(memory, sp - 40u);
    if (kind == FrameKind::Helper28 || kind == FrameKind::Helper29)
        frame.r29 = Read64(memory, sp - 32u);
    if (kind != FrameKind::Single31)
        frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    if (kind == FrameKind::Helper28 || kind == FrameKind::Helper29)
        frame.lr = memory.ReadU32(sp - 8u);
}

class ArrayAdapter final : public ArrayResizeServices
{
public:
    ArrayAdapter(GuestMemory& memory, ManagerFacadeServices& services,
        GuestAddress init_frame)
        : memory_(memory), services_(services), init_frame_(init_frame) {}

    void InitializeManager() override
    { (void)lo::semantic::gpu::InitializeManager(
        memory_, services_, init_frame_); }

    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    { return services_.ResizeStorage(method, manager, old_storage,
                                    bytes, argument); }

private:
    GuestMemory& memory_;
    ManagerFacadeServices& services_;
    GuestAddress init_frame_;
};

void Rebuild(GuestMemory& memory, ManagerFacadeServices& services,
    std::uint64_t table_register, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 96u, FrameKind::Single31, frame);
    const std::uint64_t sp = caller_sp - 96u;
    frame.r31 = table_register;
    const GuestAddress table = static_cast<GuestAddress>(frame.r31);
    const std::uint64_t old_storage = memory.ReadU32(table + 12u);
    frame.lr = 0x82523c64u;
    (void)ReleaseManagerBuffer(memory, services, old_storage,
        static_cast<GuestAddress>(sp));

    const std::uint32_t bucket_count = memory.ReadU32(table + 16u);
    const std::uint64_t bytes = bucket_count > 0x3fffffffu ?
        ~std::uint64_t{0} : std::uint64_t{bucket_count << 2u};
    frame.lr = 0x82523c84u;
    result = AllocateManagerBuffer(memory, services, bytes,
        static_cast<GuestAddress>(sp));
    const std::uint32_t live_bucket_count = memory.ReadU32(table + 16u);
    memory.WriteU32(table + 12u, static_cast<std::uint32_t>(result));
    if (Signed(live_bucket_count) > 0)
    {
        std::uint32_t index = 0;
        std::uint32_t offset = 0;
        do
        {
            const GuestAddress storage = memory.ReadU32(table + 12u);
            memory.WriteU32(storage + offset, 0xffffffffu);
            ++index;
            offset += 4u;
        } while (Signed(index) < Signed(memory.ReadU32(table + 16u)));
    }

    if (Signed(memory.ReadU32(table + 4u)) > 0)
    {
        std::uint32_t index = 0;
        std::uint32_t offset = 0;
        do
        {
            const GuestAddress entries = memory.ReadU32(table);
            const GuestAddress slot = entries + offset;
            const GuestAddress key = memory.ReadU32(slot + 4u);
            const std::uint32_t key_mask = key != 0 ?
                memory.ReadU32(key + 4u) : 0u;
            const std::uint32_t current_bucket_count =
                memory.ReadU32(table + 16u);
            const std::uint32_t bucket_offset =
                ((current_bucket_count - 1u) & key_mask) << 2u;
            const GuestAddress buckets = memory.ReadU32(table + 12u);
            const std::uint32_t head =
                memory.ReadU32(buckets + bucket_offset);
            memory.WriteU32(slot, head);
            const GuestAddress live_buckets = memory.ReadU32(table + 12u);
            memory.WriteU32(live_buckets + bucket_offset, index);
            ++index;
            offset += 12u;
        } while (Signed(index) < Signed(memory.ReadU32(table + 4u)));
    }
    LeaveFrame(memory, caller_sp, FrameKind::Single31, frame);
}

void Append(GuestMemory& memory, ManagerFacadeServices& services,
    std::uint64_t table_register, std::uint64_t key_register,
    std::uint64_t value_register, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 128u, FrameKind::Helper28, frame);
    const std::uint64_t sp = caller_sp - 128u;
    frame.r29 = key_register;
    frame.r28 = value_register;
    frame.r31 = table_register;
    const GuestAddress table = static_cast<GuestAddress>(frame.r31);
    ArrayAdapter adapter(memory, services,
        static_cast<GuestAddress>(sp - 96u - 128u - 112u));
    frame.lr = 0x8256b938u;
    const std::uint64_t old_index =
        registered_metadata_words::AddArrayElements(memory, adapter,
            table, 1u, 12u, 8u);
    const std::uint32_t slot_offset =
        static_cast<std::uint32_t>(old_index * 3u) << 2u;
    const std::uint64_t candidate =
        std::uint64_t{memory.ReadU32(table)} + slot_offset;
    frame.r30 = static_cast<GuestAddress>(candidate) == 0 ? 0u : candidate;
    if (static_cast<GuestAddress>(candidate) != 0)
    {
        memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 4u,
            static_cast<std::uint32_t>(frame.r29));
        memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 8u,
            static_cast<std::uint32_t>(frame.r28));
    }

    const GuestAddress key = memory.ReadU32(
        static_cast<GuestAddress>(frame.r30) + 4u);
    const std::uint32_t key_mask = key != 0 ?
        memory.ReadU32(key + 4u) : 0u;
    const std::uint32_t bucket_count = memory.ReadU32(table + 16u);
    const GuestAddress buckets = memory.ReadU32(table + 12u);
    const std::uint32_t bucket_offset =
        ((bucket_count - 1u) & key_mask) << 2u;
    const std::uint32_t head = memory.ReadU32(buckets + bucket_offset);
    memory.WriteU32(static_cast<GuestAddress>(frame.r30), head);
    const std::uint32_t count = memory.ReadU32(table + 4u);
    const GuestAddress live_buckets = memory.ReadU32(table + 12u);
    memory.WriteU32(live_buckets + bucket_offset, count - 1u);

    const std::uint32_t live_bucket_count = memory.ReadU32(table + 16u);
    const std::uint32_t live_count = memory.ReadU32(table + 4u);
    const std::uint32_t threshold = (live_bucket_count + 4u) << 1u;
    if (Signed(threshold) < Signed(live_count))
    {
        memory.WriteU32(table + 16u, live_bucket_count << 1u);
        frame.lr = 0x8256b9d4u;
        std::uint64_t ignored = 0;
        Rebuild(memory, services, frame.r31, sp, frame, ignored);
    }
    result = frame.r30 + 8u;
    LeaveFrame(memory, caller_sp, FrameKind::Helper28, frame);
}

void Insert(GuestMemory& memory, ManagerFacadeServices& services,
    std::uint64_t table_register, std::uint64_t key_register,
    std::uint64_t value_register, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 112u, FrameKind::Helper29, frame);
    const std::uint64_t sp = caller_sp - 112u;
    frame.r31 = table_register;
    frame.r30 = key_register;
    frame.r29 = value_register;
    GuestAddress table = static_cast<GuestAddress>(frame.r31);
    if (memory.ReadU32(table + 12u) == 0)
    {
        frame.lr = 0x826bd888u;
        std::uint64_t ignored = 0;
        Rebuild(memory, services, frame.r31, sp, frame, ignored);
        // Rebuild restores r31 through its guest save slot. A callback may
        // redirect it before this caller resumes its live table accesses.
        table = static_cast<GuestAddress>(frame.r31);
    }

    if (Signed(memory.ReadU32(table + 4u)) > 0)
    {
        const GuestAddress key = static_cast<GuestAddress>(frame.r30);
        const std::uint32_t key_mask = key != 0 ?
            memory.ReadU32(key + 4u) : 0u;
        const std::uint32_t bucket_count = memory.ReadU32(table + 16u);
        const GuestAddress buckets = memory.ReadU32(table + 12u);
        const std::uint32_t bucket_offset =
            ((bucket_count - 1u) & key_mask) << 2u;
        std::uint32_t index = memory.ReadU32(buckets + bucket_offset);
        if (Signed(index) != -1)
        {
            const GuestAddress entries = memory.ReadU32(table);
            do
            {
                const GuestAddress slot = entries + index * 12u;
                if (memory.ReadU32(slot + 4u) == key)
                {
                    memory.WriteU32(slot + 8u,
                        static_cast<std::uint32_t>(frame.r29));
                    const GuestAddress live_entries = memory.ReadU32(table);
                    result = std::uint64_t{live_entries} + index * 12u + 8u;
                    LeaveFrame(memory, caller_sp, FrameKind::Helper29,
                        frame);
                    return;
                }
                index = memory.ReadU32(slot);
            } while (Signed(index) != -1);
        }
    }
    frame.lr = 0x826bd904u;
    Append(memory, services, frame.r31, frame.r30, frame.r29,
        sp, frame, result);
    LeaveFrame(memory, caller_sp, FrameKind::Helper29, frame);
}

void InitializeDescriptor(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t destination_register,
    std::uint64_t owner_register, std::uint64_t key_register,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 112u, FrameKind::Double3031, frame);
    const std::uint64_t sp = caller_sp - 112u;
    frame.r31 = owner_register;
    frame.r30 = destination_register;
    GuestAddress destination = static_cast<GuestAddress>(frame.r30);
    GuestAddress owner = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(destination + 4u, static_cast<std::uint32_t>(frame.r31));
    std::uint64_t key = key_register;
    if (static_cast<GuestAddress>(key) == 0)
        key = memory.ReadU32(owner + 56u);
    memory.WriteU32(destination, static_cast<std::uint32_t>(key));
    frame.lr = 0x82408d68u;
    Insert(memory, services, frame.r30 + 32u, key, frame.r31,
        sp, frame, result);

    // Insert also restores its nonvolatile registers from guest save slots.
    destination = static_cast<GuestAddress>(frame.r30);
    owner = static_cast<GuestAddress>(frame.r31);
    const bool enabled = (Read64(memory, owner + 8u) & 0x400u) != 0;
    memory.WriteU32(destination + 12u, enabled ? 1u : 0u);
    const bool feature = enabled &&
        (memory.ReadU32(kFeatureGlobal) & 4u) != 0;
    memory.WriteU32(destination + 16u, feature ? 1u : 0u);
    LeaveFrame(memory, caller_sp, FrameKind::Double3031, frame);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    switch (address)
    {
    case kRebuild:
        (void)incoming_r4;
        (void)incoming_r5;
        Rebuild(memory, services, incoming_r3, caller_sp, frame, result);
        return true;
    case kAppend:
        Append(memory, services, incoming_r3, incoming_r4,
            incoming_r5, caller_sp, frame, result);
        return true;
    case kInsert:
        Insert(memory, services, incoming_r3, incoming_r4,
            incoming_r5, caller_sp, frame, result);
        return true;
    case kInitialize:
        InitializeDescriptor(memory, services, incoming_r3, incoming_r4,
            incoming_r5, caller_sp, frame, result);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::metadata_descriptor_index
