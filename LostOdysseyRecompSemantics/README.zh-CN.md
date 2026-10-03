# Lost Odyssey 语义恢复库

本目录是项目首个可读语义恢复代码库。它是独立的 C++20 static library，可以在不依赖私有游戏数据、生成的 PPC 代码、渲染器或游戏 SDK 的情况下用 CMake 配置；Windows C++ 工具链仍需要正常的编译器和 SDK 环境。当前源码已有四十九个可读实现，覆盖 query 创建、query-pool 分配／释放、allocator 分派、memory service、guest-memory fill、cache range、heap-core、thread-state、heap allocation helper、heap growth／range／segment、guest memory move/copy、dynamic-array、special/raw allocation、manager 构造／初始化／锁／storage，以及 manager allocation／resize 和 fallback resize。Guest memory 仍是显式的 32 位大端窗口，尚未恢复契约的 service boundary 保持不透明。

这是研究和行为对照用的代码面，不会替换生成的 runtime，也不会启用 runtime hook；当前不宣称完整恢复、兼容性、性能提升或跨平台验证。恢复记录将可读实现、差分检查、runtime 接入、场景验证和完整语义分别记录；当前完整恢复仍为 0。

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

库内还实现了将 query 创建、槽位初始化和释放串联起来的 PooledQueryServices 组合 adapter；它是库内组合面，不是 runtime 接入。manager lifecycle 组合对原始 PPC 的 InitializeManager、AllocateRawMemory、GetProcessHeap 及 primary／fallback constructor 通过 12 条有边界用例；manager-lock 组合再通过 12 条；manager-startup 组合对 10 个原始／恢复 body 通过 6 条，三个虚表调用均指向真实目标。manager-allocation 和 fallback-resize 组合各通过 12 条补充用例，不增加函数数量。6 个新函数各有独立、有边界的 receipt，包含大小类别边界用例。3 个 lock 函数各有 16、24、24 条用例，2 个 storage 函数各有 18 条。其 test-only adapter 会显式重放 ABI prologue 保存和 backchain；production 仍没有完整 PPCContext adapter。heap、CRT、native-critical-section 和更深 vtable 边界仍为 synthetic，generic ABI scratch 和 volatile context 不纳入。部分测试仍使用有边界的 stub 或合成组合，因此不代表内存已完整恢复。下一步先做 inventory 后的入口／边界审计，再处理更深 vtable、CRT／native kernel、unwind、ABI adapter 以及代表性 runtime 场景验证。后续按证据和热点扩展，不为所有地址生成占位实现。
本阶段当前源码和 oracle 集合各为 49 个。48 个普通 PPC 函数共 21,536 例；cache 单独记录 1,042 条轨迹（1,040 条完整、2 条有限前缀）。query-lifecycle 增加 74 例，补充 heap-lifecycle runner 增加 1 条序列，manager lifecycle 增加 12 例，manager-lock 组合增加 12 例，manager-startup 增加 6 例，manager-allocation 组合增加 12 例，fallback-resize 组合增加 12 例，共 21,665 条有边界对照例。runtime、scene 和 complete 仍为 0；MMIO、fault、concurrency、完整 void r3 和机器码 runtime 验证仍未覆盖，cache 结果也不代表硬件同步已验收。
memory-fill 对照还覆盖未注册的 PPCContext adapter，包括 store address、width、value 和 order。该 adapter 只属于对照 harness，不是 runtime 接入。函数名和字段名是根据 PPC 证据推断的工作名称，不是已恢复的原始 debug symbol。
fill adapter 不证明 fault 期间的中间 PPCContext 状态，也未在 optimized 或 live path 上测试。

从仓库根目录复现有边界的 oracle 对照：

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
~~~

生成下方汇总报告前，还需运行其余[语义对照脚本](../tools/ghidra/README.md)；receipt 列表需要全部四十九个分函数结果。

当前进度报告覆盖 62,627 个候选地址，其中 49 个可读函数、49 个有边界对照函数，runtime、scene 和 complete 均为 0。receipt 保存在仓库外，可用进度工具汇总：

