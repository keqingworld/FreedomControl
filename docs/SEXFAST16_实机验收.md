# SexFast16 实机验收

1. `VERIFY_LATEST.bat` 应显示 `VERIFIED: FC-0.5.5-SEXFAST16-20261002-L`。
2. 必须运行 `BUILD.bat`，不要只用 PACKAGE_ONLY 包装旧 DLL。
3. MO2 安装 `dist\FreedomControl-0.5.5-SEXFAST16-MO2.zip`，只保留一份 FreedomControl；保持 `FreedomControlRuntime.esp` 启用。
4. 进游戏后 F8 标题应包含 `0.5.5 | SexFast / 完整冻结修复16`。
5. 打开 F8：NPC/物理/世界时间应暂停，游戏主音频应立即无声；关闭 F8 后恢复进入面板前的音量。
6. F8 -> SexLab 快速启动：准星指向一个活着 NPC/生物，动画标签先留空，点击“玩家 + 当前准星目标”。面板应先关闭，再由 SexLab 尝试自动匹配动画。
7. 多人场景：加入参与者、调整顺序，最多 5 人，再点启动。若页面显示 SexLab 未绑定，先完成 SexLab MCM 初始化。
8. 若日志显示 `QuickStart dispatched` 但没有动画，检查 SexLab/SLAL 注册状态、Creature Framework/MNC 和参与者种族；这不是 FreedomControl 可以伪造为成功的阶段。
