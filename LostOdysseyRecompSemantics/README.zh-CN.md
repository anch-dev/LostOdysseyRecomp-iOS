# Lost Odyssey 语义恢复库

本目录是项目首个可读语义恢复代码库。它是独立的 C++20 static library，可以在不依赖私有游戏数据、生成的 PPC 代码、渲染器或游戏 SDK 的情况下用 CMake 配置；Windows C++ 工具链仍需要正常的编译器和 SDK 环境。当前源码已有二十八个可读实现，覆盖 query 创建、query-pool 分配／释放、allocator 分派、memory service、guest-memory fill、cache range、heap-core、thread-state、heap allocation helper 以及 heap growth／range／segment 操作。Guest memory 仍是显式的 32 位大端窗口，尚未恢复契约的 service boundary 保持不透明。

这是研究和行为对照用的代码面，不会替换生成的 runtime，也不会启用 runtime hook；当前不宣称完整恢复、兼容性、性能提升或跨平台验证。恢复记录将可读实现、差分检查、runtime 接入、场景验证和完整语义分别记录；当前完整恢复仍为 0。

## 构建

在 Windows cmd 中先调用仓库的设置脚本，再把独立目录配置和构建到 ownCloud checkout 之外：

~~~cmd
call tools\setup_windows.bat
cmake -S LostOdysseyRecompSemantics -B "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-library" -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-library"
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

库内还实现了将 query 创建、槽位初始化和释放串联起来的 PooledQueryServices 组合 adapter；它是库内组合面，不是 runtime 接入。部分 heap 测试仍使用有边界的 stub 或合成 range 组合，因此不代表内存已完整恢复。下一步是 special allocator、indirect commit／manager、真实 native kernel lifetime、unwind 和 runtime 场景验证。后续按证据和热点扩展，不为所有地址生成占位实现。
当前已有二十八个可读实现和二十八个有边界行为记录。二十七个普通 PPC 函数共 4,551 例，加上 74 个 query-lifecycle 组合用例和 1 个补充 heap-lifecycle 序列为 4,626 例；cache 单独记录 1,042 条轨迹（1,040 条完整、2 条有限前缀）。runtime、scene 和 complete 仍为 0，cache 结果不代表硬件同步已验收。
memory-fill 对照还覆盖未注册的 PPCContext adapter，包括 store address、width、value 和 order。该 adapter 只属于对照 harness，不是 runtime 接入。函数名和字段名是根据 PPC 证据推断的工作名称，不是已恢复的原始 debug symbol。
fill adapter 不证明 fault 期间的中间 PPCContext 状态，也未在 optimized 或 live path 上测试。

从仓库根目录复现有边界的 oracle 对照：

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
~~~

生成下方汇总报告前，还需运行其余[语义对照脚本](../tools/ghidra/README.md)；receipt 列表需要全部二十八个分函数结果。

当前进度报告覆盖 62,627 个候选地址，其中 28 个可读函数、28 个有边界对照函数，runtime、scene 和 complete 均为 0。receipt 保存在仓库外，可用进度工具汇总：

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
  'semantic-heap-segment-tests\receipt-827CC2C0.json'
)
$receiptArgs = foreach ($relative in $receiptRel) { '--receipt'; Join-Path $evidenceRoot $relative }
python tools/ghidra/semantic_progress.py `
  --catalog out/decomp-index/catalog.sqlite `
  --manifest LostOdysseyRecompSemantics/recovery.json `
  @receiptArgs `
  --output out/semantic-recovery
~~~

有边界的对照结果只支持其记录范围，不代表 runtime 已接入、场景覆盖、完整语义或性能变化。
库内 query-lifecycle fixture 另外通过 74 个用例，receipt 为 `semantic-query-lifecycle-tests/receipt-827B7408-lifecycle.json`。独立的 heap-lifecycle runner 另外记录 1 条多 body 的 Allocate→Free→Allocate 序列，receipt 为 `semantic-heap-lifecycle-tests/receipt.json`；它是补充组合证据，不放入二十八条分函数 receipt 列表。该序列核对了 Grow、Initialize、Extend、Decommit 和首次 Fill，包括 0x4000 decommit 与未提交 range 合并；Kernel/process/lock 部分为 synthetic，未验证 runtime 并发或物理页状态。这些是附加组合记录，不是额外函数，也不替代原有分函数 receipt。27 个普通 PPC 函数共 4,551 例，加上 74 个 query-lifecycle 用例和 1 条 heap-lifecycle 序列为 4,626 例；cache 的 1,042 条轨迹单独记录（1,040 条完整、2 条有限前缀）。

详见[恢复计划](RECOVERY_PLAN.md)和 [English README](README.md)。recovery.json 是机器可读的状态记录。恢复期间保留原始 Ghidra 工程、生成的 PPC 源码和现有 runtime 实现。
