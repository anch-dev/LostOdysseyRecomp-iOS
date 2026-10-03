# Lost Odyssey 语义恢复库

本目录是项目首个可读语义恢复代码库。它是独立的 C++20 static library，可以在不依赖私有游戏数据、生成的 PPC 代码、渲染器或游戏 SDK 的情况下用 CMake 配置；Windows C++ 工具链仍需要正常的编译器和 SDK 环境。当前源码已有六十八个逐个记录的可读条目，覆盖 query 创建、query-pool 分配／释放、allocator 分派、memory service、guest-memory fill、cache range、heap-core、thread-state、heap allocation helper、heap growth／range／segment、guest memory move/copy、dynamic-array、special/raw allocation、manager 构造／初始化／锁／storage、manager allocation／resize、fallback resize、pointer-vector 清理、manager buffer/array facade、allocation-failure reporting 以及 CRT service。Guest memory 仍是显式的 32 位大端窗口，尚未恢复契约的 service boundary 保持不透明。

这是研究和行为对照用的代码面，runtime 替换默认关闭；当前不宣称完整恢复、兼容性、性能提升或跨平台验证。恢复记录将可读实现、差分检查、runtime 接入、场景验证和完整语义分别记录；当前完整恢复仍为 0。

另有一个独立的 leaf-family 批次，将 942 个原始 entrypoint 映射到两个共享的可读 C++ 实现（`PreserveR3` 和 `ReturnOne`），使用 3 个精确源码模板。一次 native 编译及 4,710/4,710 条完整 PPCContext 加 4,096 字节普通内存对照均通过。共享批处理流程去除了逐函数编译开销，但不宣称整体加速；逐个跟踪的实现仍单独计数。入口映射见 [leaf_families.json](leaf_families.json)。可用 `python -B tools/ghidra/test_semantic_leaf_family.py --write-map --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-leaf-family-tests"` 重现。运行需要已有的 `out/function-inventory/exact-body-families.json` 清单和私有输入；完整 PPC 位于其他 checkout 时，用 `--ppc-root <目录>` 指定。该证据仅限有边界的行为对照，不会启用 runtime 替换，也不代表游戏验收通过。

11 份 family 清单分别包含 942 个 leaf、304 个 accessor、296 个 integer、153 个 field-bit、277 个 pointer-field、104 个 global-assignment、134 个 field-operation、89 个 field-arithmetic、39 个 read-only-field、71 个 memory-write 和 46 个 single-write-field 地址。与 `recovery.json` 中的 68 个逐项记录合并，并扣除 `829664E8`（individual/integer）与 `82B84D88`（individual/global-assignment）两个重叠后，共覆盖 2,521 个唯一地址。这是入口地址映射数量，不代表独立逻辑函数数量或完整恢复。

最新批次新增 43 个 multi-write 和 46 个 single-write-field 入口，分别通过 258 和 276 条有边界原始 PPC 对照；object wrapper 通过 2,060 条对照，5 个 manager buffer/array facade 另通过 12 条组合对照。此前 39 个 read-only-field 入口及其 234 条对照继续有效，本轮未重复运行。此前 family 和 CRT 检查继续在其记录范围内有效，本轮未重复运行。CRT service boundary、ABI 保存字和 volatile 状态仍未完整覆盖。只在对应 family 行为改变时运行聚焦对照，并复用现有输出目录：

~~~powershell
python -B tools/ghidra/test_semantic_accessor_family.py --write-map --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-accessor-family-tests"
python -B tools/ghidra/test_semantic_integer_leaf.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-integer-leaf-tests"
python -B tools/ghidra/test_semantic_pointer_vector.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-pointer-vector-tests"
python -B tools/ghidra/test_semantic_field_bits.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-field-bits-tests"
python -B tools/ghidra/test_semantic_pointer_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-pointer-fields-tests"
python -B tools/ghidra/test_semantic_allocation_failure.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-allocation-failure-tests"
python -B tools/ghidra/test_semantic_read_only_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-read-only-field-tests"
python -B tools/ghidra/test_semantic_memory_writes.py --new-only --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-memory-write-tests"
python -B tools/ghidra/test_semantic_single_write_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-single-write-tests"
python -B tools/ghidra/test_semantic_crt_lifecycle.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-crt-lifecycle-tests"
~~~