扩展恢复集合前，先运行[只读 function inventory 审计](../tools/ghidra/README.md#deduplicate-before-expanding-recovery)。当前 snapshot 是 62,627 个 catalog/manifest union 地址：50,412 个 `shared_entry`、7 个 `ghidra_only_entry`、4 个 `generated_interior_candidate` 和 12,204 个 `generated_uncovered_entry`。这些分类不等于真实逻辑函数数量，也不证明入口等价；审计不会独立核验实际 XEX 或 image 来源链，也不会修改恢复记录。四个 interior candidate 都有外部直接调用和独立生成的函数体证据，不安全合并；12,204 个 uncovered entry 是未解决候选，不是重复记录。边界审计证据保存在 `out/function-inventory/boundary-review.json`。 精确函数体审计发现 61,597 种不同文本，48 个重复组覆盖 1,071 个入口，有 1,023 份重复实现候选。后续可按组复用实现，但需保留每个入口映射并独立对照；这些计数不代表逻辑函数总数。

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

有边界的对照结果只支持其记录范围，不代表 runtime 已接入、场景覆盖、完整语义或性能变化。copy/move 对照包含最终普通字节、r1-8 spill 以及完整 64 位 r3/count；其中 U64 用两个 U32 实现，因此不能证明原子性、fault 行为或 access-width 行为。array 和 free API 是 void 语义面，不暴露原始 residual r3；除 live locals 和 spill 外，generic ABI scratch 不纳入范围。array 与 special-allocation oracle 内的真实 copy/move 组合由各自有边界 receipt 覆盖。manager lifecycle receipt 覆盖低于 0xA0000 的全字节、nested stack 和 7 个高地址页、完整 r3、callback 参数／顺序及 before/after fingerprint，并覆盖 primary／fallback／null／global mutation／frame+80 global alias／CRT retry 路径；heap backend、CRT、kernel 和 virtual method 边界仍为 synthetic，真实 manager lifetime 和 concurrency 仍待验证。包含全部 49 个恢复函数的静态库已通过 Native Windows Release build，但尚无 runtime integration、scene validation 或 complete-semantics 结果，也未启用 hook。
库内 query-lifecycle fixture 另外通过 74 个用例，receipt 为 `semantic-query-lifecycle-tests/receipt-827B7408-lifecycle.json`。独立的 heap-lifecycle runner 另外记录 1 条多 body 的 Allocate→Free→Allocate 序列，receipt 为 `semantic-heap-lifecycle-tests/receipt.json`；manager lifecycle runner 另外通过 12 条用例，receipt 为 `semantic-manager-lifecycle-tests/receipt-manager-lifecycle.json`；manager-lock 组合另外通过 12 条用例，receipt 为 `semantic-manager-lock-tests/receipt-manager-lock-composed.json`；manager-startup runner 另外通过 6 条用例，receipt 为 `semantic-manager-startup-tests/receipt-manager-startup.json`；manager-allocation 组合另外通过 12 条，receipt 为 `semantic-manager-allocate-tests/receipt-manager-allocate-composed.json`；fallback-resize 组合另外通过 12 条，receipt 为 `semantic-fallback-resize-tests/receipt-fallback-resize-composed.json`。这些是补充组合证据，不放入四十九条分函数 receipt 列表。6 个新函数各有独立 receipt。startup runner 默认读取 `LostOdysseyRecompLib/private/image_disc1.bin`，也可用 `--image <path>`，分别固定 XEX/image hash 并直接检查 4 个 slots；10 个 body 的三个 virtual call 均为真实目标，但 image 解密／解包来源链未独立证明。heap、CRT、native-critical-section 为 synthetic，generic ABI scratch 和 volatile context 不纳入；尚未验证机器码 runtime、MMIO、fault 或 concurrency。48 个普通 PPC 函数共 21,536 例，加上 74 个 query-lifecycle 用例、1 条 heap-lifecycle 序列、12 个 manager-lifecycle 用例、12 个 manager-lock 用例、6 个 manager-startup 用例、12 个 manager-allocation 用例和 12 个 fallback-resize 用例，共 21,665 例；cache 的 1,042 条轨迹单独记录（1,040 条完整、2 条有限前缀）。

详见[恢复计划](RECOVERY_PLAN.md)和 [English README](README.md)。recovery.json 是机器可读的状态记录。恢复期间保留原始 Ghidra 工程、生成的 PPC 源码和现有 runtime 实现。
