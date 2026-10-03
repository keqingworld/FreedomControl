# FreedomControl 0.5.5

> **把控制权还给玩家。** 这是一个面向 **The Elder Scrolls V: Skyrim Anniversary Edition 1.6.1170** 的 SKSE / CommonLibSSE-NG 插件项目。

**制作：莱茵多特 😄**  
Bilibili：<https://space.bilibili.com/1329692237>

当前公开源码构建：`FC-0.5.5-VMARGS18-20261002-N`

## 这是什么？

FreedomControl 的目标很直接：**尽可能让 Skyrim 里的玩家拥有真正的自由控制权**。

它不是只加几个作弊按钮，而是把玩家控制、世界规则、NPC/生物、随从、军团、生成器、任务、运行时状态以及扩展功能集中到同一个框架里。主入口为 **F8**。

## 主要功能

- **玩家自由控制**：持续恢复移动 / 攻击 / 施法 / 视角等玩家控制，处理部分 Scene、家具与脚本残留造成的锁定。
- **World Freedom**：等待、旅行、训练次数及一系列世界限制的自由控制入口。
- **门 / 物体接管**：对准目标进行解封、接管与持续维护，处理部分原版无法正常激活的对象。
- **犯罪 / 守卫控制**：赏金、抓捕、监禁等相关流程的控制与保护逻辑。
- **NPC / Creature 接管**：角色状态、行为、AI Package、移动与战斗相关控制。
- **Follower**：独立随从控制与恢复逻辑，支持大量引用槽位。
- **Legion**：军团成员、战斗目标与队伍行为管理。
- **Spawner + Catalog**：检索并生成 NPC / 生物 / 对象；支持批量生成与生物目录。
- **Quest Center**：任务检索、阶段 / 目标控制，以及自建任务槽位。
- **Kernel / Runtime / Save**：统一的规则、状态与运行时持久化基础层。
- **SexLab QuickStart**：可选扩展。快速选择参与者与标签并启动场景；运行时检测 SexLab Framework。
- **F8 控制中心**：打开时可完整暂停世界、隔离输入，并维护进入菜单前的主音量状态。

更细的实现范围与边界见 [`FEATURES_zh-CN.md`](FEATURES_zh-CN.md) 与 [`BUILD_STATUS.md`](BUILD_STATUS.md)。

## 自由使用 / Fork

这个仓库就是为了让大家能看、能学、能改、能 Fork。

**你可以：**

- 阅读、研究和学习源码；
- Fork 到自己的 GitHub；
- 修改代码、加入新功能；
- 做自己的兼容版、实验分支；
- 免费分享原版或修改版源码 / 二进制；
- 提交 Pull Request，一起改进项目。

**唯一需要特别注意：不要把它拿去收费或二次贩卖。**

- 不得出售 FreedomControl 原版、修改版、Fork、重打包版本或下载权限；
- 不得把它放进付费墙、订阅、付费整合包中作为收费内容；
- 免费再分发可以，但请保留原作者署名和本仓库许可证；
- 修改版请明确标注你做过哪些修改，不要冒充官方原版。

完整条款见 [`LICENSE`](LICENSE)。这是一个**源码公开 / 非商业再分发许可**，不是标准 MIT 许可证。

## 环境

主要目标环境：

- Skyrim AE **1.6.1170**
- SKSE **2.2.6**
- Windows x64
- Visual Studio / MSVC
- C++23
- CommonLibSSE-NG
- CMake + vcpkg

当前 CMake 配置固定启用 AE，关闭 SE / VR 目标。

## 编译

推荐目录：

```text
D:\SkyrimDev\
├─ CommonLibSSE-NG\
└─ FreedomControl\
```

准备好本地 `CommonLibSSE-NG`、vcpkg / CMake / MSVC 后，在项目目录运行：

```powershell
.\VERIFY_LATEST.bat
.\BUILD.bat
```

验证成功应看到：

```text
VERIFIED: FC-0.5.5-VMARGS18-20261002-N
```

构建完成后，MO2 安装包默认生成到：

```text
dist\FreedomControl-0.5.5-VMARGS18-MO2.zip
```

不要使用 `PACKAGE_ONLY` 去尝试修复 C++ 编译错误；它只负责重新打包已有构建结果。

## 源码结构

```text
src/            Skyrim / SKSE 运行时实现
include/fc/     核心策略与公共头文件
cmake/          构建、打包和源码身份逻辑
package/        ESP、INI、生物目录等运行时资源
tests/          主机侧、策略、打包与回归测试
tools/          ESP / Catalog / Runtime 辅助工具
docs/           当前版本技术说明与审计资料
```

## 0.5.5 / VMARGS18

0.5.5 在 SEXFAST16 的基础上修正了 Papyrus `MakeFunctionArguments` 参数类型推导问题。VMARGS18 通过按值封装参数后再交给 CommonLib 工厂，避免数组元素被推导成不被接受的引用类型，同时保留原有游戏逻辑。

源码包中的自动化验证覆盖 GCC / Clang 主机侧测试、打包、运行时记录、构建身份及 VM 参数契约。最终 Windows / Skyrim 实机使用仍以实际安装环境为准。

## 关于第三方组件

FreedomControl 使用或依赖 SKSE、CommonLibSSE-NG、ImGui、fmt、spdlog、nlohmann/json 等第三方项目。它们仍遵循各自的许可证、例外条款和版权声明；本仓库的许可证不会替代第三方许可证，也不授予 Bethesda 游戏资产的任何权利。

## 作者

**莱茵多特 😄**  
Bilibili：<https://space.bilibili.com/1329692237>

欢迎 Fork、研究、改进和免费分享。只要别拿去收费 / 二次贩卖就行。