新增的 `827C4FA0` pointer-vector 恢复会释放缓存指针并将 count 清零；原先的 GrowPointerVector 边界名称并不准确。10 条边界对照已通过 return、memory 和调用顺序检查。5 个 allocation-failure helper 为 `ReportRuntimeError`（`82B7FC98`）、`ReportMissingHeapBanner`（`82B7FCE0`）、`TerminateAllocationFailure`（`82B7BF20`）、`InvokeNewHandler`（`82B7FE68`）和 `GetAllocationErrorAddress`（`82B7FD78`）；其 18 条独立边界对照和 5 条 raw-allocation 组合均通过。ABI 保存、volatile 状态和真实 kernel 仍未纳入。此前 49 函数 hash receipt 的命令属于历史检查点，共享 header 改动后不应视为当前全源码新鲜度报告。

实验性的 runtime adapter 在 `LO_ENABLE_SEMANTIC_RUNTIME` 下提供 2,299 个 wrapper：`LO_SEMANTIC_LEAF_RUNTIME=1` 选择 942 个 leaf wrapper，`LO_SEMANTIC_INTEGER_RUNTIME=1` 选择 296 个 integer wrapper，`LO_SEMANTIC_ACCESSOR_RUNTIME=1` 选择 304 个 accessor wrapper，`LO_SEMANTIC_FIELDS_RUNTIME=1` 选择 242 个 field-bit/field-arithmetic wrapper，`LO_SEMANTIC_OBJECT_RUNTIME=1` 选择 515 个 pointer-field/global-assignment/field-operation wrapper。五个 gate 默认均使用原始 fallback。Object wrapper 批次通过 2,060 条原始 PPC context/memory 对照；旧 wrapper 检查直接复用。read-only-field、memory-write、single-write-field、manager-facade 和 CRT API 尚未替换 runtime。

Windows RelWithDebInfo runtime 在加入 object wrapper 后通过增量编译与链接，并复用了本地 PPC archive。read-only-field、memory-write、single-write-field、CRT lifecycle 和包含 5 个 facade 的 manager_facade 模块均已通过语义库编译与链接；它们尚未替换 runtime，且 manager facade 不在五 gate scene smoke 范围内。一次隔离的五 gate Windows D3D12 Uhra smoke 在 1280x720、30 fps 下到达 Map20 并打开菜单，未观察到崩溃或明显结构回归；见 [object runtime checkpoint](runtime_object_checkpoint.json)。可设置 `LO_PREBUILT_RECOMP_LIBRARY` 复用本地 PPC archive，避免重复编译 246 个 PPC 分块；`LO_PREBUILT_RECOMP_INCLUDE_DIR` 可覆盖 include 路径，默认使用当前 PPC 目录。library 变量为空时仍使用源码编译。

此前 Windows D3D12 原始路径／恢复路径的 Uhra Residential Area（Map20）及主菜单 smoke 继续作为对照基线，见 [runtime_checkpoint.json](runtime_checkpoint.json)。前四个 gate 的隔离运行见 [runtime_field_checkpoint.json](runtime_field_checkpoint.json)，本轮五 gate 的隔离运行见 [runtime_object_checkpoint.json](runtime_object_checkpoint.json)。这些都是有边界的 smoke 证据，不代表像素一致性、性能或逐入口验收；2,299 个 wrapper 未逐一追踪。

## 构建

在 Windows cmd 中先调用仓库的设置脚本，再把独立目录配置和构建到 ownCloud checkout 之外：

