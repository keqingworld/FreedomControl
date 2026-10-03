# FreedomControl 0.5.5

> **把控制权还给玩家。**

FreedomControl 是面向 **The Elder Scrolls V: Skyrim Anniversary Edition 1.6.1170** 的
SKSE / CommonLibSSE-NG 插件项目。

**制作：莱茵多特 😄**  
Bilibili：<https://space.bilibili.com/1329692237>

当前源码构建标识：

```text
FC-0.5.5-VMARGS18-20261002-N
```

## 主要功能

- 玩家自由控制：恢复移动、攻击、施法、视角等控制，处理部分 Scene / Furniture / 脚本残留锁定。
- World Freedom：等待、旅行、训练次数以及一系列世界规则控制。
- 门 / 物体接管：对准对象进行解封、接管与持续维护。
- 犯罪 / 守卫控制：赏金、抓捕、监禁等流程的控制与保护逻辑。
- NPC / Creature 接管：状态、行为、AI Package、移动与战斗相关控制。
- Follower：独立随从控制、恢复与引用槽位系统。
- Legion：军团成员、目标、战斗与队伍行为管理。
- Spawner + Catalog：检索并生成 NPC / 生物 / 对象。
- Quest Center：任务检索、阶段 / 目标控制及自建任务。
- Kernel / Runtime / Save：规则、状态和运行时持久化基础层。
- SexLab QuickStart：可选扩展；快速选择参与者、标签并启动场景。
- F8 控制中心：统一菜单入口；暂停、输入隔离与主音量状态维护。

更细的范围见 [`FEATURES_zh-CN.md`](FEATURES_zh-CN.md)。

## 使用 / Fork

你可以：

- ✅ 阅读、学习、研究源码
- ✅ Fork
- ✅ 修改、重构、加入新功能
- ✅ 发布自己的免费修改版
- ✅ 免费分享源码或二进制
- ✅ 提交 Pull Request

唯一明确限制：

> **❌ 不允许收费、二次贩卖、付费下载、付费墙，或把 FreedomControl / Fork 当作付费整合包内容出售。**

修改版应说明已修改，并保留原作者署名与许可证。

完整条款见 [`LICENSE`](LICENSE)。

> 因为禁止商业再分发，本项目采用 source-available（源码公开）许可，
> 并非 OSI 定义下的标准开源许可证。

## 目标环境

- Skyrim AE **1.6.1170**
- SKSE **2.2.6**
- Windows x64
- Visual Studio / MSVC
- C++23
- CommonLibSSE-NG
- CMake + vcpkg

## 编译

推荐目录：

```text
D:\SkyrimDev\
├─ CommonLibSSE-NG\
└─ FreedomControl\
```

准备 CommonLibSSE-NG、CMake、MSVC 与 vcpkg 后：

```powershell
.\VERIFY_LATEST.bat
if ($LASTEXITCODE -eq 0) { .\BUILD.bat }
```

验证成功：

```text
VERIFIED: FC-0.5.5-VMARGS18-20261002-N
```

MO2 包默认输出：

```text
dist\FreedomControl-0.5.5-VMARGS18-MO2.zip
```

## 目录

```text
src/        Skyrim / SKSE 运行时实现
include/    核心策略与公共头文件
cmake/      构建、打包和源码身份逻辑
package/    ESP、INI、生物目录等运行时资源
tests/      主机侧、策略、打包与回归测试
tools/      ESP / Catalog / Runtime 辅助工具
docs/       当前版本技术说明
```

## 0.5.5 / VMARGS18

0.5.5 在 SEXFAST16 / COMPILE17 基础上修复 Papyrus `MakeFunctionArguments`
参数类型推导问题。VMARGS18 通过按值封装参数，再交给 CommonLib 工厂，
避免数组元素被推导为不被接受的引用类型。

## 隐私清理

本 GitHub 源码包已移除历史验证日志、沙箱路径、build / dist / vcpkg_installed、
DLL / PDB / OBJ / LIB、Git 元数据、本地 build-settings.json / commonlib.lock.txt、
密钥、Token、私人邮箱与用户主目录信息。

`package/FreedomControlRuntime.esp` 是项目运行时资源，因此保留。

## 作者

**莱茵多特 😄**  
Bilibili：<https://space.bilibili.com/1329692237>
