# 自由内核11：实现、证据和边界

构建：FC-0.5.0-KERNEL11-20260925-G。基于完整0.4.0源码升级，未删除此前军团、生成器、净域、时间/相机/技能/物品功能。

## 1. 门不能只靠一个 Owner 值

旧 Own 已写归属和解锁，本版同时 SetActivationBlocked(false) 并记录具体引用/基础ID。对象页增加组合“接管并使用”，另有配对门直接通行。OwnUse 和等待/旅行被投递到 afterClose，先释放自己的暂停和输入占用，再请求原生激活。

持续守护只检查本工具接管的引用，循环队列每次最多32个；核验实例对应Base未改变、所属Cell已附着、对象未禁用/删除。没有删除或重启原对象脚本。已接管室内Cell名称/Owner在当前位置重新维护。自动准星接管只处理同空间、1024单位内的可交互对象，不碰Actor。

这不是所有谜题门脚本的万能补丁。普通归属、锁、BlockActivation组合已在代码实现；没有传送链接的装饰物不会凭空有目的地；强制默认动作也不等于执行每个Mod要求的任务阶段。

## 2. 犯罪与守卫

CrimeHooks11.cpp 只在1.6.1170上安装PlayerCharacter主虚表B5/B6/B9/BA/BB对应的罪金写入/增加、入狱、服刑、罚款入口。保留此前函数供关闭策略时调用，策略在存档ready后生效、卸载/读档前关闭。Hook只使用原子开关、计数、清理请求，不在回调内修改插件容器或获取状态互斥锁。

声明依据为固定CommonLib commit的Actor.h；目标页保护和地址检查失败则报告未安装。尚未做真实ABI/地址/Windows Hook测试，也不宣称能压过之后安装的其他Hook或未知Mod直接调用其他路径。INI允许禁用此Hook，定时清理作为较弱备用，不假称备用拥有相同拦截能力。

周期维护清除现有罪金/被捕标志。守卫处理要求同空间/配置距离，且正在犯罪搜索、对玩家愤怒或正攻击己方。只停止相关守卫战斗/抓捕强制问话，保留正常主动对话，排除本工具生成的敌人和军团成员。不把所有敌人变和平。

## 3. 玩家控制权

逐项恢复ControlMap中11类已知操作位，保留未知位/原storedControls；可取消任意项目。F8自身输入阻断、原生暂停、加载、正常对话和手动暂停守护时跳过，避免面板鼠标点击透到游戏。

可另解除AI驱动、移动/攻击/施法/视角固定标志。强自由只解除玩家的Scene链接和玩家家具占用，不调用不存在的C++ BGSScene::Stop，也不停止场景里所有NPC或全局所有Quest。因此某些脚本仍可能重新安排场景；本版通过周期检查重新释放，不能声称在每条外部指令执行前裁决。

F8自由等待调用StartWaiting，再分帧AdvanceSleepWaitTick，不使用原版等待菜单的附近敌人许可。每帧最多8次推进，只推进本插件自己发起的等待。解除禁保存/等待标志和enablefasttravel权限并非替换原版地图/T键全部检查。每级训练计数清除、其他原有无限资源控制均可分别开关。

## 4. 任务中心，不要求手抄ID

UI直接检索当前加载的TESQuest数组，显示Full FormID、EditorID、来源、当前阶段/状态/目标，并通过immutable snapshot提供给渲染线程。原生ID由当前加载顺序解析，不硬编码网站上的十六进制加载前缀。

当前候选阶段来自executedStages、waitingStages、GetCurrentStageID，不能把这个集合当完整静态剧本或完整攻略。阶段/目标状态命令只拼接内部验证的数字ID/索引，不拼接用户输入的任意名字或目标文本。任务文字通过SetFullName/BSFixedString写入，由当前co-save保存改动；在新存档载入前恢复上次会话的原显示文本。

按任务拒接最多2048个，周期请求停止再次启动的任务，不会撤销该任务过去产生的后果。自有军团运行任务和自建模板通过各自专用页面管理，避免从剧情编辑器破坏基础控制器。

## 5. 自建任务实际增加了哪些内容

FreedomControlRuntime.esp原0x800等军团/净域记录本地ID保持不变；新增0x900—0x93F共64个原生QUST。每个模板8目标（10—80），阶段0/10/100/200，完成/失败标志，类型SideQuest；不自动启动，无第三方Papyrus/VMAD脚本，也不在现有剧情任务里插入虚假阶段。文件共99个非TES4记录。

主线程可创建、保存、运行、暂停、放弃、完成、重置、删除并复用槽。每项维护乐观修订号，UI草稿旧于自动进度时拒绝覆盖。重复标题以槽和ID区分。已完成且disabled的原生任务重同步时不会Start/重放stage100；恢复运行时清掉旧完成状态。

四种目标判定：手动、库存数量、同室内Cell/同室外世界内到达坐标、指定实例死亡并核验Base一致。一次检查复用一个库存快照。不把缺失引用或彻底删除当作击杀。地点仅用于判定，不自动绑定地图目标箭头；没有生成对白/配音/任意任务脚本。代码对原生状态做读回，J日志实际表现和ESP加载未在本环境验收。

## 6. 测试并不等于实机

新生产Kernel11/QuestCenter11/KernelSave11文件在明确模拟引擎环境中编译并执行76条控制流断言；真实可用fmt头用于命令格式，JSON使用标明为TEST的值树适配器，不是nlohmann真实解析器。CrimeHooks11实际文件对TEST虚表/OS函数执行26条；旧控制层84条。GCC及Clang ASan/UBSan分别执行。5个独立CTest程序均通过。

另有真实CMake打包56场景、身份/缓存14场景（含clang-cl/lld真实链接仅版本标识文件的fixture）、原创记录与目录16项结构检查，菜单类/Engine头的桩环境语法检查。它们不是Windows完整插件编译、真实CommonLib布局、SKSE、xEdit/CK加载或Skyrim玩法测试。记录和脚本变更要以新的Windows构建与实机结果为准。

## 7. 主要一手参考

固定CommonLib提交 `a898f469851c464d05137bb74b069dd234897643`：
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/Actor.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/P/PlayerCharacter.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/C/ControlMap.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UserEvents.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESQuest.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESObjectREFR.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BGSScene.h

xEdit作者维护的SSE记录定义（QUST/DNAM/INDX/QSDT/QOBJ/FNAM）：
- https://raw.githubusercontent.com/TES5Edit/TES5Edit/dev-4.1.5/Core/wbDefinitionsTES5.pas

对照声明/格式是实现依据，不是机器码级签名或运行正确性证明。