~~~cmd
call tools\setup_windows.bat
cmake -S LostOdysseyRecompSemantics -B "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-isolated-library" -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-isolated-library"
~~~

也可以在 Visual Studio Developer Terminal 中执行相同的 CMake 命令。设置脚本只选择现有本地工具，不安装依赖。根项目可通过 LO_BUILD_SEMANTIC_LIBRARY 暴露可选目标，默认值为 OFF。

## 恢复范围

GuestMemory 表示有边界的 Xbox guest 地址窗口，并显式读写大端字。QueryServices 在外部调用约定恢复前保持不透明。CreateType9Query 记录已观察到的分配大小和 flags、写入顺序、slot 循环、失败清理和返回行为。原始生成 PPC 与反编译结果仍是证据来源；可读 C++ 本身不等于语义正确。

第一阶段的顺序是：

1. 确认 XEX 与生成源码身份；
2. 恢复类型、ABI 和控制流；
3. 恢复副作用和外部依赖；
4. 通过 adapter 和有边界的 guest-memory 模型做行为对照；
5. 在代表性游戏场景中验证；
6. 前述证据完成后，才逐个替换 runtime 边界。

当前可读切片包括：


 - 827B7408 CreateType9Query
 - 827B72E8 InitializeQuerySlot
 - 823CDCA8 ReleaseQuery
 - 827C9D88 AllocateDispatch
 - 827C9DB0 FreeDispatch
 - 823EA178 FlushDataCacheRange
 - 827CA050 AllocateGeneral
 - 827CA0E8 FreeGeneral
 - 827C9E20 AllocatePhysicalMemory
 - 827C9EB8 FreePhysicalMemory
 - 827CAD38 AllocateHeapMemory
 - 827CAD80 FreeHeapMemory
 - 823ACC98 GetProcessHeap
 - 82B7BC40 FillGuestMemory
 - 827CBA60 InsertFreeBlocks
 - 823AE108 CoalesceFreeBlocks
 - 823ADE28 FreeHeapBlock
 - 823AE0BC LeaveHeapCriticalSection
 - 822CA180 ReportAllocationFailure
 - 822CA188 StoreThreadFailureCode
 - 823ACCB0 AllocateHeapBlock
 - 823AD544 LeaveHeapAllocateCriticalSection
 - 827CC428 GrowHeap
 - 827CC668 DecommitFreeBlock
 - 827CB498 AllocateRangeNode
 - 827CB658 InsertRangeRecord
 - 827CB778 ExtendHeapSegment
 - 827CC2C0 InitializeHeapSegment
 - 82B7A0B0 CopyGuestMemory
 - 82B7C470 MoveGuestMemory
 - 8229F678 ResizeArray
 - 82298AF8 RemoveArrayRange
 - 827C9A40 AllocateSpecialBlock
 - 827C9C60 FreeSpecialBlock
 - 823ACBD0 AllocateRawMemory
 - 827C5F38 InitializeManager
 - 827C5970 ConstructPrimaryManager
 - 827C4ED0 ConstructFallbackManager
 - 829664E8 ReturnZeroStatus
 - 822958F8 StoreLockAndWaitForEnter
 - 827C5688 InvokeManagerUnderLock
 - 827C5B30 InitializeStorageBuckets
 - 827C5D88 InitializePrimaryManagerStorage
 - 823F2670 AllocateThroughPrimaryManager
 - 823F25E0 AllocateThroughFallbackManager
 - 82295950 ResizePrimaryManagerStorage
 - 822A0738 FindPrimaryResizeNode
 - 82295530 ResizeFallbackAllocation
- 827C5050 AppendPointerToVector
- 827C4FA0 FlushPointerVector
- 823F3340 ReleaseManagerBuffer
- 82486C88 AllocateManagerBuffer
- 823F3548 ClearBufferHeader
- 82298A98 ReleaseTwoByteArray
- 82298938 ResetTwoByteArray

