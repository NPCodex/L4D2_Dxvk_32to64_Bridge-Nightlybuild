# 37 项调查中的四项采用

本次版本只采用四项已完成候选，生产包不把调查中的其他方向写成优化结果。

| 候选 | 层级 | 采用内容 | 证据边界 |
| --- | --- | --- | --- |
| O34 | Bridge | retention 数据库使用一次 `from_chars` 解析，并延迟初始化 CNG SHA-256 provider | x86 warm-cache `Database::open` 约快 49%，约 0.36-0.42 ms/次；没有 FPS、冷启动或长期收益证明 |
| GPL-CLIP-01 | GPLALL 源码 | 把裁剪平面快速覆盖值按 specialization packed dword 正确移位 | GPL=True 的四个裁剪语义案例修正，其余案例保持；这是正确性修复 |
| GPL-WORKERS-01 | GPLALL 源码 | `hardware_concurrency()` 为 0/1 时先饱和减法，再应用原有上限和配置覆盖 | 修复低核数下溢导致的错误 worker 数；32 逻辑核实机没有速度收益结论 |
| GPL-SPEC-01 | GPLALL 源码 | 用后端实际 `canUseGraphicsPipelineLibrary()` 判断 specialization UBO 路径 | 四轮对照方向不稳定；没有稳定 FPS、1% low 或全场景收益结论 |

GPLALL 源码固定为 `https://github.com/Digger1955/dxvk-gplall@6a0ea561f9add008899680e6c313aa21c151e03e`。正式构建把三项 GPL 补丁和已发布的 MSAA `DontUnroll` shader 补丁应用到同一源码树，分别生成 x86/x64 D3D9 DLL，并由 `BACKEND-SOURCE.json` 与 runtime 包校验绑定。旧的预编译 GCC DLL 配方不再作为本版本的 GPLALL 产物来源。

本次没有重新启动原 `E:` 玩家安装，也没有新增游戏性能或长期稳定性结论。若出现掉帧或不稳定，退出游戏和所有 Host 后成套回退到 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)，并恢复该版本的两个 GPLALL 后端。
