# FreedomControl 0.5.5

**把 Skyrim 的控制权真正还给玩家。**

FreedomControl 是一个面向 **The Elder Scrolls V: Skyrim Anniversary Edition 1.6.1170** 的
SKSE / CommonLibSSE-NG 插件。它把玩家控制、世界规则、NPC / Creature、Follower、
Legion、Spawner、Quest Center 与运行时状态管理集中到同一个 **F8 控制中心**。

> 当前源码构建：`FC-0.5.5-VMARGS18-20261002-N`

| 项目 | 当前目标 |
| --- | --- |
| 游戏版本 | Skyrim AE 1.6.1170 |
| SKSE | 2.2.6 |
| 平台 | Windows x64 |
| 编译器 | MSVC / C++23 |
| 基础库 | CommonLibSSE-NG |
| 菜单 | F8 |

## 特性

| 模块 | 作用 |
| --- | --- |
| **Player Freedom** | 恢复移动、攻击、施法、视角等控制，处理部分 Scene / Furniture / 脚本残留锁定 |
| **World Freedom** | 等待、旅行、训练次数与多类世界规则控制 |
| **Object Takeover** | 对准门、家具和可交互对象进行解封、接管与持续维护 |
| **Crime / Guard** | 赏金、抓捕、监禁等流程的控制与保护逻辑 |
| **NPC / Creature** | 状态、行为、AI Package、移动与战斗相关控制 |
| **Follower** | 独立随从控制、恢复与引用槽位系统 |
| **Legion** | 军团成员、目标、战斗与队伍行为管理 |
| **Spawner + Catalog** | 检索并生成 NPC、生物与对象 |
| **Quest Center** | 任务检索、阶段 / 目标控制与自建任务 |
| **Kernel / Save** | 规则、状态与运行时持久化基础层 |
| **SexLab QuickStart** | 可选扩展：快速选择参与者、标签并启动场景 |
| **F8 Control Center** | 统一入口，包含暂停、输入隔离和主音量状态维护 |

更完整的功能范围见 [FEATURES_zh-CN.md](FEATURES_zh-CN.md)。

## 当前状态

0.5.5 已由项目作者在目标 Windows / MSVC / Skyrim AE 1.6.1170 环境完成编译和实机测试，
核心功能整体可正常运行。

不同 Mod List、SKSE 插件组合和运行环境仍可能出现兼容性差异。
详细说明见 [BUILD_STATUS.md](BUILD_STATUS.md)。

## 编译

推荐目录：

```text
D:\SkyrimDev\
├─ CommonLibSSE-NG\
└─ FreedomControl\
```

准备好 CommonLibSSE-NG、CMake、MSVC 与 vcpkg 后：

```powershell
.\VERIFY_LATEST.bat
if ($LASTEXITCODE -eq 0) { .\BUILD.bat }
```

验证成功应显示：

```text
VERIFIED: FC-0.5.5-VMARGS18-20261002-N
```

构建完成后，MO2 安装包默认输出到：

```text
dist\FreedomControl-0.5.5-VMARGS18-MO2.zip
```

## 仓库结构

```text
src/        Skyrim / SKSE 运行时实现
include/    核心策略与公共头文件
cmake/      构建、打包和源码身份逻辑
package/    ESP、INI、生物目录等运行时资源
tests/      主机侧、策略、打包与回归测试
tools/      ESP / Catalog / Runtime 辅助工具
docs/       技术说明、版本演进与实机验收记录
```

技术文档索引见 [docs/README.md](docs/README.md)。

## Fork 与使用规则

这个项目的源码公开，鼓励研究、学习、修改和 Fork。

**允许：**

- 阅读、研究和学习源码
- Fork、重构、修改和加入新功能
- 发布自己的免费修改版
- 免费分享源码或编译后二进制
- 提交 Pull Request

**不允许：**

- 收费出售原版、修改版或 Fork
- 二次贩卖
- 付费下载 / 付费墙
- 将 FreedomControl 或其衍生版本作为付费整合包内容出售
- 修改后冒充官方原版

完整条款见 [LICENSE](LICENSE)。

> 本项目采用自定义 **source-available / 源码公开非商业许可**。
> 因为限制商业再分发，它不是 OSI 定义下的标准 Open Source 许可证。

## 0.5.5 · VMARGS18

0.5.5 在 SEXFAST16 / COMPILE17 基础上修复了 Papyrus `MakeFunctionArguments`
参数类型推导问题。VMARGS18 按值封装参数后再交给 CommonLib 工厂，
避免数组元素被推导成不被接受的引用类型。

## 第三方项目

FreedomControl 使用或依赖 SKSE、CommonLibSSE-NG、ImGui、fmt、spdlog、
nlohmann/json 等第三方项目。它们继续遵循各自许可证和版权声明。

## 作者

**莱茵多特 😄**

Bilibili：<https://space.bilibili.com/1329692237>

欢迎 Fork、研究、改进和免费分享。