库内还实现了将 query 创建、槽位初始化和释放串联起来的 PooledQueryServices 组合 adapter；它是库内组合面，不是 runtime 接入。manager lifecycle 组合对原始 PPC 的 InitializeManager、AllocateRawMemory、GetProcessHeap 及 primary／fallback constructor 通过 12 条有边界用例；manager-lock 组合再通过 12 条；manager-startup 组合对 10 个原始／恢复 body 通过 6 条，三个虚表调用均指向真实目标。manager-allocation 和 fallback-resize 组合各通过 12 条补充用例，不增加函数数量。5 个 manager buffer/array facade 另通过 12 条有边界原始 PPC 组合对照；下层 manager 和 array callee 仍是 service stand-in，尚无 runtime/scene 验收。现有逐项新增函数均有独立、有边界的 receipt，包含大小类别边界用例。3 个 lock 函数各有 16、24、24 条用例，2 个 storage 函数各有 18 条。其 test-only adapter 会显式重放 ABI prologue 保存和 backchain；production 仍没有完整 PPCContext adapter。heap、CRT、native-critical-section 和更深 vtable 边界仍为 synthetic，generic ABI scratch 和 volatile context 不纳入。部分测试仍使用有边界的 stub 或合成组合，因此不代表内存已完整恢复。当前工作继续推进 family 与 dependency 恢复，并同步构建 runtime adapter；更深 vtable、CRT／native kernel、unwind、ABI adapter 以及代表性 runtime 场景验证仍待完成。后续按证据和热点扩展，不为所有地址生成占位实现。
当前 `recovery.json` 的 68 个逐项记录与 11 份 family 清单在扣除上述两个重叠后，共覆盖 2,521 个唯一映射地址；逐入口 runtime 和 scene 验收仍未设置，object gate 仅有 Map20／主菜单 smoke，5 个 manager facade 尚无 runtime/scene 验收，complete 仍为 0。此前 49 函数报告和 receipt 保留为历史检查点；MMIO、fault、concurrency、完整 void r3 和机器码 runtime 验证仍未覆盖，cache 结果也不代表硬件同步已验收。
memory-fill 对照还覆盖未注册的 PPCContext adapter，包括 store address、width、value 和 order。该 adapter 只属于对照 harness，不是 runtime 接入。函数名和字段名是根据 PPC 证据推断的工作名称，不是已恢复的原始 debug symbol。
fill adapter 不证明 fault 期间的中间 PPCContext 状态，也未在 optimized 或 live path 上测试。

从仓库根目录复现有边界的 oracle 对照：

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
~~~

生成下方汇总报告前，还需运行其余[语义对照脚本](../tools/ghidra/README.md)；receipt 列表需要当前分函数结果，CRT 两个 service 的 9 条对照另行记录。

此前的进度报告覆盖 62,627 个候选地址，其中 49 个可读函数、49 个有边界对照函数；它保留为历史检查点，receipt 保存在仓库外。当前 family 与 dependency 恢复及 runtime adapter 工作见上文：runtime、scene 和 complete 仍为 0。

