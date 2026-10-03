# Contributing

FreedomControl 欢迎研究、Fork、修改和 Pull Request。

## 基本原则

- 尽量保持 Skyrim AE 1.6.1170 的稳定性。
- 不要为了“功能更多”而破坏玩家已有控制权。
- 涉及 Hook / 输入 / Pause / Save / Papyrus 的改动，请说明实机测试范围。
- 新功能尽量提供可关闭的开关，不强迫玩家采用单一玩法。
- 提交前避免带入 `build/`、`dist/`、vcpkg 缓存、PDB、OBJ、个人路径和本地配置。

## Pull Request

PR 建议写清：

1. 改了什么；
2. 为什么要改；
3. 是否改变存档格式；
4. 是否需要新的依赖；
5. 在哪个 Skyrim / SKSE 版本测试；
6. 是否影响现有 F8 菜单、输入、暂停或第三方 Mod 兼容。

## License

贡献到本仓库的代码应当允许按仓库根目录 `LICENSE` 的非商业源码公开许可进行分发。

本项目允许免费 Fork 和免费修改版，但不允许收费、二次贩卖或放入付费墙。
