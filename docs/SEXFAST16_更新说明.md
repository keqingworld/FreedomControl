# FreedomControl 0.5.5 · SexFast16 / 完整冻结修复16

构建：`FC-0.5.5-SEXFAST16-20261002-L`。

## SexLab 快速启动器

- 新增独立页面“SexLab 快速启动”。
- 运行时检测 `SexLab.esm` 的 Framework Quest（本地 ID 0xD62）及 `SexLabFramework` 绑定状态。
- 一键模式：玩家 + 当前准星 Actor，面板关闭后直接调用 `SexLabFramework.QuickStart`。
- 自定义模式：最多 5 名参与者，支持加入玩家、当前目标、附近角色、上移/下移/移除/清空。
- 动画标签可留空：留空时由 SexLab 自己根据参与者、种族与已注册动画自动匹配；也可填写已注册标签缩小范围。
- 参与者只保存 FormID；真正启动时重新解析当前引用，过滤已死亡、禁用或失效对象，避免持有跨 Cell 的裸指针。
- 启动动作强制 `afterClose`：FreedomControl 先退出 F8、恢复时间/输入/声音并确认原生暂停菜单已退出，再向 Papyrus VM 提交 QuickStart，避免在暂停状态中启动 SexLab。
- 增加 `sslThreadSlots.StopAll` 快捷入口，用于停止/清理全部活动 SexLab 线程；同样在 F8 关闭后提交。
- “已提交”只表示 Papyrus VM 接受调用，不伪报 SexLab 动画一定已经进入播放；SLAL 注册、Creature/Race 兼容仍由现有 SexLab/MNC/Creature Framework 环境决定。

## F8 完整冻结修复

REPAIR15 的原生 `kPausesGame` 菜单继续负责真正暂停游戏模拟。SexFast16 再增加主声音分类租约：

1. F8 打开时记录当前 Master Sound Category 音量；
2. 面板保持打开期间持续把主音量钳到 0；
3. F8 关闭、读档、异常释放路径中恢复打开前的确切主音量；
4. 如果其他代码在面板期间改动主音量，下一次同步会重新钳到 0；
5. 音频接口不可用时会记录警告，不把“世界暂停”误报为“音频已冻结”。

这使面板打开期间世界模拟暂停且游戏主音频静音。它不声明底层音频播放游标本身停止推进；关闭后某个长音效可能从后续位置继续，这是“静音 + 恢复”与“音频引擎暂停”之间的真实边界。

## 隐藏问题修复

- SexLab 页面不再引用 Engine.cpp 私有的 `kPlayer` 常量；使用页面本地 Player FormID，避免单独编译 Overlay 时失败。
- 版本、构建 ID、vcpkg 版本、打包 ZIP 名、源码指纹与验证脚本统一升级到 0.5.5。
- `SexLab16.cpp` 加入源码完整性检查和 Windows 构建前置检查，避免“源码丢了但旧对象仍被打包”。
- Pause seam 把音频设备明确作为宿主边界；音量租约逻辑由独立 runtime tests 覆盖，避免用测试桩冒充真实音频设备。