复用此前[只读 function inventory 审计](../tools/ghidra/README.md#deduplicate-before-expanding-recovery)的缓存；底层源码不变时，扩展恢复集合无需重复全源码扫描或 hash。当前 snapshot 是 62,627 个 catalog/manifest union 地址：50,412 个 `shared_entry`、7 个 `ghidra_only_entry`、4 个 `generated_interior_candidate` 和 12,204 个 `generated_uncovered_entry`。这些分类不等于真实逻辑函数数量，也不证明入口等价；审计不会独立核验实际 XEX 或 image 来源链，也不会修改恢复记录。四个 interior candidate 都有外部直接调用和独立生成的函数体证据，不安全合并；12,204 个 uncovered entry 是未解决候选，不是重复记录。边界审计证据保存在 `out/function-inventory/boundary-review.json`。 精确函数体审计发现 61,597 种不同文本，48 个重复组覆盖 1,071 个入口，有 1,023 份重复实现候选。后续可按组复用实现，但需保留每个入口映射并独立对照；这些计数不代表逻辑函数总数。

~~~powershell
$evidenceRoot = Join-Path $env:USERPROFILE 'worktrees/LostOdysseyRecomp'
$receiptRel = @(
  'semantic-recovery-tests\receipt.json',
  'semantic-recovery-pool-tests\receipt-823CDCA8.json',
  'semantic-recovery-pool-tests\receipt-827B72E8.json',
  'semantic-recovery-pool-tests\receipt-827C9D88.json',
  'semantic-recovery-pool-tests\receipt-827C9DB0.json',
  'semantic-cache-tests\receipt.json',
  'semantic-allocation-backend-tests\receipt-827CA050.json',
  'semantic-allocation-backend-tests\receipt-827CA0E8.json',
  'semantic-memory-services-tests\receipt-823ACC98.json',
  'semantic-memory-services-tests\receipt-827C9E20.json',
  'semantic-memory-services-tests\receipt-827C9EB8.json',
  'semantic-memory-services-tests\receipt-827CAD38.json',
  'semantic-memory-services-tests\receipt-827CAD80.json',
  'semantic-memory-fill-tests\receipt.json',
  'semantic-heap-tests\receipt-823AE108.json',
  'semantic-heap-tests\receipt-827CBA60.json',
  'semantic-heap-free-tests\receipt-823ADE28.json',
  'semantic-heap-free-tests\receipt-823AE0BC.json',
  'semantic-thread-state-tests\receipt-822CA180.json',
  'semantic-thread-state-tests\receipt-822CA188.json',
  'semantic-heap-allocate-tests\receipt-823ACCB0.json',
  'semantic-heap-allocate-tests\receipt-823AD544.json',
  'semantic-heap-growth-tests\receipt-827CC428.json',
  'semantic-heap-decommit-tests\receipt-827CC668.json',
  'semantic-heap-ranges-tests\receipt-827CB498.json',
  'semantic-heap-ranges-tests\receipt-827CB658.json',
  'semantic-heap-segment-tests\receipt-827CB778.json',
  'semantic-heap-segment-tests\receipt-827CC2C0.json',
  'semantic-memory-move-tests\receipt-82B7A0B0.json',
  'semantic-memory-move-tests\receipt-82B7C470.json',
  'semantic-allocation-array-tests\receipt-8229F678.json',
  'semantic-allocation-array-tests\receipt-82298AF8.json',
  'semantic-special-allocation-tests\receipt-827C9A40.json',
  'semantic-special-allocation-tests\receipt-827C9C60.json',
  'semantic-raw-allocation-tests\receipt-823ACBD0.json',
  'semantic-manager-init-tests\receipt-827C5F38.json',
  'semantic-manager-construction-tests\receipt-827C5970.json',
  'semantic-manager-construction-tests\receipt-827C4ED0.json',
  'semantic-manager-lock-tests\receipt-829664E8.json',
  'semantic-manager-lock-tests\receipt-822958F8.json',
  'semantic-manager-lock-tests\receipt-827C5688.json',
  'semantic-manager-storage-tests\receipt-827C5B30.json',
  'semantic-manager-storage-tests\receipt-827C5D88.json',
  'semantic-manager-allocate-tests\receipt-823F2670.json',
  'semantic-manager-allocate-tests\receipt-823F25E0.json',
  'semantic-manager-resize-tests\receipt-82295950.json',
  'semantic-manager-resize-tests\receipt-822A0738.json',
  'semantic-fallback-resize-tests\receipt-82295530.json',
  'semantic-fallback-resize-tests\receipt-827C5050.json'
)
$receiptArgs = foreach ($relative in $receiptRel) { '--receipt'; Join-Path $evidenceRoot $relative }
python tools/ghidra/semantic_progress.py `
  --catalog out/decomp-index/catalog.sqlite `
  --manifest LostOdysseyRecompSemantics/recovery.json `
  @receiptArgs `
  --output out/semantic-recovery
~~~

有边界的对照结果只支持其记录范围，不代表 runtime 已接入、场景覆盖、完整语义或性能变化。copy/move 对照包含最终普通字节、r1-8 spill 以及完整 64 位 r3/count；其中 U64 用两个 U32 实现，因此不能证明原子性、fault 行为或 access-width 行为。array 和 free API 是 void 语义面，不暴露原始 residual r3；除 live locals 和 spill 外，generic ABI scratch 不纳入范围。array 与 special-allocation oracle 内的真实 copy/move 组合由各自有边界 receipt 覆盖。manager lifecycle receipt 覆盖低于 0xA0000 的全字节、nested stack 和 7 个高地址页、完整 r3、callback 参数／顺序及 before/after fingerprint，并覆盖 primary／fallback／null／global mutation／frame+80 global alias／CRT retry 路径；heap backend、CRT、kernel 和 virtual method 边界仍为 synthetic，真实 manager lifetime 和 concurrency 仍待验证。包含全部 49 个恢复函数的静态库已通过 Native Windows Release build；bounded Uhra/menu smoke 已通过，后续五 gate object smoke 也在 Map20／主菜单范围内通过。逐入口 runtime integration 与 scene acceptance 仍未设置，complete semantics 仍未验证，runtime replacement 默认关闭。详见 [runtime checkpoint](runtime_checkpoint.json)、[field runtime checkpoint](runtime_field_checkpoint.json) 和 [object runtime checkpoint](runtime_object_checkpoint.json)。
库内 query-lifecycle fixture 另外通过 74 个用例，receipt 为 `semantic-query-lifecycle-tests/receipt-827B7408-lifecycle.json`。独立的 heap-lifecycle runner 另外记录 1 条多 body 的 Allocate→Free→Allocate 序列，receipt 为 `semantic-heap-lifecycle-tests/receipt.json`；manager lifecycle runner 另外通过 12 条用例，receipt 为 `semantic-manager-lifecycle-tests/receipt-manager-lifecycle.json`；manager-lock 组合另外通过 12 条用例，receipt 为 `semantic-manager-lock-tests/receipt-manager-lock-composed.json`；manager-startup runner 另外通过 6 条用例，receipt 为 `semantic-manager-startup-tests/receipt-manager-startup.json`；manager-allocation 组合另外通过 12 条，receipt 为 `semantic-manager-allocate-tests/receipt-manager-allocate-composed.json`；fallback-resize 组合另外通过 12 条，receipt 为 `semantic-fallback-resize-tests/receipt-fallback-resize-composed.json`。这些是补充组合证据，不放入当前分函数 receipt 列表。CRT 两个 service 另有 9 条独立边界对照。startup runner 默认读取 `LostOdysseyRecompLib/private/image_disc1.bin`，也可用 `--image <path>`，分别固定 XEX/image hash 并直接检查 4 个 slots；10 个 body 的三个 virtual call 均为真实目标，但 image 解密／解包来源链未独立证明。heap、CRT、native-critical-section 为 synthetic，generic ABI scratch 和 volatile context 不纳入；尚未验证机器码 runtime、MMIO、fault 或 concurrency。此前四十八个普通 PPC 函数共 21,536 条用例，历史组合累计 21,665 条；当前 CRT service 增加 9 条独立边界对照，global-assignment 和 field-operation family 分别增加 416 和 536 条完整 PPCContext 与普通内存对照；cache 的 1,042 条轨迹单独记录（1,040 条完整、2 条有限前缀）。

详见[恢复计划](RECOVERY_PLAN.md)和 [English README](README.md)。recovery.json 是机器可读的状态记录。恢复期间保留原始 Ghidra 工程、生成的 PPC 源码和现有 runtime 实现。
