#include "PCH.h"
#include "Engine.hpp"
#include "Overlay.hpp"
#include "Input13.hpp"
#include "fc/HotkeyState.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace fc::overlay {
namespace {
using Microsoft::WRL::ComPtr;
using PresentFn = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
PresentFn originalPresent{};
WNDPROC originalWndProc{};
HWND window{};
IDXGISwapChain* hookedSwapchain{}; // Borrowed; game owns its lifetime.
ImGuiContext* context{};
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> deviceContext;
std::recursive_mutex uiMutex;
bool initialized{}, initAttempted{}, installAttempted{};
HotkeyState hotkeyState;
bool focusKnown{}, wasFocused{}, menuDrawLogged{}, drawFailureLogged{};
bool mousePrimed{}, mouseClickLogged{}, wndProcStateKnown{}, wndProcOwned{};
std::array<bool,5> mouseButtons{};
std::uint64_t polledMousePresses{};
float polledMouseX{}, polledMouseY{};
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// This is the ONLY hotkey toggle path. WndProc must not also toggle on key-up,
// or a press would open the menu and its release would immediately close it.
void PollMenuHotkey() {
    auto& engine=Engine::Get();
    const HWND foreground=GetForegroundWindow();
    const HWND root=window ? GetAncestor(window,GA_ROOT) : nullptr;
    const bool focused=window && foreground &&
        (foreground==window || (root && GetAncestor(foreground,GA_ROOT)==root));
    // Only the high bit means "currently down". The low "since last call" bit
    // can be consumed by a different caller and must not be used here.
    const bool down=focused && (GetAsyncKeyState(engine.Hotkey()) & 0x8000)!=0;
    if (!focusKnown || focused!=wasFocused) {
        spdlog::info("Input-fix.6: game foreground={}, menu key VK=0x{:02X}.",focused,engine.Hotkey());
        // Alt-Tab does not close F8 or release its native world pause. Input13
        // clears text/key state on focus loss; HotkeyState primes on return.
        focusKnown=true;
        wasFocused=focused;
    }
    if (hotkeyState.Update(focused,down)) {
        const bool open=!engine.MenuOpen();
        engine.SetMenuOpen(open);
        menuDrawLogged=false;
        drawFailureLogged=false;
        spdlog::info("Input-fix.6: menu {} via polled key VK=0x{:02X}.",open ? "opened" : "closed",engine.Hotkey());
    }
}

bool IsMouseButtonMessage(UINT msg) {
    switch (msg) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        return true;
    default:
        return false;
    }
}

bool GameWindowFocused() {
    const HWND foreground=GetForegroundWindow();
    const HWND root=window ? GetAncestor(window,GA_ROOT) : nullptr;
    return window && foreground && (foreground==window || (root && GetAncestor(foreground,GA_ROOT)==root));
}

void PollMouseInput() {
    auto& engine=Engine::Get();
    auto& io=ImGui::GetIO();
    const bool active=engine.MenuOpen() && GameWindowFocused();
    if (!active) {
        for (std::size_t i=0;i<mouseButtons.size();++i)
            if (mouseButtons[i]) io.AddMouseButtonEvent(static_cast<int>(i),false);
        mousePrimed=false;
        mouseButtons.fill(false);
        return;
    }

    POINT point{};
    if (GetCursorPos(&point) && ScreenToClient(window,&point)) {
        polledMouseX=static_cast<float>(point.x);
        polledMouseY=static_cast<float>(point.y);
        io.AddMousePosEvent(polledMouseX,polledMouseY);
    }

    constexpr std::array<int,5> vkeys{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
    std::array<bool,5> current{};
    for (std::size_t i=0;i<vkeys.size();++i) current[i]=(GetAsyncKeyState(vkeys[i]) & 0x8000)!=0;

    if (!mousePrimed) {
        mouseButtons=current;
        mousePrimed=true;
        return;
    }
    for (std::size_t i=0;i<current.size();++i) {
        if (current[i]==mouseButtons[i]) continue;
        io.AddMouseButtonEvent(static_cast<int>(i),current[i]);
        if (current[i]) {
            ++polledMousePresses;
            if (!mouseClickLogged) {
                spdlog::info("Input-fix.6: polled mouse press received (button={}, x={:.0f}, y={:.0f}).",i,polledMouseX,polledMouseY);
                mouseClickLogged=true;
            }
        }
        mouseButtons[i]=current[i];
    }
}

void UpdateWndProcState() {
    if (!window) return;
    const auto current=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC));
    const bool ours=current==&WindowProc;
    if (!wndProcStateKnown || ours!=wndProcOwned) {
        wndProcOwned=ours;
        wndProcStateKnown=true;
        spdlog::info("Input-fix.6: current game WndProc owned by FreedomControl={}; mouse buttons use polling fallback regardless.",ours);
    }
}

struct ContextScope {
    ImGuiContext* before{ImGui::GetCurrentContext()};
    explicit ContextScope(ImGuiContext* wanted) { ImGui::SetCurrentContext(wanted); }
    ~ContextScope() { ImGui::SetCurrentContext(before); }
};
struct Menu {
    View view;
    char questQuery11[192]{},questTitle11[256]{},questGoalText11[768]{};
    ID questUI11{};int questFilter11{},objectiveEdit11{-1},waitHours11{1};std::uint64_t questEditEpoch11{};
    PersonalQuest11 personalDraft11;
    char personalTitle11[256]{};
    std::array<std::array<char,512>,kPersonalGoalLimit11> personalGoalText11{};
    std::array<std::array<char,16>,kPersonalGoalLimit11> personalFormText11{};
    bool personalDraftLoaded11{},personalPending11{};std::uint64_t personalDraftEpoch11{},personalRequest11{};
    std::shared_ptr<const std::vector<Entry>> catalog;
    std::vector<std::size_t> filtered;
    const std::vector<Entry>* lastCatalog{};
    std::string lastFilter;
    int category{}, lastCategory{-1};
    bool includeUnnamed{}, lastUnnamed{}, advanced{}, dragonsInside{};
    char filter[160]{}, refID[16]{}, formID[16]{}, actorValue[80]{"health"};
    char rename[128]{}, bookmarkName[128]{}, command[512]{}, cellEditor[96]{};
    char factionA[16]{}, factionB[16]{}, questID[16]{};
    ID selectedForm{};
    int amount{1}, skillValue{100}, eviction{1}, stage{}, factionRank{}, aliasIndex{};
    float move[3]{}, rotation[3]{}, scale{1}, statValue{1000};
    float speed{100}, jump{76}, worldSpeed{1}, hour{12}, timeScale{20}, month{}, fov{85}, cameraSpeed{10};
    float followDist{240}, leashDist{4000}, flySpeed{1000};
    bool settingsLoaded{}, onlyMissing{true}, playableOnly{true};
    int activePage{};
    char pageFilter[96]{}, npcFilter[128]{};
    std::string sourceFilter,lastSource;
    bool lastPlayable{true};
    float textScale{1};
    std::vector<std::string> sources;
    bool sizeReset{true};
    std::uint64_t settingsEpoch{};
    int uiClickTest{};
    CombatProfile memberProfile;
    char legionFilter[128]{},actorQuery[192]{},spawnFormText[16]{};
    float battleRadius{5000},spawnRadius{300}; int spawnCount{1},actorType{};
    bool legionSettingsLoaded{},spawnRecruit{true},spawnProfile{},actorUnique{},hideActorPresets{true},actorRaceFilter{},actorFilterDirty{true};
    std::uint64_t legionEpoch{}; ID spawnForm{},actorRace{};
    const std::vector<Entry>* actorCatalog{};
    std::string actorSource;
    std::vector<std::string> actorSources;
    std::vector<std::pair<ID,std::string>> actorRaces;
    std::vector<std::size_t> actorRows;
    int actorScope10{},soldierCount10{6},enemyCount10{4},enemyScope10{};
    float soldierRadius10{350},enemyRadius10{700},clearRadiusUI10{2000},stuckUI10{12};
    bool soldierProfile10{true},enemyProfile10{},enemySafe10{true},protectUI10{true};
    char enemyQuery10[192]{},enemySeed10[32]{},travelQuery10[128]{},clearConfirm10[24]{};
    char sexLabTags16[256]{};
    std::uint64_t configEpoch10{};
    bool configInit10{};

    void RuntimeStatus10() {
        ImGui::Text("独立任务：%s | 已登记 %zu / %zu",view.quest10?"运行中":(view.runtime10?"启动中，请关闭菜单等待片刻":"运行插件未就绪"),view.followers.size(),MaxLegionMembers);
        if(!view.runtime10) Help("本版新增 FreedomControlRuntime.esp。必须在 MO2 右侧插件页勾选；仅安装 DLL 不够。任务、别名和行为包属于本工具，不占用原版随从任务。");
    }
    void SpawnStatus10() {
        if(view.spawnTotal) {
            ImGui::Text("本批请求 %zu | 已创建 %zu | 模型/AI 就绪 %zu | 未通过 %zu | 等待模型/跟随确认 %zu",view.spawnTotal,view.spawnCreated,view.spawnVisible10,view.spawnFailed,view.births10);
            ImGui::Text("本批跟随包已读回 %zu | 就绪但跟随仍待确认 %zu",view.spawnFollowReady14,view.spawnFollowPending14);
            const float ratio=std::min(1.0f,static_cast<float>(view.spawnDone)/static_cast<float>(view.spawnTotal));
            ImGui::ProgressBar(ratio,ImVec2(-1,0),"引用创建进度（不等于模型就绪）");
            if((view.spawnPending || view.births10) && ImGui::Button("取消剩余生成 / 等待检查")) Send(Op::CancelSpawn);
        }
        Help("生成命令会自动关闭 F8，恢复游戏后才执行。已经创建的角色可在生成实例列表里选择、召回或移除；没有模型/AI 的引用不再伪报为可见士兵。");
    }
    void SyncFreedom10() {
        if(!configInit10 || configEpoch10!=view.epoch) {
            clearRadiusUI10=view.clearRadius10;stuckUI10=view.stuckDelay10;protectUI10=view.protectArmy10;
            configInit10=true;configEpoch10=view.epoch;
        }
    }
    void ClearSettings10(bool aura,bool world) {
        Engine::Get().Submit(Action{.op=Op::ClearConfig10,.value={clearRadiusUI10,aura?1.0f:0.0f,world?1.0f:0.0f,0},.epoch=view.epoch,.protectArmy=protectUI10});
    }

    void Send(Op op, ID target = 0, ID form = 0, int count = 1, std::string text = {},
              std::array<float,4> values = {}) {
        Engine::Get().Submit(Action{.op=op,.target=target,.form=form,.count=count,.value=values,.text=std::move(text),.epoch=view.epoch});
    }
    void Cmd(std::string text, ID target = 0, bool close = false) {
        // Native menus/COC must execute AFTER edits drain and our pause lease is released.
        Engine::Get().Submit(Action{.op=Op::Command,.target=target,.text=std::move(text),.epoch=view.epoch,.afterClose=close});
    }
    void SetStat(const char* name,float value,ID target=0x14,bool effective=true) {
        Send(Op::SetValue,target,0,effective?1:0,name,{value,0,0,0});
    }
    void Batch(int kind,const char* query="",bool unnamed=false) {
        Action a{.op=Op::AddAll,.target=0x14,.count=kind,.value={unnamed?1.0f:0.0f,static_cast<float>(std::clamp(amount,1,1000000)),0,0},
                 .text=query,.epoch=view.epoch,.source=sourceFilter,.onlyMissing=onlyMissing,.playableOnly=playableOnly};
        Engine::Get().Submit(std::move(a));
    }
    void NearbyPicker() {
        if (ImGui::BeginCombo("附近角色（可搜索）","选择 NPC / 龙 / 生物")) {
            ImGui::InputTextWithHint("##npcfilter","名称或 RefID",npcFilter,sizeof(npcFilter));
            ImGui::BeginChild("nearby-choices",ImVec2(0,230),true);
            for (const auto& r:view.nearby) {
                auto label=fmt::format("{} [{:08X}]  {:.0f}",r.name,r.id,r.distance);
                if (Contains(label,npcFilter) && ImGui::Selectable(label.c_str(),r.id==view.target.id)) {
                    Send(Op::Select,r.id); ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild(); ImGui::EndCombo();
        }
    }

    void Help(const char* text) { ImGui::TextWrapped("%s",text); }
    void RefButton(const char* label, Op op, int count = 1) {
        if (ImGui::Button(label)) Send(op,view.target.id,0,count);
    }
    void Check(const char* label, bool current, Op op) {
        if (ImGui::Checkbox(label,&current)) Send(op,0,0,current ? 1 : 0);
    }
    void TargetHeader() {
        ImGui::Text("区域：%s [%08X]",view.cellName.c_str(),view.playerCell);
        if (ImGui::Button("选择准星目标")) Send(Op::SelectCrosshair);
        ImGui::SameLine(); if (ImGui::Button("选择玩家")) Send(Op::SelectPlayer);
        ImGui::SameLine(); ImGui::SetNextItemWidth(110);
        ImGui::InputTextWithHint("##ref","RefID（十六进制）",refID,sizeof(refID));
        ImGui::SameLine(); if (ImGui::Button("按 RefID 选择")) if (auto id = ParseID(refID)) Send(Op::Select,*id);
        if (view.target.id) {
            const auto& t = view.target;
            ImGui::Text("目标：%s | Ref %08X | Base %08X",t.name.c_str(),t.id,t.base);
            ImGui::Text("所有权：%s [%08X] | 缩放 %.2f%s",t.owned?"玩家可合法使用":"其他所有者 / 待接管",t.owner,t.scale,t.disabled?" | 已禁用":"");
            if (t.house) ImGui::Text("房屋：%s [%08X] | %s",t.houseName.c_str(),t.house,t.houseOwned?"已接管":"未接管");
            if (t.actor) ImGui::Text("生命 %.1f | 魔法 %.1f | 耐力 %.1f%s%s",t.health,t.magicka,t.stamina,t.dead?" | 已死亡":"",t.dragon?" | 龙":"");
            if (t.stage >= 0) ImGui::Text("任务当前阶段：%d",t.stage);
        } else Help("把准星对准对象后打开菜单；也可以选择附近角色，或手动输入 RefID。");
        NearbyPicker();
        ImGui::Separator();
    }
    std::array<float,3> maximum12{100,100,100},percent12{100,100,100},regenDraft12{};
    bool initialized12{},fill12{true}; std::uint64_t epoch12{};
    int levelDraft12{1}; float carryDraft12{300};
    void ReadPlayer12() {
        maximum12={view.maxHealth,view.maxMagicka,view.maxStamina12};
        percent12={100*custom12::Fraction(view.health,view.maxHealth),100*custom12::Fraction(view.magicka,view.maxMagicka),100*custom12::Fraction(view.stamina12,view.maxStamina12)};
        regenDraft12=view.regen12;levelDraft12=view.level12;carryDraft12=view.carry12;
        initialized12=true;epoch12=view.epoch;
    }
    void CustomPlayer12() {
        if(!view.ready) { initialized12=false;Help("进入存档后可自定义玩家属性。");return; }
        if(!initialized12 || epoch12!=view.epoch)ReadPlayer12();
        if(!ImGui::CollapsingHeader("自定义生命 / 法力 / 耐力 / 等级",ImGuiTreeNodeFlags_DefaultOpen))return;
        if(ImGui::Button("读取当前属性到输入框"))ReadPlayer12();
        ImGui::Checkbox("修改上限时同时补满（关闭则保持当前比例）",&fill12);
        constexpr const char* labels[]={"生命","法力","耐力"};
        constexpr const char* rates[]={"healrate","magickarate","staminarate"};
        const float current[]={view.health,view.magicka,view.stamina12};
        const float maximum[]={view.maxHealth,view.maxMagicka,view.maxStamina12};
        for(int i=0;i<3;++i) {
            ImGui::PushID(i);
            ImGui::Text("%s：%.1f / %.1f",labels[i],current[i],maximum[i]);
            ImGui::ProgressBar(custom12::Fraction(current[i],maximum[i]),ImVec2(-1,0),labels[i]);
            ImGui::SetNextItemWidth(150);ImGui::InputFloat("目标上限",&maximum12[i],10,100,"%.1f");
            ImGui::SameLine();ImGui::BeginDisabled(!custom12::ValidMaximum(maximum12[i]));
            if(ImGui::Button("应用上限"))Send(Op::ResourceMax12,0,0,i,{}, {maximum12[i],fill12?1.0f:0.0f,0,0});
            ImGui::EndDisabled();
            ImGui::SetNextItemWidth(150);ImGui::SliderFloat("当前百分比",&percent12[i],0,100,"%.1f%%");
            ImGui::SameLine();if(ImGui::Button("应用当前比例"))Send(Op::ResourcePercent12,0,0,i,{}, {percent12[i],0,0,0});
            ImGui::SetNextItemWidth(150);ImGui::InputFloat("基础恢复率（% / 秒）",&regenDraft12[i],0.1f,1,"%.2f");
            ImGui::SameLine();ImGui::BeginDisabled(!std::isfinite(regenDraft12[i]) || regenDraft12[i]<0 || regenDraft12[i]>1000);
            if(ImGui::Button("应用恢复率"))SetStat(rates[i],regenDraft12[i],0x14,false);
            ImGui::EndDisabled();ImGui::Text("基础恢复率读回：%.2f",view.regen12[i]);
            ImGui::PopID();ImGui::Separator();
        }
        Help("上限 1—1000000；生命目标至少 1 点；会产生负基础值的请求将拒绝。上限按当前装备 / 效果补偿，效果变化后仍会变化。修改对应资源会关闭其百万增强；设置当前比例或保持比例会关闭三项持续补满。正常恢复仍生效。");
        ImGui::Text("当前等级：%d",view.level12);
        ImGui::SetNextItemWidth(150);ImGui::InputInt("目标等级",&levelDraft12);
        ImGui::SameLine();ImGui::BeginDisabled(!custom12::ValidLevel(levelDraft12));
        if(ImGui::Button("应用等级"))Send(Op::PlayerLevel12,0,0,levelDraft12);
        ImGui::EndDisabled();
        Help("等级 1—65535；仅修改角色等级，不补发技能、Perk 或升级属性奖励，也不重置经验进度。恢复率还受战斗、延迟和其他效果影响。");
        ImGui::Text("负重上限读回：%.1f",view.carry12);
        ImGui::SetNextItemWidth(150);ImGui::InputFloat("目标负重上限",&carryDraft12,10,100,"%.1f");
        ImGui::SameLine();ImGui::BeginDisabled(!std::isfinite(carryDraft12) || carryDraft12<0 || carryDraft12>1000000);
        if(ImGui::Button("应用负重"))SetStat("carryweight",carryDraft12);
        ImGui::EndDisabled();ImGui::Separator();
    }
    void PlayerPage() {
        CustomPlayer12();
        ImGui::Text("生命 %.0f / %.0f | 魔力 %.0f / %.0f",view.health,view.maxHealth,view.magicka,view.maxMagicka);
        Check("超级血量：百万上限 + 持续恢复",view.superHealth,Op::SuperHealth);
        Check("无限魔力：百万魔力池 + 每帧补满",view.infiniteMagicka,Op::InfiniteMagicka);
        Check("持续补满生命 / 魔法 / 耐力",view.unlimited,Op::Unlimited);
        Help("高上限补满不会更改第三方法术脚本的独立消耗规则；直接无敌可使用下方 TGM。");
        if (ImGui::Button("切换原版上帝模式（TGM）")) Cmd("tgm");
        ImGui::SameLine(); if (ImGui::Button("立即回满生命")) Cmd("restoreav health 100000",0x14);
        Check("自由飞行：原版 TCL + Page Up/Down 垂直移动",view.flight,Op::Flight);
        Help("开启前请确保 TCL 关闭。飞行启用时不要在其他菜单重复切换 TCL。WASD/鼠标使用原版无碰撞移动；Page Up/Down 上下移动；按住 Shift 时垂直速度 ×3。");
        ImGui::SetNextItemWidth(130); ImGui::InputFloat("移动速度 %",&speed,10,100,"%.1f");
        ImGui::SameLine(); if (ImGui::Button("设置速度")) SetStat("speedmult",Finite(speed,100,1,10000));
        ImGui::Text("实时读回：速度 %.1f%% | 跑速缓存 %.1f",view.speedMult,view.runSpeed);
        if (ImGui::Button("恢复正常移动速度")) { speed=100; SetStat("speedmult",100); }
        ImGui::SetNextItemWidth(130); ImGui::InputFloat("跳跃高度",&jump,10,100,"%.1f");
        ImGui::SameLine(); if (ImGui::Button("设置跳跃")) Cmd(fmt::format("setgs fJumpHeightMin {}",Finite(jump,76,1,100000)));
        ImGui::Separator();
        ImGui::SetNextItemWidth(130); ImGui::InputInt("全部技能数值",&skillValue);
        ImGui::SameLine(); if (ImGui::Button("设置全部 18 项技能")) Send(Op::Skills,0,0,skillValue);
        if (ImGui::Button("添加全部有名称的 Perk")) Send(Op::AddAll,0x14,0,static_cast<int>(Kind::Perk));
        ImGui::SameLine(); if (ImGui::Button("添加全部有名称的法术")) Send(Op::AddAll,0x14,0,static_cast<int>(Kind::Spell));
        if (ImGui::Button("添加全部有名称的龙吼")) Send(Op::AddAll,0x14,0,static_cast<int>(Kind::Shout));
        ImGui::SameLine(); if (ImGui::Button("学习并解锁全部有名称的龙语")) Send(Op::AddAll,0x14,0,static_cast<int>(Kind::Word));
        Help("目录也可以显示无名称/内部记录。技能、Perk、龙吼和龙语是不同类型的记录；批量添加法术/Perk 可能包含脚本效果或不兼容内容。");
        ImGui::Separator();
        if (ImGui::Button("龙吼冷却倍率设为 0")) SetStat("shoutrecoverymult",0);
        ImGui::SameLine(); if (ImGui::Button("负重设为 100000")) SetStat("carryweight",100000);
        if (ImGui::Button("增加 100000 金币")) Cmd("additem 0000000F 100000",0x14);
        ImGui::SameLine(); if (ImGui::Button("显示全部地图标记")) Cmd("tmm 1");
        if (ImGui::Button("打开玩家捏脸菜单")) Cmd("showracemenu",0,true);
    }
    void HousePage() {
        Help("请把准星对准房屋外侧入口门，不要对墙或建筑外观。接管会沿着门的传送链接处理一个室内 Cell；多 Cell 建筑需要分别接管其他区域。");
        constexpr const char* modes[] = {"保留居民", "驱逐到室外入口", "禁用居民", "禁用并标记删除"};
        ImGui::Combo("居民处理方式",&eviction,modes,4);
        if (ImGui::Button("接管目标门连接的室内区域")) Send(Op::ClaimDoor,view.target.id,0,eviction);
        if (ImGui::Button("接管当前室内区域")) Send(Op::ClaimCell,0,0,eviction);
        ImGui::Separator();
        RefButton("绕过条件直接进入该门",Op::DoorTransit10);
        RefButton("接管 + 解锁 + 解除激活封锁",Op::Own); ImGui::SameLine(); RefButton("解锁目标",Op::Unlock);
        if(ImGui::Button("接管并使用目标（E键默认流程）"))Engine::Get().Submit(Action{.op=Op::OwnUse11,.target=view.target.id,.epoch=view.epoch});
        ImGui::SameLine();if(ImGui::Button("只走引擎默认激活"))Engine::Get().Submit(Action{.op=Op::OwnUse11,.target=view.target.id,.epoch=view.epoch,.scope=1});
        ImGui::Text("目标：锁=%s | 激活封锁=%s",view.target.locked?"是":"否",view.target.activationBlocked?"是":"否");
        Help("设为我的现在同步清锁和 BlockActivation；配对门还可点直接进入。脚本自行判断条件/无配对链接机关不伪装成已解除。");
        Help("接管后房屋区域名和入口名自动改成 keqing，面板/准星文字主动刷新。已经加载的床、家具、容器立即改归属；尚未实例化的内容在加载时处理。");
        Help("不会重命名整座城市或共享的门基础记录；多区域建筑可逐个接管室内区域。购房任务、剧情脚本不会被假装标记为已完成。");
        ImGui::Text("已接管室内区域：%zu",view.claimed);
        Help("驱逐会把角色移动到真实室外入口，并在你停留室内时继续检查闯入者。找不到室外门时请改用“禁用居民”。玩家和已控制随从不会被驱逐；任务关键 NPC 不会被自动排除。");
    }
    void ObjectPage() {
        ImGui::BeginDisabled(!view.target.id);
        RefButton("设为我的",Op::Own); ImGui::SameLine(); RefButton("解锁",Op::Unlock);
        ImGui::SameLine(); RefButton("上锁 100",Op::Lock,100);
        RefButton("启用",Op::Enable); ImGui::SameLine(); RefButton("禁用",Op::Disable);
        ImGui::SameLine(); RefButton("克隆基础对象",Op::Clone);
        ImGui::InputFloat3("移动增量 XYZ",move,"%.2f");
        if (ImGui::Button("移动目标")) Send(Op::Move,view.target.id,0,1,{}, {move[0],move[1],move[2],0});
        ImGui::InputFloat3("旋转增量 XYZ（度）",rotation,"%.2f");
        if (ImGui::Button("旋转目标")) Send(Op::Rotate,view.target.id,0,1,{}, {rotation[0],rotation[1],rotation[2],0});
        ImGui::InputFloat("缩放倍率",&scale,0.1f,1,"%.2f");
        if (ImGui::Button("设置缩放")) Send(Op::Scale,view.target.id,0,1,{}, {scale,0,0,0});
        RefButton("碰撞开启",Op::Collision,1); ImGui::SameLine(); RefButton("碰撞关闭",Op::Collision,0);
        ImGui::InputText("显示名称（UTF-8）",rename,sizeof(rename));
        if (ImGui::Button("重命名对象")) Send(Op::Rename,view.target.id,0,1,rename);
        RefButton("召到身边",Op::Summon); ImGui::SameLine(); RefButton("前往目标",Op::Goto);
        ImGui::Separator();
        if (ImGui::Button("彻底移除当前对象")) Send(Op::Banish,view.target.id);
        Help("移除先关闭碰撞并禁用，再标记删除和登记同引用抑制；不会删除整个基础模板。克隆不会复制独有库存、脚本状态或门链接。");
        ImGui::EndDisabled();
    }
    void ActorPage() {
        NearbyPicker();
        ImGui::BeginDisabled(!view.target.actor);
        RefButton("加入军团 / 跟随我",Op::Follow); ImGui::SameLine(); RefButton("等待",Op::Wait,1);
        ImGui::SameLine(); RefButton("继续跟随",Op::Wait,0); ImGui::SameLine(); RefButton("解除跟随",Op::Dismiss);
        RefButton("停止战斗",Op::StopCombat); ImGui::SameLine(); RefButton("召到身边",Op::Summon);
        ImGui::SameLine(); RefButton("前往该角色",Op::Goto);
        RefButton("AI 开启",Op::SetAI,1); ImGui::SameLine(); RefButton("AI 关闭",Op::SetAI,0);
        if (ImGui::Button("复活")) Cmd("resurrect 1",view.target.id);
        ImGui::SameLine(); RefButton("强制死亡（失败后移除）",Op::Kill,1);
        RefButton("仅强制死亡（保留尸体）",Op::Kill,0); ImGui::SameLine(); RefButton("彻底移除（不留尸体）",Op::Banish);
        Help("死亡后留下尸体是正常结果。第一项会解除保护、尝试原生死亡并读回；持续失败才改为移除。第三项直接禁用碰撞和显示，再抑制同引用被重新启用。");
        RefButton("强制抢回控制 / 退出固定动作",Op::ForceControl);
        ImGui::Text("关键=%s | 受保护=%s | 无敌模板=%s | 队友=%s | 坐姿=%s | 受制=%s",
            view.target.essential?"是":"否",view.target.protectedActor?"是":"否",view.target.invulnerable?"是":"否",
            view.target.teammate?"是":"否",view.target.sitting?"是":"否",view.target.restrained?"是":"否");
        ImGui::Text("当前包 %08X (%s) | 当前场景 %08X",view.target.packageID,view.target.packageName.c_str(),view.target.sceneID);
        if (ImGui::Button(view.target.pacified?"解除持续安抚":"持续停止该 NPC 战斗")) Send(Op::Pacify,view.target.id,0,view.target.pacified?0:1);
        ImGui::Text("状态：AI %s | 战斗 %s | 持续安抚 %s",view.target.ai?"开":"关",view.target.combat?"是":"否",view.target.pacified?"开":"关");
        if (ImGui::Button("设为关键角色（基础记录）")) Cmd("setessential " + Hex(view.target.base) + " 1");
        ImGui::SameLine(); if (ImGui::Button("取消关键角色（基础记录）")) Cmd("setessential " + Hex(view.target.base) + " 0");
        if (ImGui::Button("关系：盟友")) Cmd("setrelationshiprank 00000014 4",view.target.id);
        ImGui::SameLine(); if (ImGui::Button("关系：中立")) Cmd("setrelationshiprank 00000014 0",view.target.id);
        if (ImGui::Button("打开角色库存")) Cmd("openactorcontainer 1",view.target.id,true);
        ImGui::SameLine(); if (ImGui::Button("卸下全部装备")) Cmd("unequipall",view.target.id);
        Help("修改基础记录可能影响所有共享该基础模板的角色。库存、阵营、种族和套装控制也可在“目录”页使用。");
        ImGui::EndDisabled();
        ImGui::SeparatorText("已控制随从");
        for (const auto& r : view.followers) {
            ImGui::PushID(static_cast<int>(r.id));
            if (ImGui::SmallButton("选择")) Send(Op::Select,r.id);
            ImGui::SameLine(); if (ImGui::SmallButton(r.waiting?"继续":"等待")) Send(Op::Wait,r.id,0,r.waiting?0:1);
            ImGui::SameLine(); if (ImGui::SmallButton("解除跟随")) Send(Op::Dismiss,r.id);
            ImGui::SameLine(); ImGui::TextUnformatted(r.name.c_str());
            ImGui::Text("距离 %.0f | %s",r.distance,r.state.c_str());
            ImGui::PopID();
        }
        Check("快速脱困：卡住 5 秒后允许纠偏",view.forcedFollow,Op::FollowMode);
        Help("跟随使用独立任务别名 + 正式 Follow 程序；走路/跑步和绕障交给引擎。仅持续无进展、跨区或超远时才纠偏。默认脱困等待 12 秒，可在自由开关页调整；龙默认留在室外。");
        if (view.freeze) Help("当前全局 AI 冻结已开启；需在世界页关闭后随从才会行走。");
    }
    void ProfileEditor() {
        ImGui::InputFloat("生命上限",&memberProfile.health,100,1000,"%.0f");
        ImGui::InputFloat("魔力上限",&memberProfile.magicka,100,1000,"%.0f");
        ImGui::InputFloat("耐力上限",&memberProfile.stamina,100,1000,"%.0f");
        ImGui::InputFloat("物理攻击倍率",&memberProfile.damage,0.5f,5,"%.2f");
        ImGui::InputFloat("护甲值",&memberProfile.armor,100,1000,"%.0f");
        ImGui::InputFloat("魔法抗性 %",&memberProfile.resist,5,25,"%.0f");
        ImGui::InputFloat("移动速度 %",&memberProfile.speed,10,100,"%.0f");
        ImGui::Checkbox("入队后持续补满生命 / 魔力 / 耐力",&memberProfile.refill);
        Help("这些值作用于角色实例。攻击倍率不等于所有法术的伤害倍率；原版抗性/护甲上限仍由游戏处理。");
    }
    void LegionPage() {
        RuntimeStatus10();
        if(ImGui::CollapsingHeader("在身边生成 k 个新士兵",ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputInt("士兵数量 k",&soldierCount10);soldierCount10=std::clamp(soldierCount10,1,MaxSpawnBatch);
            ImGui::InputFloat("士兵分布半径",&soldierRadius10,50,300,"%.0f");
            ImGui::Checkbox("新士兵应用下方实力配置",&soldierProfile10);
            ImGui::BeginDisabled(!view.quest10 || view.spawnPending || view.births10);
            if(ImGui::Button("生成随机士兵并编入军团")) Engine::Get().Submit(Action{.op=Op::SpawnSoldiers10,.count=soldierCount10,.value={soldierRadius10,0,0,0},.epoch=view.epoch,.profile=memberProfile,.recruit=true,.applyProfile=soldierProfile10});
            ImGui::BeginDisabled(!spawnForm);
            if(ImGui::Button("使用生成器中选定模板生成兵团")) Engine::Get().Submit(Action{.op=Op::SpawnSoldiers10,.form=spawnForm,.count=soldierCount10,.value={soldierRadius10,0,0,0},.epoch=view.epoch,.profile=memberProfile,.recruit=true,.applyProfile=soldierProfile10});
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!view.target.actor || view.target.id==0x14 || !view.target.base);
            if(ImGui::Button("以当前所选 NPC 为模板生成部队"))Engine::Get().Submit(Action{.op=Op::SpawnSoldiers10,.form=view.target.base,.count=soldierCount10,.value={soldierRadius10,0,0,0},.epoch=view.epoch,.profile=memberProfile,.recruit=true,.applyProfile=soldierProfile10});
            ImGui::EndDisabled();ImGui::EndDisabled();
            ImGui::Text("选定模板：%08X（可在生物快速生成页改选；也可以选生物组成军团）",spawnForm);
            Help("上面的生成按钮会创建新实例；下面的加入军团只招募已有目标。随机士兵依据加载的 EncGuard / EncSoldier 类 EditorID 筛选，不会假装凭空登记一个士兵。");
            SpawnStatus10();
        }
        ImGui::SeparatorText("招募 / 控制现有角色");
        NearbyPicker();
        ImGui::BeginDisabled(!view.target.actor || view.target.id==0x14 || !view.quest10);
        RefButton("把所选角色加入我的军团",Op::Follow); ImGui::SameLine(); RefButton("强制抢回控制",Op::ForceControl);
        ImGui::EndDisabled();
        ImGui::Text("军团 %zu / %zu | 同引用移除抑制 %zu",view.followers.size(),MaxLegionMembers,view.suppressed);
        bool fight=view.autoFight,hard=view.hardControl,isolate=view.isolateFactions;
        bool changed=ImGui::Checkbox("自动护卫：攻击玩家敌人，禁止攻击军团成员",&fight);
        changed|=ImGui::Checkbox("强制接管：退出场景、坐姿和移动限制",&hard);
        changed|=ImGui::Checkbox("隔离成员原阵营（解除入队时尝试恢复）",&isolate);
        if(changed) Send(Op::LegionOptions,0,0,1,{}, {fight?1.0f:0.0f,hard?1.0f:0.0f,isolate?1.0f:0.0f,view.battleRadius});
        if(!legionSettingsLoaded || legionEpoch!=view.epoch) { battleRadius=view.battleRadius; legionSettingsLoaded=true; legionEpoch=view.epoch; }
        ImGui::InputFloat("自动迎敌范围（游戏单位）",&battleRadius,250,1000,"%.0f");
        ImGui::SameLine(); if(ImGui::Button("应用范围")) Send(Op::LegionOptions,0,0,1,{}, {fight?1.0f:0.0f,hard?1.0f:0.0f,isolate?1.0f:0.0f,battleRadius});
        if(view.peace || view.freeze) Help("当前世界和平 / AI 冻结优先于军团作战。请到世界控制关闭后再测试护卫。");
        Help("强制接管会分离角色正在执行的剧情场景，移除其原阵营；不会替你完成剧情。不会修改全世界的阵营关系。");
        if(ImGui::Button("全军跟随")) Send(Op::LegionOrder,0,0,0);
        ImGui::SameLine(); if(ImGui::Button("全军等待")) Send(Op::LegionOrder,0,0,1);
        ImGui::SameLine(); if(ImGui::Button("全军召回")) Send(Op::LegionOrder,0,0,2);
        if(ImGui::Button("全军停战 + 关闭自动迎敌")) Send(Op::LegionOrder,0,0,3);
        ImGui::BeginDisabled(!view.target.actor || view.target.id==0x14);
        ImGui::SameLine(); if(ImGui::Button("全军集火当前目标")) Send(Op::LegionOrder,view.target.id,0,4);
        ImGui::EndDisabled();
        if(view.focusEnemy) ImGui::Text("集火 RefID：%08X（死亡 / 消失后自动清除）",view.focusEnemy);
        Check("快速脱困：无进展 5 秒后允许纠偏",view.forcedFollow,Op::FollowMode);
        Help("正常跟随由独立别名行为包驱动，使用三档距离错开队列；不会每帧拉坐标。默认连续无进展 12 秒才脱困，正在正常战斗时不抢回跟随。引擎导航和特殊飞行限制仍存在。");
        ImGui::InputTextWithHint("##legionfilter","按成员名称 / RefID 搜索",legionFilter,sizeof(legionFilter));
        ImGui::BeginChild("legion-members",ImVec2(0,230),true);
        for(const auto& member:view.followers) {
            const auto label=fmt::format("{} [{:08X}]",member.name,member.id);
            if(!Contains(label,legionFilter)) continue;
            ImGui::PushID(static_cast<int>(member.id));
            if(ImGui::SmallButton("选择")) Send(Op::Select,member.id);
            ImGui::SameLine(); if(ImGui::SmallButton(member.waiting?"出发":"等待")) Send(Op::Wait,member.id,0,member.waiting?0:1);
            ImGui::SameLine(); if(ImGui::SmallButton("抢回")) Send(Op::ForceControl,member.id);
            ImGui::SameLine(); if(ImGui::SmallButton("解除")) Send(Op::Dismiss,member.id);
            ImGui::SameLine(); ImGui::TextUnformatted(label.c_str());
            ImGui::Text("距离 %.0f | %s",member.distance,member.state.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        if(ImGui::CollapsingHeader("自定义成员实力",ImGuiTreeNodeFlags_DefaultOpen)) {
            ProfileEditor();
            ImGui::BeginDisabled(!view.target.actor || view.target.id==0x14);
            if(ImGui::Button("应用到所选角色并补满")) Engine::Get().Submit(Action{.op=Op::LegionStats,.target=view.target.id,.count=0,.epoch=view.epoch,.profile=memberProfile});
            ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(view.followers.empty());
            if(ImGui::Button("应用到整个军团并补满")) Engine::Get().Submit(Action{.op=Op::LegionStats,.count=1,.epoch=view.epoch,.profile=memberProfile});
            ImGui::EndDisabled();
            Help("已登记成员会保存并维护该配置。非成员只立即应用一次；要持续维护，请先招募。");
        }
        if(ImGui::CollapsingHeader("解除全部成员")) {
            if(ImGui::Button("解除整个军团（不杀死、不删除）")) Send(Op::LegionOrder,0,0,5);
            Help("尝试恢复原阵营、AI 开关及接管前的角色标志；不会重演已经中断的任务场景，也不会撤销你手动设置的实力。");
        }
    }
    void SpawnActorsPage() {
        if(!catalog) return;
        if(actorCatalog!=catalog.get()) {
            actorSources.clear(); actorRaces.clear(); actorRows.clear();
            for(const auto& entry:*catalog) if(entry.kind==Kind::NPC || entry.kind==Kind::LeveledActor) {
                actorSources.push_back(entry.source);
                if(entry.kind==Kind::NPC) actorRaces.emplace_back(entry.race,entry.raceName.empty()?"未提供种族":entry.raceName);
            }
            std::sort(actorSources.begin(),actorSources.end()); actorSources.erase(std::unique(actorSources.begin(),actorSources.end()),actorSources.end());
            std::sort(actorRaces.begin(),actorRaces.end(),[](const auto& a,const auto& b){return a.first<b.first;});
            actorRaces.erase(std::unique(actorRaces.begin(),actorRaces.end(),[](const auto& a,const auto& b){return a.first==b.first;}),actorRaces.end());
            actorCatalog=catalog.get(); actorFilterDirty=true;
        }
        ImGui::TextWrapped("%s",view.catalogStatus10.c_str());
        constexpr const char* scopes10[]={"生物包专用清单（默认）","仅 DC / Demonic Creatures","相同种族扩展（包括原版）","全部 NPC / 等级列表"};
        actorFilterDirty|=ImGui::Combo("目录范围",&actorScope10,scopes10,4);
        if(ImGui::Button("只看 DC")){actorScope10=1;actorFilterDirty=true;actorSource.clear();actorRaceFilter=false;actorUnique=false;actorType=1;actorQuery[0]='\0';}
        ImGui::SameLine();if(ImGui::Button("恢复专用清单")){actorScope10=0;actorFilterDirty=true;actorSource.clear();actorRaceFilter=false;actorUnique=false;actorType=1;actorQuery[0]='\0';}
        actorFilterDirty|=ImGui::InputTextWithHint("搜索角色记录","名称 / EditorID / FormID / 种族 / 插件",actorQuery,sizeof(actorQuery));
        if(ImGui::BeginCombo("来源插件",actorSource.empty()?"所有插件":actorSource.c_str())) {
            if(ImGui::Selectable("所有插件",actorSource.empty())) { actorSource.clear(); actorFilterDirty=true; }
            for(const auto& source:actorSources) if(ImGui::Selectable(source.c_str(),actorSource==source)) { actorSource=source; actorFilterDirty=true; }
            ImGui::EndCombo();
        }
        const char* raceLabel="所有种族 / 等级列表";
        for(const auto& r:actorRaces) if(actorRaceFilter && actorRace==r.first) { raceLabel=r.second.c_str(); break; }
        if(ImGui::BeginCombo("种族",raceLabel)) {
            if(ImGui::Selectable("所有种族 / 等级列表",!actorRaceFilter)) { actorRaceFilter=false; actorFilterDirty=true; }
            for(const auto& r:actorRaces) {
                const auto label=fmt::format("{} [{:08X}]",r.second,r.first);
                if(ImGui::Selectable(label.c_str(),actorRaceFilter && actorRace==r.first)) { actorRaceFilter=true; actorRace=r.first; actorFilterDirty=true; }
            }
            ImGui::EndCombo();
        }
        actorFilterDirty|=ImGui::Checkbox("只看独有角色",&actorUnique); ImGui::SameLine();
        actorFilterDirty|=ImGui::Checkbox("隐藏捏脸预设记录",&hideActorPresets);
        const char* types[]={"NPC + 等级列表","仅 NPC / 生物","仅等级生物列表"};
        actorFilterDirty|=ImGui::Combo("记录类型",&actorType,types,3);
        if(ImGui::Button("重建当前加载目录")) Send(Op::RebuildCatalog);
        Help("默认使用随包附带的生物包 NPC_ 基础 ID 清单，按本次实际加载顺序解析。不是从全游戏 NPC 手动查找。DC 已单独分类；同名不同 ID 不合并，缺失/未启用插件不会生成假条目。");
        Help("清单来自生物插件的 NPC_ 记录，不把 Race、皮肤或动画 ID 当生成 ID；不是动画兼容认证。相同种族扩展会包含原版角色；预设记录默认隐藏，可手动放开查看。");
        if(actorFilterDirty) {
            actorRows.clear();
            for(std::size_t i=0;i<catalog->size();++i) {
                const auto& e=(*catalog)[i];
                if(e.kind!=Kind::NPC && e.kind!=Kind::LeveledActor) continue;
                if(actorScope10==0 && !e.creaturePack)continue;
                if(actorScope10==1 && !e.dc)continue;
                if(actorScope10==2 && !e.raceMatch)continue;
                if(ActorRecordMatches({e.leveled,e.unique,e.preset,e.race,e.source,e.search},
                    {actorType,actorUnique,hideActorPresets,actorRaceFilter,actorRace,actorSource,actorQuery})) actorRows.push_back(i);
            }
            actorFilterDirty=false;
        }
        ImGui::Text("匹配角色记录：%zu",actorRows.size());
        ImGui::BeginChild("actor-base-list",ImVec2(0,std::clamp(ImGui::GetIO().DisplaySize.y*0.27f,160.0f,360.0f)),true);
        ImGuiListClipper clipper; clipper.Begin(static_cast<int>(actorRows.size()));
        while(clipper.Step()) for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto& e=(*catalog)[actorRows[static_cast<std::size_t>(row)]];
            auto label=fmt::format("{} | {} [{}]{}###actor-{}",e.name.empty()?(e.editor.empty()?"<无名称>":e.editor):e.name,
                e.packTags.empty()?e.source:e.packTags,Hex(e.id),e.leveled?" [等级列表]":"",Hex(e.id));
            if(ImGui::Selectable(label.c_str(),spawnForm==e.id)) { spawnForm=e.id; std::snprintf(spawnFormText,sizeof(spawnFormText),"%08X",e.id); }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("EditorID: %s\n种族: %s [%08X]\n定义来源: %s\n涉及生物包: %s\n独有角色: %s | 捏脸预设: %s | 附带脚本: %s",e.editor.c_str(),e.raceName.c_str(),e.race,e.source.c_str(),e.packTags.c_str(),e.unique?"是":"否",e.preset?"是":"否",e.scripted?"是":"否");
        }
        ImGui::EndChild();
        if(ImGui::InputText("生成基础 FormID（不是现有角色 RefID）",spawnFormText,sizeof(spawnFormText))) spawnForm=ParseID(spawnFormText).value_or(0);
        ImGui::InputInt("本次数量（1—256）",&spawnCount); spawnCount=std::clamp(spawnCount,1,MaxSpawnBatch);
        ImGui::InputFloat("当前位置分布半径（0 = 同一点）",&spawnRadius,50,500,"%.0f");
        ImGui::Checkbox("生成后自动加入我的军团",&spawnRecruit); ImGui::SameLine();
        ImGui::Checkbox("应用自定义实力",&spawnProfile);
        if(spawnProfile && ImGui::CollapsingHeader("生成角色的实力配置")) ProfileEditor();
        ImGui::BeginDisabled(!spawnForm || view.spawnPending>0 || view.births10>0 || (spawnRecruit && !view.quest10));
        if(ImGui::Button("在当前位置附加生成新角色")) Engine::Get().Submit(Action{.op=Op::SpawnActors,.form=spawnForm,.count=spawnCount,
            .value={spawnRadius,0,0,0},.epoch=view.epoch,.profile=memberProfile,.recruit=spawnRecruit,.applyProfile=spawnProfile});
        ImGui::EndDisabled();
        SpawnStatus10();
        if(spawnRecruit) RuntimeStatus10();
        if(ImGui::CollapsingHeader("管理本工具生成的实例")) {
            ImGui::Text("已登记 %zu 个实例",view.spawned.size());
            ImGui::BeginChild("spawned-refs",ImVec2(0,180),true);
            ImGuiListClipper refs; refs.Begin(static_cast<int>(view.spawned.size()));
            while(refs.Step()) for(int i=refs.DisplayStart;i<refs.DisplayEnd;++i) {
                const auto& r=view.spawned[static_cast<std::size_t>(i)];
                const auto label=fmt::format("{} [{:08X}]",r.name,r.id);
                if(ImGui::Selectable(label.c_str(),view.target.id==r.id)) Send(Op::Select,r.id);
            }
            ImGui::EndChild();
            if(ImGui::Button("彻底移除这些生成实例（不处理原地图角色）")) Send(Op::ClearSpawned);
        }
    }
    void RandomEnemiesPage10() {
        Help("独立随机敌人生成：每个新实例分别从筛选池抽取，允许抽到同种。不会移动原有 NPC；只给新生成实例设置敌对。");
        ImGui::TextWrapped("%s",view.catalogStatus10.c_str());
        constexpr const char* pools[]={"生物包专用清单","仅 DC","同种族扩展","全部 NPC 基础记录"};
        ImGui::Combo("随机池",&enemyScope10,pools,4);
        ImGui::InputTextWithHint("筛选随机池","名称 / 种族 / DC / 插件",enemyQuery10,sizeof(enemyQuery10));
        ImGui::Checkbox("随机池排除独有角色和已知附带脚本角色",&enemySafe10);
        ImGui::InputInt("随机敌人数",&enemyCount10);enemyCount10=std::clamp(enemyCount10,1,MaxSpawnBatch);
        ImGui::InputFloat("敌人分布半径",&enemyRadius10,100,500,"%.0f");
        ImGui::InputTextWithHint("随机种子","留空 / 0 = 每次随机；十进制整数可复现",enemySeed10,sizeof(enemySeed10));
        ImGui::Checkbox("随机敌人应用自定义实力",&enemyProfile10);
        if(enemyProfile10)ProfileEditor();
        const auto parsed=ParseSeed10(enemySeed10);
        if(!parsed)Help("种子格式无效：需要 0 到 18446744073709551615 的十进制整数。");
        if(view.peace || view.freeze)Help("当前和平 / AI 冻结会阻止新敌人正常作战。关闭相应开关后再测试。");
        if(view.aura10 || view.emptyWorld10)Help("清空领域开启时会拒绝生成敌人，避免刚生成就被删除。");
        ImGui::BeginDisabled(!parsed || view.spawnPending || view.births10 || !view.runtime10);
        if(ImGui::Button("生成随机敌人：自动关面板后执行")) Engine::Get().Submit(Action{.op=Op::SpawnRandom10,.count=enemyCount10,.value={enemyRadius10,0,0,0},.text=enemyQuery10,.epoch=view.epoch,.playableOnly=enemySafe10,.profile=memberProfile,.recruit=false,.applyProfile=enemyProfile10,.enemy=true,.scope=enemyScope10,.seed=parsed.value_or(0)});
        ImGui::EndDisabled();
        SpawnStatus10();
    }
    void FreedomPage10() {
        SyncFreedom10();
        ImGui::SeparatorText("旅行 / 进门 / 操控");
        Help("自由旅行不经过原版旅行条件判断；不是随意屏蔽所有引擎检测。用下面入口可在战斗中、负重时直接旅行或进配对门。");
        if(ImGui::Button("打开自由旅行列表")){activePage=6;Send(Op::RefreshTravel10);}
        ImGui::SameLine();RefButton("直接进入所选配对门",Op::DoorTransit10);
        bool keep=view.keepTravel10;
        if(ImGui::Checkbox("持续清除脚本对快速旅行的禁用标记",&keep))Send(Op::FreedomSettings10,0,0,keep?1:0,{}, {stuckUI10,0,0,0});
        Help("上面开关仅处理 EnableFastTravel 标记。原版地图的敌人 / 室内等其他条件仍可能存在；独立旅行列表不依赖它们。");
        if(ImGui::Button("释放玩家控制 / 退出家具与场景占用"))Send(Op::RescuePlayer10);
        Help("释放控制可能中断剧情表演。脚本持续重设的限制需要针对其具体来源处理，不会偷偷停止全游戏任务。");
        ImGui::SeparatorText("移动 / 呼吸 / 冷却");
        Check("自由飞行与穿墙",view.flight,Op::Flight);
        if(ImGui::Button("玩家水下呼吸开启"))SetStat("waterbreathing",1);
        ImGui::SameLine();if(ImGui::Button("水下呼吸恢复"))SetStat("waterbreathing",0);
        if(ImGui::Button("负重设为 1000000"))SetStat("carryweight",1000000);
        ImGui::SameLine();if(ImGui::Button("龙吼冷却倍率设为 0"))SetStat("shoutrecoverymult",0);
        Check("持续补满生命 / 魔力 / 耐力",view.unlimited,Op::Unlimited);
        ImGui::SeparatorText("自然跟随 / 脱困");
        ImGui::InputFloat("连续无进展多少秒后脱困",&stuckUI10,1,5,"%.1f");
        if(ImGui::Button("应用脱困等待时间"))Send(Op::FreedomSettings10,0,0,view.keepTravel10?1:0,{}, {stuckUI10,0,0,0});
        ImGui::Text("当前 %.1f 秒；可设 5—120 秒",view.stuckDelay10);
        Check("快速脱困覆盖：改用 5 秒",view.forcedFollow,Op::FollowMode);
        Help("自然跟随仍使用引擎导航，允许正常战斗与走跑切换；不会用每帧瞬移假装灵活跟随。回收距离与飞行速度在设置页。");
    }
    void KeqingPage10() {
        SyncFreedom10();
        ImGui::SeparatorText("keqing · 净域");
        Help("移除的是角色实例，不是基础 NPC 模板。不会伤害玩家自身；没有尸体。删除关键角色可能使任务无法继续。关闭领域不自动复活已移除角色。");
        ImGui::InputFloat("清理半径（游戏单位）",&clearRadiusUI10,250,1000,"%.0f");
        ImGui::Checkbox("保留我的军团 / 队友 / 正在入队的实例",&protectUI10);
        if(ImGui::Button("保存半径和保护选项"))ClearSettings10(view.aura10,view.emptyWorld10);
        ImGui::SameLine();if(ImGui::Button("施放一次：清空周围"))Engine::Get().Submit(Action{.op=Op::ClearPulse10,.value={clearRadiusUI10,0,0,0},.epoch=view.epoch,.protectArmy=protectUI10});
        bool aura=view.aura10;
        if(ImGui::Checkbox("持续净域：新进入半径的角色也移除",&aura))ClearSettings10(aura,view.emptyWorld10);
        ImGui::BeginDisabled(!view.runtime10);
        if(ImGui::Button("学习 keqing - Clear Zone 小能力"))Send(Op::LearnPower10);
        ImGui::EndDisabled();
        Help("小能力使用本页已保存的半径和军团保护选项。学习后在魔法菜单装备，按龙吼键施放；F8 内的施放按钮不要求装备。");
        ImGui::Text("领域：%s | 全局后续清理：%s | 队列已处理 %zu / %zu | 余 %zu",view.aura10?"开":"关",view.emptyWorld10?"开":"关",view.clearDone10,view.clearTotal10,view.clearPending10);
        if(ImGui::Button("停止全部领域和剩余清理队列"))Send(Op::ClearCancel10);
        if(ImGui::CollapsingHeader("全局清空（影响剧情和人口）")) {
            Help("全局一次清理处理当前内存里已有的角色引用，分帧执行。持续全局模式还会处理之后加载的角色；不会把从未实例化的整个世界伪称为一瞬间已删除。");
            ImGui::InputTextWithHint("确认","输入 CLEAR 解锁全局操作",clearConfirm10,sizeof(clearConfirm10));
            ImGui::BeginDisabled(std::string_view(clearConfirm10)!="CLEAR");
            if(ImGui::Button("全局现有引用：一次清空"))Engine::Get().Submit(Action{.op=Op::ClearWorld10,.epoch=view.epoch,.protectArmy=protectUI10});
            if(ImGui::Button("开启持续全局无人模式"))ClearSettings10(false,true);
            ImGui::EndDisabled();
            Help("保护军团开启时，你的部队仍保留。持续模式保存于本工具的 SKSE 存档数据，取消按钮随时可停。");
        }
    }
    void WorldPage() {
        Check("和平模式：压制侦测并持续停止现有战斗",view.peace,Op::Peace);
        Check("关闭全局侦测（不只针对玩家）",view.ignore,Op::Ignore);
        Check("冻结当前处理中的角色 AI（包含新加载角色）",view.freeze,Op::FreezeAI);
        Help("和平模式不能拦截所有任务强制攻击脚本、陷阱、投射物或伤害事件；未加载角色本来也不会模拟战斗。");
        if (ImGui::Button("立即停止附近全部 NPC 战斗")) Send(Op::StopNearbyCombat);
        if (ImGui::Button("切换原版战斗 AI（TCAI）")) Cmd("tcai");
        ImGui::SameLine(); if (ImGui::Button("清除全部阵营赏金")) Send(Op::ClearBounties);
        ImGui::Separator();
        ImGui::InputFloat("时间（0-24）",&hour,1,6,"%.2f");
        if (ImGui::Button("设置时间")) Cmd(fmt::format("set gamehour to {}",Finite(hour,12,0,23.999f)));
        ImGui::InputFloat("时间倍率（0=冻结日历）",&timeScale,1,10,"%.2f");
        if (ImGui::Button("设置时间倍率")) Cmd(fmt::format("set timescale to {}",Finite(timeScale,20,0,10000)));
        ImGui::InputFloat("游戏速度倍率",&worldSpeed,0.1f,1,"%.2f");
        if (ImGui::Button("设置游戏速度")) Cmd(fmt::format("sgtm {}",Finite(worldSpeed,1,0.05f,20)));
        ImGui::InputFloat("月份（0-11）",&month,1,1,"%.0f");
        if (ImGui::Button("设置月份")) Cmd(fmt::format("set gamemonth to {}",static_cast<int>(Finite(month,0,0,11))));
        Help("月份修改并非直接控制 Seasons of Skyrim；天气记录可在“目录”中选择。");
        if (ImGui::Button("解除强制天气")) Send(Op::ReleaseWeather);
    }
    void CatalogPage() {
        if (!catalog) { Help("目录尚未准备完成。"); return; }
        if (ImGui::Button("全服装")) category=static_cast<int>(Kind::Armor);
        ImGui::SameLine(); if (ImGui::Button("全武器")) category=static_cast<int>(Kind::Weapon);
        ImGui::SameLine(); if (ImGui::Button("全部法术")) category=static_cast<int>(Kind::Spell);
        ImGui::SameLine(); if (ImGui::Button("龙吼")) category=static_cast<int>(Kind::Shout);
        ImGui::Combo("分类",&category,KindNames,static_cast<int>(Kind::Count));
        if (lastCategory!=-1 && lastCategory!=category) {
            selectedForm=0; formID[0]='\0'; Send(Op::Inspect,0,0);
        }
        ImGui::InputTextWithHint("搜索","名称 / EditorID / FormID",filter,sizeof(filter));
        if (lastCatalog!=catalog.get()) {
            sources.clear();
            for (const auto& e:*catalog) sources.push_back(e.source);
            std::sort(sources.begin(),sources.end()); sources.erase(std::unique(sources.begin(),sources.end()),sources.end());
        }
        if (ImGui::BeginCombo("来源插件",sourceFilter.empty()?"所有来源":sourceFilter.c_str())) {
            if (ImGui::Selectable("所有来源",sourceFilter.empty())) sourceFilter.clear();
            for (const auto& source:sources) if (ImGui::Selectable(source.c_str(),sourceFilter==source)) sourceFilter=source;
            ImGui::EndCombo();
        }
        ImGui::Checkbox("包含无名称 / 内部记录",&includeUnnamed); ImGui::SameLine();
        ImGui::Checkbox("排除不可玩装备",&playableOnly);
        if (lastCatalog!=catalog.get() || lastFilter!=filter || lastCategory!=category || lastUnnamed!=includeUnnamed || lastSource!=sourceFilter || lastPlayable!=playableOnly) {
            filtered.clear();
            for (std::size_t i=0;i<catalog->size();++i) {
                const auto& e=(*catalog)[i];
                if (static_cast<int>(e.kind)==category && (includeUnnamed || !e.name.empty()) && Contains(e.search,filter) &&
                    (sourceFilter.empty() || e.source==sourceFilter) && (!playableOnly || e.playable)) filtered.push_back(i);
            }
            lastCatalog=catalog.get(); lastFilter=filter; lastCategory=category; lastUnnamed=includeUnnamed;
            lastSource=sourceFilter; lastPlayable=playableOnly;
        }
        ImGui::Text("匹配 %zu / 总记录 %zu",filtered.size(),catalog->size());
        ImGui::BeginChild("catalog-list",ImVec2(0,std::clamp(ImGui::GetIO().DisplaySize.y*0.23f,160.0f,360.0f)),true);
        ImGuiListClipper clipper; clipper.Begin(static_cast<int>(filtered.size()));
        while (clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto& e=(*catalog)[filtered[static_cast<std::size_t>(row)]];
            const auto label=(e.name.empty()?(e.editor.empty()?"<无名称>":e.editor):e.name)+" | "+e.source+" ["+Hex(e.id)+"]";
            if (ImGui::Selectable(label.c_str(),e.id==selectedForm)) {
                selectedForm=e.id; std::snprintf(formID,sizeof(formID),"%08X",e.id); Send(Op::Inspect,0,e.id);
            }
        }
        ImGui::EndChild();
        if (ImGui::InputText("基础 FormID",formID,sizeof(formID))) if (auto id=ParseID(formID)) { selectedForm=*id; Send(Op::Inspect,0,*id); }
        const auto parsed=ParseID(formID);
        ImGui::InputInt("每条数量（装备一般 1，弹药可增大）",&amount);
        const Kind kind=static_cast<Kind>(category);
        const bool canAdd=kind==Kind::Armor || kind==Kind::Weapon || kind==Kind::Spell || kind==Kind::Perk || kind==Kind::Shout || kind==Kind::Word ||
            kind==Kind::Ammo || kind==Kind::Book || kind==Kind::Potion || kind==Kind::Scroll || kind==Kind::Misc || kind==Kind::Key || kind==Kind::Ingredient || kind==Kind::SoulGem || kind==Kind::Light;
        if (view.inspected==selectedForm) ImGui::Text("玩家当前：%s | 库存数量 %d（实时读回）",view.inspectedKnown?"已拥有 / 已学习":"未拥有 / 未学习",view.inspectedCount);
        ImGui::BeginDisabled(!parsed);
        if (canAdd) {
            if (ImGui::Button("给玩家添加 / 学习")) Send(Op::Add,0x14,selectedForm,amount);
            ImGui::SameLine(); if (ImGui::Button("从玩家移除")) Send(Op::Remove,0x14,selectedForm,amount);
            ImGui::BeginDisabled(!view.target.actor);
            if (ImGui::Button("给目标角色添加 / 学习")) Send(Op::Add,view.target.id,selectedForm,amount);
            if (kind==Kind::Armor || kind==Kind::Weapon || kind==Kind::Ammo) {
                ImGui::SameLine(); if (ImGui::Button("给目标装备")) Send(Op::Equip,view.target.id,selectedForm);
            }
            ImGui::EndDisabled();
        }
        if (kind==Kind::NPC || kind==Kind::Container || kind==Kind::Furniture || kind==Kind::Armor || kind==Kind::Weapon)
            if (ImGui::Button("在身边生成")) Send(Op::Spawn,0,selectedForm,amount);
        if (kind==Kind::Weather && ImGui::Button("应用天气")) Send(Op::Weather,0,selectedForm);
        ImGui::BeginDisabled(!view.target.actor);
        if (kind==Kind::Faction) {
            if (ImGui::Button("加入阵营")) Send(Op::AddFaction,view.target.id,selectedForm,amount);
            ImGui::SameLine(); if (ImGui::Button("退出阵营")) Send(Op::RemoveFaction,view.target.id,selectedForm);
        }
        if (kind==Kind::Race && ImGui::Button("设置目标种族")) Send(Op::SetRace,view.target.id,selectedForm);
        if (kind==Kind::Outfit && ImGui::Button("设置目标套装（保留库存）")) Send(Op::Outfit,view.target.id,selectedForm);
        ImGui::EndDisabled();
        if (kind==Kind::Quest && ImGui::Button("查看任务")) { Send(Op::QuestSelect11,0,selectedForm); std::snprintf(questID,sizeof(questID),"%08X",selectedForm); activePage=8; }
        ImGui::EndDisabled();
        ImGui::SeparatorText("批量解锁 / 入库");
        ImGui::Checkbox("只补缺失（已有物品补到指定数量，已学能力跳过）",&onlyMissing);
        ImGui::BeginDisabled(!canAdd || view.pending>0);
        if (ImGui::Button("应用当前筛选：全部添加给玩家")) Batch(category,filter,includeUnnamed);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(view.pending>0);
        if (ImGui::Button("全部库存类别：按当前来源和关键词入库")) Batch(-1,filter,includeUnnamed);
        ImGui::EndDisabled();
        if (ImGui::Button("取消剩余批量任务")) Send(Op::CancelBatch);
        Help("批量按帧分段处理，不重复排队。内部记录可手动放开筛选；非库存类（天气/任务等）不会伪装成可添加物品。添加龙吼会一并学习并解锁其关联龙语。");
    }

    void TeleportPage() {
        Help("本页通过对象 / 书签直接移动，不调用原版快速旅行许可。敌人附近、负重等不作为本工具的拒绝条件。仍会执行目标区域的正常加载。");
        RefButton("直接穿过配对门（绕过上锁 / 激活条件）",Op::DoorTransit10);
        if(ImGui::CollapsingHeader("自由地图旅行：可搜索地点",ImGuiTreeNodeFlags_DefaultOpen)) {
            if(ImGui::Button("读取当前可解析地图标记")) Send(Op::RefreshTravel10);
            ImGui::InputTextWithHint("##travel10","地点名称 / RefID",travelQuery10,sizeof(travelQuery10));
            ImGui::Text("已索引 %zu 个地点",view.travel10.size());
            ImGui::BeginChild("travel-list10",ImVec2(0,240),true);
            for(const auto& r:view.travel10){
                const auto label=fmt::format("{} [{:08X}]",r.name,r.id);
                if(!Contains(label,travelQuery10))continue;
                if(ImGui::Selectable(label.c_str(),false))Send(Op::Goto,r.id);
            }
            ImGui::EndChild();
            Help("点击地点直接传送。未实例化的地图标记可能不在列表中；亦可使用下方书签、对象 RefID、Cell EditorID。没有配对传送数据的脚本门不做盲目跳转。");
        }
        ImGui::InputText("书签名称",bookmarkName,sizeof(bookmarkName));
        if (ImGui::Button("保存当前位置")) Send(Op::SaveBookmark,0,0,1,bookmarkName);
        ImGui::SameLine(); if (ImGui::Button("返回 / 与上一个位置互换")) Send(Op::Return);
        RefButton("前往所选对象",Op::Goto); ImGui::SameLine(); RefButton("把所选对象召到这里",Op::Summon);
        ImGui::Separator();
        for (const auto& b:view.bookmarks) {
            ImGui::PushID(static_cast<int>(b.id));
            if (ImGui::SmallButton("前往")) Send(Op::GoBookmark,0,b.id);
            ImGui::SameLine(); if (ImGui::SmallButton("移除")) Send(Op::RemoveBookmark,0,b.id);
            ImGui::SameLine(); ImGui::TextUnformatted(b.name.c_str()); ImGui::PopID();
        }
        ImGui::InputText("用于 COC 的 Cell EditorID",cellEditor,sizeof(cellEditor));
        if (ImGui::Button("传送到 Cell 中心")) if (Identifier(cellEditor)) Cmd(std::string("coc ")+cellEditor,0,true);
        Help("COC 需要有效 EditorID，不是显示名称。有些 Mod 会在运行时移除 EditorID。书签和对象传送会保留返回点，直接 COC 不会。");
    }
    void StatsPage() {
        ImGui::InputText("角色属性名称",actorValue,sizeof(actorValue));
        ImGui::InputFloat("数值",&statValue,1,100,"%.3f");
        if (ImGui::Button("设置玩家基础值")) if (Identifier(actorValue)) SetStat(actorValue,Finite(statValue,0,-1000000,1000000),0x14,false);
        ImGui::SameLine(); if (ImGui::Button("设置目标基础值")) if (Identifier(actorValue)) SetStat(actorValue,Finite(statValue,0,-1000000,1000000),view.target.id,false);
        if (ImGui::Button("强制目标数值")) if (Identifier(actorValue)) SetStat(actorValue,Finite(statValue,0,-1000000,1000000),view.target.id,true);
        Help("示例：health、magicka、stamina、carryweight、speedmult、attackdamagemult、damageresist、magicresist、aggression、confidence、shoutrecoverymult、healrate、magickarate、staminarate。");
        ImGui::SeparatorText("阵营");
        ImGui::InputText("阵营 A FormID",factionA,sizeof(factionA));
        ImGui::InputText("阵营 B FormID",factionB,sizeof(factionB));
        auto left=ParseID(factionA),right=ParseID(factionB);
        if (ImGui::Button("A 与 B：盟友")) if (left&&right) Send(Op::FactionRelation,*left,*right,0);
        ImGui::SameLine(); if (ImGui::Button("A 与 B：敌对")) if (left&&right) Send(Op::FactionRelation,*left,*right,1);
        Help("阵营关系修改会全局影响阵营成员。任意角色属性或控制台命令可能被游戏拒绝；提交命令不等于保证成功。");
    }
    void QuestPage() {
        ImGui::SeparatorText("任务中心 / 自动列出 ID");
        Help("直接搜索任务中文名、EditorID、十六进制 ID 或来源插件；无需去物品页找 ID。顶部优先显示追踪/运行任务。");
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##quest-search11","搜索所有已加载任务…",questQuery11,sizeof(questQuery11));
        constexpr const char* filters[]={"全部任务","正在运行","当前追踪","已完成","已拒接"};
        ImGui::Combo("任务筛选",&questFilter11,filters,5);
        ImGui::SameLine();if(ImGui::Button("刷新任务列表"))Send(Op::QuestRefresh11);
        ImGui::SameLine();if(ImGui::Button("自己创建任务"))activePage=16;
        std::vector<const QuestRow11*> found;
        if(view.quests11)for(const auto& r:*view.quests11) {
            bool show=questFilter11==0||(questFilter11==1&&r.running)||(questFilter11==2&&r.active)||(questFilter11==3&&r.complete)||(questFilter11==4&&r.blocked);
            if(show&&Contains(r.search,questQuery11))found.push_back(&r);
        }
        ImGui::Text("匹配 %zu | 当前加载 %zu",found.size(),view.quests11?view.quests11->size():0);
        if(ImGui::BeginTable("quest-list11",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,ImVec2(0,265))) {
            ImGui::TableSetupColumn("FormID");ImGui::TableSetupColumn("任务名称 / EditorID");ImGui::TableSetupColumn("状态 / 阶段");ImGui::TableSetupColumn("来源插件");ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(static_cast<int>(found.size()));
            while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto& r=*found[i];ImGui::PushID(static_cast<int>(r.id));ImGui::TableNextRow();ImGui::TableNextColumn();
                if(ImGui::Selectable(Hex(r.id).c_str(),view.quest11.row.id==r.id,ImGuiSelectableFlags_SpanAllColumns))Send(Op::QuestSelect11,0,r.id);
                ImGui::TableNextColumn();ImGui::TextUnformatted(r.name.c_str());
                ImGui::TableNextColumn();ImGui::Text("%s / %d",r.blocked?"拒接":(r.complete?"完成":(r.running?"运行":"未运行")),r.stage);
                ImGui::TableNextColumn();ImGui::TextUnformatted(r.source.c_str());ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if(!view.quests11)Send(Op::QuestRefresh11);
        if(ImGui::CollapsingHeader("直接输入任务 ID（可选）")) {
            ImGui::InputText("任务 FormID",questID,sizeof(questID));ImGui::SameLine();
            if(ImGui::Button("按 ID 打开任务"))if(auto id=ParseID(questID))Send(Op::QuestSelect11,0,*id);
        }
        const auto& detail=view.quest11;const auto& r=detail.row;
        if(!detail.valid) {Help("请点击上面的任务行。没有名称的内部任务也可以用 EditorID/ID 搜索。");return;}
        if(questUI11!=r.id||questEditEpoch11!=view.epoch) {
            questUI11=r.id;questEditEpoch11=view.epoch;stage=r.stage;objectiveEdit11=-1;
            std::snprintf(questTitle11,sizeof(questTitle11),"%s",r.name.c_str());questGoalText11[0]=0;
        }
        ImGui::SeparatorText("所选任务");
        ImGui::TextWrapped("%s | %08X | %s",r.name.c_str(),r.id,r.editor.c_str());
        ImGui::Text("运行=%s | 追踪=%s | 完成=%s | 当前阶段=%d",r.running?"是":"否",r.active?"是":"否",r.complete?"是":"否",r.stage);
        if(ImGui::Button("复制 FormID"))ImGui::SetClipboardText(Hex(r.id).c_str());
        ImGui::SameLine();if(ImGui::Button("复制 EditorID"))ImGui::SetClipboardText(r.editor.c_str());
        if(r.personal){Help("这是你的自建任务，进自建任务页面改变状态和目标。");if(ImGui::Button("打开自建任务工作台"))activePage=16;return;}
        ImGui::BeginDisabled(r.protectedRuntime);
        if(ImGui::Button("开始 / 接取"))Send(Op::QuestStart,0,r.id);
        ImGui::SameLine();if(ImGui::Button("停止这条任务"))Send(Op::QuestStop,0,r.id);
        ImGui::SameLine();if(ImGui::Button(r.blocked?"解除拒接":"拒接 / 持续停止"))Send(Op::QuestBlock11,0,r.id,r.blocked?0:1);
        if(ImGui::Button(r.active?"取消追踪":"追踪此任务"))Send(Op::QuestTrack11,0,r.id,r.active?0:1);
        ImGui::SameLine();if(ImGui::Button("标记任务完成"))Send(Op::QuestComplete,0,r.id);
        ImGui::InputInt("阶段号（十进制）",&stage);
        ImGui::SameLine();if(ImGui::Button("设置阶段"))Send(Op::QuestStage,0,r.id,std::clamp(stage,0,65535));
        if(ImGui::BeginCombo("引擎可读阶段","选择一个已有阶段")) {
            for(int s:detail.stages)if(ImGui::Selectable(std::to_string(s).c_str(),stage==s))stage=s;
            ImGui::EndCombo();
        }
        Help("阶段列表是引擎当前可枚举的阶段，不伪装成完整剧情攻略。停止/重置不会撤销已发奖励和脚本后果。");
        if(ImGui::CollapsingHeader("改任务文字 / 重置 / 控制台查询")) {
            ImGui::InputText("任务显示名称",questTitle11,sizeof(questTitle11));
            if(ImGui::Button("应用显示名称"))Engine::Get().Submit(Action{.op=Op::QuestText11,.form=r.id,.text=questTitle11,.epoch=view.epoch,.scope=-1});
            if(ImGui::Button("原生重置此任务"))Send(Op::QuestReset,0,r.id);
            ImGui::SameLine();if(ImGui::Button("SQS（原版控制台结果）"))Cmd("sqs "+Hex(r.id));
            ImGui::SameLine();if(ImGui::Button("SQV（原版控制台结果）"))Cmd("sqv "+Hex(r.id));
        }
        ImGui::SeparatorText("任务目标 / 可编辑");
        constexpr const char* objectiveStates[]={"隐藏","进行中","已完成","完成并显示","已失败","失败并显示"};
        for(const auto& o:detail.goals) {
            ImGui::PushID(o.index);ImGui::TextWrapped("[%d] %s  (%s)",o.index,o.text.c_str(),o.state>=0&&o.state<6?objectiveStates[o.state]:"未知状态");
            auto goal=[&](const char* label,int action) {if(ImGui::SmallButton(label))Engine::Get().Submit(Action{.op=Op::QuestObjective11,.form=r.id,.count=action,.epoch=view.epoch,.scope=o.index});};
            goal("显示",1);ImGui::SameLine();goal("隐藏",0);ImGui::SameLine();goal("完成",2);ImGui::SameLine();goal("重新进行",3);ImGui::SameLine();goal("失败",4);ImGui::SameLine();
            if(ImGui::SmallButton("编辑文字")) {objectiveEdit11=o.index;std::snprintf(questGoalText11,sizeof(questGoalText11),"%s",o.text.c_str());}
            ImGui::PopID();
        }
        if(objectiveEdit11>=0) {
            ImGui::InputTextMultiline("目标文本",questGoalText11,sizeof(questGoalText11),ImVec2(-1,90));
            if(ImGui::Button("保存目标文字"))Engine::Get().Submit(Action{.op=Op::QuestText11,.form=r.id,.text=questGoalText11,.epoch=view.epoch,.scope=objectiveEdit11});
        }
        ImGui::EndDisabled();
        if(r.protectedRuntime)Help("这是军团内部任务，只读显示。用军团页面控制随从，避免自己关闭控制器。");
        if(!detail.feedback.empty())Help(detail.feedback.c_str());
    }
    void KernelPage11() {
        ImGui::SeparatorText("自由内核14 / 每项都可单独关闭");
        if(ImGui::Button("一键自由（保留主动交互）"))Send(Op::KernelPreset11,0,0,1);
        if(ImGui::Button("强自由（另解除家具占用）"))Send(Op::KernelPreset11,0,0,2);
        ImGui::SameLine();if(ImGui::Button("关闭持续守护"))Send(Op::KernelPreset11,0,0,0);
        Help("默认拒绝非主动对话和玩家场景接管；保留主动激活该 NPC 的会话。可能影响依赖强制对话的剧情，需要时临时放行或关闭。");
        auto r=view.rules11;bool changed=false;
        auto box=[&](const char* name,bool& b){changed=ImGui::Checkbox(name,&b)||changed;};
        ImGui::SeparatorText("守卫 / 犯罪 / 监禁");
        box("不增加赏金，并清理旧赏金",r.noBounty);
        box("禁止原生监禁、罚款和没收赃物流程",r.noArrest);
        box("附近守卫不向玩家 / 军团发起抓捕战斗",r.guardTruce);
        if(ImGui::SliderFloat("守卫处理半径",&r.guardRadius,256,12000,"%.0f"))changed=true;
        if(ImGui::Button("立即清除当前被捕状态 / 赏金"))Send(Op::ClearCrime11);
        ImGui::Text("原生拦截：%s | 赏金 %llu | 监禁 %llu | 罚款 %llu",view.law11.installed?"已安装":"未安装，仅轮询兜底",static_cast<unsigned long long>(view.law11.gold),static_cast<unsigned long long>(view.law11.prison),static_cast<unsigned long long>(view.law11.fine));
        Help("只处理守卫对你/己方的抓捕敌意，不会让所有野怪停战；本工具主动生成的敌人不会被这项误变友好。未知模组直接传送入狱不等于原生监禁调用。");
        ImGui::SeparatorText("玩家控制权");
        box("全局玩家自由：拒绝非主动对话 / 剧情接管",r.playerFreedom14);
        box("保留主动激活 NPC 的对话（支持改键 / 手柄）",r.preserveVoluntary14);
        Help("全局自由会恢复全部玩家操作；下方细项用于关闭全局自由后的自定义。只解除玩家当前占用，不停止整个任务，也不回滚已经运行的脚本或已获得物品。普通物品提示不等于强制剧情。");
        ImGui::Text("非主动对话释放 %llu | 玩家 Scene 释放 %llu",static_cast<unsigned long long>(view.rejectedDialogues14),static_cast<unsigned long long>(view.releasedScenes14));
        box("持续恢复下列操作，不接受脚本禁用",r.controlGuard);
        box("解除 Player AI Driven（不让 AI 操纵玩家）",r.noAIDriven);
        if(ImGui::CollapsingHeader("自定义保留哪些操作")) {
            constexpr const char* labels[]={"移动","视角转动","激活 / E","菜单","控制台","切换第一/第三人称","战斗/施法","潜行","四大菜单快捷键","视角滚轮","跳跃"};
            for(int i=0;i<11;++i)changed=ImGui::CheckboxFlags(labels[i],&r.controlMask,static_cast<unsigned int>(1u<<i))||changed;
        }
        box("打断玩家当前剧情 Scene（会影响剧情）",r.breakScenes);
        box("自动退出玩家家具占用（包括自己坐下）",r.breakFurniture);
        box("解除脚本禁存档 / 禁等待标志",r.noSaveWaitLock);
        box("清除原版每级训练次数计数",r.unlimitedTraining);
        box("持续解除脚本禁快速旅行标志",r.keepFastTravel);
        if(ImGui::Button("允许剧情接管60秒"))Send(Op::KernelSuspend11,0,0,60);
        ImGui::SameLine();if(ImGui::Button("立即恢复控制守护"))Send(Op::KernelSuspend11,0,0,0);
        if(view.suspended11>0)ImGui::Text("临时放行还剩 %.0f 秒（赏金/监禁豁免仍生效）",view.suspended11);
        ImGui::SeparatorText("门 / 家具 / 容器");
        box("准星指向可交互对象时自动接管、解锁和解封",r.aimedOwnership);
        box("持续保留已接管对象的所有权 / 解封状态",r.protectClaims);
        ImGui::Text("守护对象 %zu | 操作恢复 %llu | 守卫处理 %llu | 解封修复 %llu",view.protectedObjects11,static_cast<unsigned long long>(view.controlsRestored11),static_cast<unsigned long long>(view.guardReleases11),static_cast<unsigned long long>(view.activationRepairs11));
        if(changed)Engine::Get().Submit(Action{.op=Op::KernelRules11,.epoch=view.epoch,.rules11=r});
        ImGui::SeparatorText("任何地点等待（本面板入口）");
        ImGui::InputInt("等待小时",&waitHours11);waitHours11=std::clamp(waitHours11,1,72);
        if(ImGui::Button("关闭面板并等待"))Send(Op::Rest11,0,0,waitHours11);
        Help("F8 等待不检查附近敌人/非法闯入；保留原生时间推进和相关脚本。不宣称已替换原版 T 键/地图的所有判断。");
        if(ImGui::CollapsingHeader("旅行 / 脱困 / 其他原有自由工具"))FreedomPage10();
    }
    void LoadPersonalDraft11(const PersonalQuest11& q) {
        personalDraft11=q;personalDraftLoaded11=true;personalDraftEpoch11=view.epoch;
        std::snprintf(personalTitle11,sizeof(personalTitle11),"%s",q.title.c_str());
        for(int i=0;i<kPersonalGoalLimit11;++i) {
            std::snprintf(personalGoalText11[i].data(),personalGoalText11[i].size(),"%s",i<static_cast<int>(q.goals.size())?q.goals[i].text.c_str():"");
            std::snprintf(personalFormText11[i].data(),personalFormText11[i].size(),"%08X",i<static_cast<int>(q.goals.size())?q.goals[i].form:0);
        }
    }
    void PersonalPage11() {
        ImGui::SeparatorText("keqing 自建任务工作台");
        Help("最多64条自建任务，每条8个目标。开始后使用本工具独立 QUST 模板进入原版 J 日志；支持手动、收集物品、到达地点、击败指定实例。不生成对白/配音或任意剧情脚本。");
        if(!view.personalRuntime11){Help("自建任务模板未加载，请替换并勾选本版 FreedomControlRuntime.esp。");return;}
        if(!personalDraftLoaded11||personalDraftEpoch11!=view.epoch){LoadPersonalDraft11(PersonalQuest11{});personalPending11=false;}
        if(personalPending11 && view.personalRequest11==personalRequest11) {
            personalPending11=false;
            for(const auto& q:view.personal11)if(q.slot==view.personalSlot11){LoadPersonalDraft11(q);break;}
        }
        if(ImGui::Button("新建任务草稿"))LoadPersonalDraft11(PersonalQuest11{});
        ImGui::SameLine();ImGui::Text("已创建 %zu / %d",view.personal11.size(),kPersonalQuestSlots11);
        if(ImGui::BeginCombo("我的任务列表",personalDraft11.slot<0?"新草稿":personalDraft11.title.c_str())) {
            for(const auto& q:view.personal11){ImGui::PushID(q.slot);if(ImGui::Selectable(q.title.c_str(),q.slot==personalDraft11.slot))LoadPersonalDraft11(q);ImGui::PopID();}
            ImGui::EndCombo();
        }
        const PersonalQuest11* live=nullptr;
        for(const auto& q:view.personal11)if(q.slot==personalDraft11.slot){live=&q;break;}
        if(live) {
            constexpr const char* statuses[]={"草稿","进行中","暂停","完成","已放弃"};
            ImGui::Text("任务 #%d | %s | 修订 %llu",live->slot+1,statuses[static_cast<int>(live->status)],static_cast<unsigned long long>(live->revision));
            auto action=[&](const char* label,int code){if(ImGui::Button(label))Send(Op::PersonalCommand11,static_cast<ID>(live->slot),0,code);};
            action("开始 / 继续",1);ImGui::SameLine();action("暂停",2);ImGui::SameLine();action("全部完成",3);ImGui::SameLine();action("放弃",4);
            action("重置进度",5);ImGui::SameLine();action("删除自建任务",6);ImGui::SameLine();
            if(ImGui::Button("重新读取 / 放弃未保存修改"))LoadPersonalDraft11(*live);
            if(view.quests11)for(const auto& q:*view.quests11)if(q.personal&&q.personalSlot==live->slot) {
                ImGui::Text("原生任务 %08X | 启用=%s | 运行=%s | 阶段=%d",q.id,q.enabled?"是":"否",q.running?"是":"否",q.stage);break;
            }
            for(std::size_t i=0;i<live->goals.size();++i) {
                ImGui::PushID(static_cast<int>(i)+100);bool done=live->goals[i].done;
                if(ImGui::Checkbox(live->goals[i].text.c_str(),&done))Send(Op::PersonalGoal11,static_cast<ID>(live->slot),static_cast<ID>(i),done?1:0);
                ImGui::PopID();
            }
        }
        ImGui::SeparatorText("编辑任务定义（点保存才应用）");
        ImGui::InputText("任务标题",personalTitle11,sizeof(personalTitle11));
        ImGui::Checkbox("全部目标达成时自动完成任务",&personalDraft11.autoFinish);
        int remove=-1;
        constexpr const char* kinds[]={"手动勾选","拥有指定物品","到达记录位置","击败指定 NPC 实例"};
        for(int i=0;i<static_cast<int>(personalDraft11.goals.size());++i) {
            auto& g=personalDraft11.goals[i];ImGui::PushID(i);ImGui::Separator();
            ImGui::Text("目标 %d",i+1);ImGui::InputText("说明",personalGoalText11[i].data(),personalGoalText11[i].size());
            int kind=static_cast<int>(g.kind);if(ImGui::Combo("判定方式",&kind,kinds,4))g.kind=static_cast<GoalKind11>(kind);
            if(g.kind==GoalKind11::CollectItem) {
                ImGui::InputText("物品 Base FormID",personalFormText11[i].data(),personalFormText11[i].size());
                if(ImGui::Button("使用目录中选中的物品")){g.form=selectedForm;std::snprintf(personalFormText11[i].data(),16,"%08X",selectedForm);}
                ImGui::InputInt("所需数量",&g.count);
            } else if(g.kind==GoalKind11::ReachPoint) {
                if(ImGui::Button("记录玩家当前位置")){g.form=view.playerCell;g.world=view.playerWorld11;g.position=view.playerPosition11;std::snprintf(personalFormText11[i].data(),16,"%08X",g.form);}
                ImGui::Text("Cell %08X | XYZ %.0f %.0f %.0f",g.form,g.position[0],g.position[1],g.position[2]);
                ImGui::InputFloat("到达半径",&g.radius,32,256,"%.0f");
            } else if(g.kind==GoalKind11::DefeatReference) {
                if(ImGui::Button("绑定当前选中的 NPC")&&view.target.actor&&view.target.id!=0x14){g.form=view.target.id;g.base=view.target.base;std::snprintf(personalFormText11[i].data(),16,"%08X",g.form);}
                ImGui::Text("目标 Ref %08X | Base %08X",g.form,g.base);
                Help("只认指定实例真正死亡。丢失引用、未加载或被删除不算死亡。");
            }
            if(personalDraft11.goals.size()>1&&ImGui::SmallButton("删除这个目标"))remove=i;
            ImGui::PopID();
        }
        if(remove>=0) {
            for(int i=0;i<static_cast<int>(personalDraft11.goals.size());++i) {personalDraft11.goals[i].text=personalGoalText11[i].data();if(personalDraft11.goals[i].kind==GoalKind11::CollectItem)personalDraft11.goals[i].form=ParseID(personalFormText11[i].data()).value_or(0);}
            personalDraft11.title=personalTitle11;personalDraft11.goals.erase(personalDraft11.goals.begin()+remove);LoadPersonalDraft11(personalDraft11);
        }
        if(personalDraft11.goals.size()<kPersonalGoalLimit11&&ImGui::Button("增加目标")) {
            const auto i=personalDraft11.goals.size();personalDraft11.goals.emplace_back();std::snprintf(personalGoalText11[i].data(),512,"%s","自定义目标");personalFormText11[i][0]=0;
        }
        ImGui::BeginDisabled(personalPending11);
        if(ImGui::Button(personalPending11?"正在保存…":"保存任务定义")) {
            personalDraft11.title=personalTitle11;
            for(int i=0;i<static_cast<int>(personalDraft11.goals.size());++i){personalDraft11.goals[i].text=personalGoalText11[i].data();if(personalDraft11.goals[i].kind==GoalKind11::CollectItem)personalDraft11.goals[i].form=ParseID(personalFormText11[i].data()).value_or(0);}
            personalDraft11.Normalize();personalPending11=true;++personalRequest11;
            Engine::Get().Submit(Action{.op=Op::PersonalSave11,.epoch=view.epoch,.seed=personalRequest11,.personal11=personalDraft11});
        }
        ImGui::EndDisabled();
        Help("正在运行的任务若被自动更新，保存旧草稿会被拒绝以免覆盖进度；点重新读取后再改。目标地点不自动生成导航标记。");
    }
    void SexLabPage16() {
        constexpr ID playerID16=0x14;
        ImGui::SeparatorText("SexLab 快速启动器");
        ImGui::Text("SexLab.esm：%s | Framework 脚本：%s",view.sexLabInstalled16?"已检测":"未检测",view.sexLabBound16?"已绑定":"未绑定");
        ImGui::TextWrapped("状态：%s",view.sexLabStatus16.empty()?"尚无操作。":view.sexLabStatus16.c_str());
        if (!view.sexLabInstalled16) Help("当前运行时没有检测到 SexLab.esm。请确认 MO2 右侧插件页已启用 SexLab.esm。\n");
        else if (!view.sexLabBound16) Help("检测到 SexLab.esm，但 SexLabFramework 尚未绑定到它的 Framework Quest。通常需要先让 SexLab 在 MCM 中完成初始化。\n");
        ImGui::SetNextItemWidth(std::min(520.0f,ImGui::GetContentRegionAvail().x));
        ImGui::InputTextWithHint("动画标签","留空 = 让 SexLab 自动匹配；也可填你已注册动画的标签",sexLabTags16,sizeof(sexLabTags16));
        Help("快速启动器直接调用你已安装的 SexLabFramework.QuickStart。标签留空时由 SexLab 根据参与者/生物种族自动筛动画；本工具不重新注册 SLAL 动画。\n");

        const bool quickTarget=view.target.actor && view.target.id && view.target.id!=playerID16 && !view.target.dead;
        ImGui::BeginDisabled(!view.sexLabBound16 || !quickTarget);
        if (ImGui::Button("玩家 + 当前准星目标：立即开始（自动匹配）",ImVec2(-1,0))) {
            Engine::Get().Submit(Action{.op=Op::SexLabStart16,.target=view.target.id,.text=sexLabTags16,.epoch=view.epoch,.afterClose=true,.scope=1});
        }
        ImGui::EndDisabled();
        if (!quickTarget) Help("先把准星对着一个活着的 NPC / 生物；按 F8 时准星目标会显示在面板顶部。\n");

        ImGui::SeparatorText("自定义参与者（最多 5 名）");
        if (ImGui::Button("加入玩家")) Send(Op::SexLabAdd16,playerID16);
        ImGui::SameLine();
        ImGui::BeginDisabled(!view.target.actor || !view.target.id || view.target.dead);
        if (ImGui::Button("加入当前目标")) Send(Op::SexLabAdd16,view.target.id);
        ImGui::EndDisabled();
        ImGui::SameLine(); if (ImGui::Button("清空参与者")) Send(Op::SexLabClear16);

        for (std::size_t i=0;i<view.sexLabActors16.size();++i) {
            const auto& row=view.sexLabActors16[i];
            ImGui::PushID(static_cast<int>(row.id));
            ImGui::Text("%zu. %s | %s",i+1,row.name.c_str(),row.state.c_str());
            ImGui::SameLine(); ImGui::BeginDisabled(i==0); if (ImGui::SmallButton("上移")) Send(Op::SexLabMove16,row.id,0,-1); ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::BeginDisabled(i+1>=view.sexLabActors16.size()); if (ImGui::SmallButton("下移")) Send(Op::SexLabMove16,row.id,0,1); ImGui::EndDisabled();
            ImGui::SameLine(); if (ImGui::SmallButton("移除")) Send(Op::SexLabRemove16,row.id);
            ImGui::PopID();
        }
        ImGui::BeginDisabled(!view.sexLabBound16 || view.sexLabActors16.empty());
        if (ImGui::Button("按上方参与者顺序启动 SexLab",ImVec2(-1,0))) {
            Engine::Get().Submit(Action{.op=Op::SexLabStart16,.text=sexLabTags16,.epoch=view.epoch,.afterClose=true});
        }
        ImGui::EndDisabled();
        Help("点击启动后 FreedomControl 会先完整关闭 F8、恢复游戏时间/输入/声音，再在主线程向 SexLab QuickStart 提交请求，避免在暂停面板里启动动画。参与者顺序会原样传给 SexLab。\n");
        ImGui::BeginDisabled(!view.sexLabBound16);
        if (ImGui::Button("停止 / 清理全部 SexLab 活动线程",ImVec2(-1,0))) {
            Engine::Get().Submit(Action{.op=Op::SexLabStopAll16,.epoch=view.epoch,.afterClose=true});
        }
        ImGui::EndDisabled();
        Help("用于卡死或需要立即结束全部 SexLab 场景时调用 SexLab 自己的 sslThreadSlots.StopAll。它会先关闭 F8 再提交，不会在暂停状态里强拆线程。\n");

        if (ImGui::CollapsingHeader("从附近角色快速加入")) {
            int shown=0;
            for (const auto& row:view.nearby) {
                if (shown++>=32) break;
                ImGui::PushID(static_cast<int>(row.id));
                if (ImGui::SmallButton("+")) Send(Op::SexLabAdd16,row.id);
                ImGui::SameLine(); ImGui::TextUnformatted(row.name.c_str());
                ImGui::PopID();
            }
            if (!shown) Help("当前附近角色列表为空。\n");
        }
        ImGui::SeparatorText("故障定位");
        ImGui::BulletText("未检测：检查 SexLab.esm 是否启用。");
        ImGui::BulletText("未绑定：先完成 SexLab MCM 初始化。");
        ImGui::BulletText("已提交但没有动画：检查 SLAL 是否已注册、参与者种族是否被 SexLab/MNC 支持、标签是否过窄。");
        ImGui::BulletText("Creature 场景仍由 SexLab 的 CreatureSlots / Creature Framework / 你的动画包决定兼容性。");
    }
    void CameraPage() {
        if (ImGui::Button("切换自由相机（TFC）")) Cmd("tfc",0,true);
        ImGui::SameLine(); if (ImGui::Button("自由相机 + 冻结世界（TFC 1）")) Cmd("tfc 1",0,true);
        ImGui::InputFloat("视野 FOV",&fov,5,10,"%.1f");
        if (ImGui::Button("设置 FOV")) Cmd(fmt::format("fov {}",Finite(fov,85,20,160)));
        ImGui::InputFloat("自由相机速度",&cameraSpeed,1,10,"%.1f");
        if (ImGui::Button("设置相机速度")) Cmd(fmt::format("sucsm {}",Finite(cameraSpeed,10,0.1f,1000)));
        if (ImGui::Button("切换原版 UI（TM）")) Cmd("tm");
        Help("这些是原版命令开关，本菜单不会自动推断它们当前的开启状态。即使隐藏原版 UI，本覆盖层仍可独立显示。");
    }
    void SettingsPage() {
        if (!settingsLoaded || settingsEpoch!=view.epoch) { followDist=view.followDistance; leashDist=view.teleportDistance; flySpeed=view.flightSpeed; dragonsInside=view.dragonsIndoors; textScale=view.fontScale; settingsLoaded=true; settingsEpoch=view.epoch; }
        ImGui::Text("F8 冻结：世界 %s | 主音频 %s",view.worldPaused?"已暂停":"等待原生暂停",view.audioMuted16?"已静音冻结":(view.audioAvailable16?"等待静音":"音频接口不可用"));
        Help("Repair16 会同时使用 Skyrim 原生 PausesGame 菜单暂停世界，并独占主声音分类为 0；关闭面板时恢复打开前的主音量。音频内部播放游标可能继续推进，但面板打开期间不会再听到游戏声音。");
        if (ImGui::SliderFloat("文字缩放",&textScale,0.8f,1.6f,"%.2f")) Send(Op::UISettings,0,0,1,{}, {textScale,0,0,0});
        if (ImGui::Button("重新居中 / 放大面板")) sizeReset=true;
        ImGui::InputFloat("跟随距离",&followDist,50,500,"%.0f");
        ImGui::InputFloat("自动拉回距离",&leashDist,100,1000,"%.0f");
        ImGui::InputFloat("飞行垂直速度（单位/秒）",&flySpeed,100,1000,"%.0f");
        ImGui::Checkbox("允许把龙自动拉入室内（实验）",&dragonsInside);
        if (ImGui::Button("应用控制器设置")) Send(Op::Settings,0,0,dragonsInside?1:0,{}, {followDist,leashDist,flySpeed,0});
        Help("菜单键和批处理速率在 Data/SKSE/Plugins/FreedomControl.ini 中配置。控制列表/设置保存在当前存档对应的 SKSE co-save 中，不会全角色共享。");
        ImGui::SeparatorText("控制台命令桥");
        ImGui::InputText("单条控制台命令",command,sizeof(command));
        if (ImGui::Button("运行全局命令")) Cmd(command);
        ImGui::SameLine(); if (ImGui::Button("对所选对象运行")) Cmd(command,view.target.id);
        Help("仅支持单行 Skyrim 控制台/脚本命令，不会执行 Windows 命令。某些命令在当前游戏状态下可能无效果。");
    }
    void Draw() {
        view=Engine::Get().Snapshot(); catalog=Engine::Get().Catalog();
        auto& io=ImGui::GetIO(); io.FontGlobalScale=view.fontScale;
        if (io.DisplaySize.x<64 || io.DisplaySize.y<64) return; // minimized / invalid render area
        const auto bounds=FitPanel(io.DisplaySize.x,io.DisplaySize.y);
        if (sizeReset) {
            ImGui::SetNextWindowPos(ImVec2(bounds.left,bounds.top),ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(bounds.width,bounds.height),ImGuiCond_Always); sizeReset=false;
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(700.0f,bounds.width),std::min(500.0f,bounds.height)),ImVec2(io.DisplaySize.x-12,io.DisplaySize.y-12));
        bool open=Engine::Get().MenuOpen();
        if (!ImGui::Begin("FreedomControl 0.5.5 | SexFast / 完整冻结修复16 | keqing | Skyrim 1.6.1170",&open)) {
            ImGui::End(); if (!open) Engine::Get().SetMenuOpen(false); return;
        }
        if (!open) Engine::Get().SetMenuOpen(false);
        ImGui::Text("%s | 菜单 F8 | 待处理 %zu",view.worldPaused?"世界已暂停":"世界运行中",view.pending);
        ImGui::SameLine(); if (ImGui::Button(view.closing?"等待应用完成…":"应用并关闭")) Engine::Get().SetMenuOpen(false);
        if (view.batchTotal>0) {
            const float fraction=std::min(1.0f,static_cast<float>(view.batchDone)/static_cast<float>(view.batchTotal));
            auto label=fmt::format("批量 {}/{} | 提交 {} | 已有 {} | 失败 {}",view.batchDone,view.batchTotal,view.batchSent,view.batchSkipped,view.batchFailed);
            ImGui::ProgressBar(fraction,ImVec2(-1,0),label.c_str());
            if (view.closing && view.pending && ImGui::Button("取消余项并关闭")) Send(Op::CancelBatch);
        }
        if(view.spawnPending) {
            ImGui::Text("角色生成中：%zu / %zu（余 %zu）",view.spawnDone,view.spawnTotal,view.spawnPending);
            ImGui::SameLine(); if(ImGui::Button("取消剩余生成")) Send(Op::CancelSpawn);
        }
        if (!view.ready) Help("请先读取存档。");
        ImGui::BeginDisabled(!view.ready);
        if(ImGui::Button("任务中心 / 查 ID")){activePage=8;Send(Op::QuestRefresh11);}
        ImGui::SameLine();if(ImGui::Button("自由内核 / 守卫豁免"))activePage=14;
        ImGui::SameLine();if(ImGui::Button("创建自己的任务"))activePage=16;
        ImGui::SameLine();if(ImGui::Button("SexLab 快速启动"))activePage=17;
        TargetHeader();
        const float footer=ImGui::GetTextLineHeightWithSpacing()*3.0f;
        ImGui::BeginChild("navigation",ImVec2(195.0f*view.fontScale,-footer),true);
        ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##featurefind","找功能…",pageFilter,sizeof(pageFilter));
        constexpr const char* labels[]={"玩家能力","房屋接管","对象编辑","NPC / 跟随","世界控制","目录 / 物品 / 解锁","传送","属性 / 阵营","任务中心 / ID","相机","设置 / 诊断","我的军团","生物快速生成","随机敌人","自由内核","keqing 净域","自建任务","SexLab 快速启动"};
        constexpr const char* tags[]={"无敌 无限 血量 生命 法力 魔力 耐力 等级 恢复率 负重 飞行 速度 跳跃 技能 魔法 龙吼", "房屋 归属 占领 接管 keqing 床", "对象 移动 旋转 缩放 名字 删除", "NPC 跟随 龙 生物 秒杀 停止 战斗 复活", "天气 和平 时间 战斗 冻结", "物品 item 全解锁 武器 服装 护甲 魔法 筛选 搜索", "传送 地点 书签", "属性 数值 阵营 速度", "任务 quest id 编号 阶段 搜索 拒接 追踪 目标", "相机 摄影 视角", "设置 字体 大小 调试 冻结 声音 音频", "军团 队伍 随从 强制 控制 抢回 战斗 敌人 集火 实力", "角色 NPC 生物 DC MNC 怪物 人类 生成 刷怪 种族 搜索 数量", "随机 敌人 生物 刷怪 种子", "自由 限制 旅行 战斗 进门 负重 呼吸 控制 脱困 守卫 抓捕 监禁 赏金 等待 训练", "keqing 净域 清空 删除 所有 全局 半径 技能 能力", "任务 新建 自己 创建 修改 keqing 收集 目标 日志", "SexLab sexlab 动画 快速 启动 SLAL MNC 生物 参与者 标签"};
        for (int i=0;i<static_cast<int>(std::size(labels));++i) if (Contains(labels[i],pageFilter)||Contains(tags[i],pageFilter))
            if (ImGui::Selectable(labels[i],activePage==i)) activePage=i;
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("page-content",ImVec2(0,-footer),true);
        ImGui::PushItemWidth(std::min(360.0f,ImGui::GetContentRegionAvail().x*0.45f));
        switch(activePage) {
        case 0:PlayerPage();break; case 1:HousePage();break; case 2:ObjectPage();break;
        case 3:ActorPage();break; case 4:WorldPage();break; case 5:CatalogPage();break;
        case 6:TeleportPage();break; case 7:StatsPage();break; case 8:QuestPage();break;
        case 9:CameraPage();break; case 11:LegionPage();break; case 12:SpawnActorsPage();break; case 13:RandomEnemiesPage10();break; case 14:KernelPage11();break; case 15:KeqingPage10();break; case 16:PersonalPage11();break; case 17:SexLabPage16();break; default:SettingsPage();break;
        }
        if (activePage==10 && ImGui::CollapsingHeader("输入自检 / 日志")) {
            if (ImGui::Button("输入自检：点击这里")) ++uiClickTest;
            ImGui::Text("UI 点击 %d | 鼠标按下 %llu",uiClickTest,static_cast<unsigned long long>(polledMousePresses));
            const auto input=input13::Snapshot();
            ImGui::Text("键盘事件 %llu | 文字 %llu | 滚轮 %llu | 通道 %s / %s",input.keys,input.characters,input.wheels,input.messages?"已接入":"备用",input.mouse?"已接入":"备用");
            for (const auto& msg:view.messages) ImGui::TextWrapped("%s",msg.c_str());
        }
        ImGui::PopItemWidth(); ImGui::EndChild();
        ImGui::EndDisabled();
        ImGui::Separator();
        if (!view.messages.empty()) ImGui::TextWrapped("%s",view.messages.back().c_str());
        ImGui::End();
    }

};
Menu menu;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if(msg==WM_NCDESTROY)input13::Shutdown();
    input13::WindowMessage(hwnd,msg,wparam,lparam);
    std::scoped_lock lock(uiMutex);
    auto& engine=Engine::Get();
    // Keep the panel and world paused across ordinary Alt-Tab. Only destruction
    // is terminal; release through the engine/UI tasks, never from WndProc.
    if (msg==WM_NCDESTROY) {
        engine.SetMenuOpen(false); engine.RequestTick();
    }
    // Hotkey state is sampled independently in DrawOverlay. Do not toggle here:
    // Skyrim or another overlay may not forward keyboard window messages.
    if ((msg==WM_KEYDOWN || msg==WM_KEYUP) && static_cast<int>(wparam)==engine.Hotkey()) return 0;
    if (initialized && context) {
        ContextScope scope(context);
        if (engine.MenuOpen()) {
            // Input13 owns keyboard/text/wheel; PollMouseInput owns buttons.
            // Feeding these messages to the backend again would duplicate input.
            const bool routed=(msg>=WM_KEYFIRST && msg<=WM_KEYLAST) || msg==WM_CHAR ||
                msg==WM_MOUSEWHEEL || msg==WM_MOUSEHWHEEL || IsMouseButtonMessage(msg);
            if (!routed) ImGui_ImplWin32_WndProcHandler(hwnd,msg,wparam,lparam);
            if ((msg>=WM_MOUSEFIRST && msg<=WM_MOUSELAST) || msg==WM_INPUT ||
                (msg>=WM_KEYFIRST && msg<=WM_KEYLAST) || msg==WM_CHAR) {
                // WM_INPUT requires DefWindowProc for foreground raw-input cleanup.
                return msg==WM_INPUT ? DefWindowProcW(hwnd,msg,wparam,lparam) : 0;
            }
        } else if (msg==WM_KILLFOCUS) ImGui_ImplWin32_WndProcHandler(hwnd,msg,wparam,lparam);
    }
    return CallWindowProcW(originalWndProc,hwnd,msg,wparam,lparam);
}
bool InitializeImGui(IDXGISwapChain* swapchain) {
    initAttempted=true;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapchain->GetDesc(&desc)) || !desc.OutputWindow) return false;
    if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(device.GetAddressOf())))) return false;
    device->GetImmediateContext(deviceContext.GetAddressOf());
    if (!deviceContext) return false;
    window=desc.OutputWindow;
    auto* previous=ImGui::GetCurrentContext();
    IMGUI_CHECKVERSION();
    context=ImGui::CreateContext();
    if (!context) { ImGui::SetCurrentContext(previous); return false; }
    ImGui::SetCurrentContext(context);
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr; io.LogFilename=nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.20f);
    // Bound the full CJK atlas dimensions; use system fonts only, never redistribute fonts.
    io.Fonts->TexDesiredWidth=4096;
    ImFontConfig fontConfig; fontConfig.OversampleH=1; fontConfig.OversampleV=1;
    // Use a Chinese-capable font already installed by Windows. No font file is distributed.
    wchar_t windir[MAX_PATH]{};
    bool chineseFontLoaded=false;
    if (GetWindowsDirectoryW(windir,MAX_PATH)) {
        const auto fonts=std::filesystem::path(windir)/L"Fonts";
        for (const auto* name : {L"msyh.ttc",L"msyhbd.ttc",L"simsun.ttc",L"simhei.ttf"}) {
            const auto font=fonts/name;
            if (!std::filesystem::exists(font)) continue;
            auto utf8=font.u8string();
            if (io.Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8.c_str()),24.0f,&fontConfig,io.Fonts->GetGlyphRangesChineseFull())) {
                chineseFontLoaded=true;
                spdlog::info("zh-CN UI font loaded from Windows Fonts: {}.",font.filename().string());
                break;
            }
        }
    }
    if (!chineseFontLoaded) {
        io.Fonts->AddFontDefault();
        spdlog::warn("No Chinese-capable Windows font was loaded; zh-CN glyphs may be missing.");
    }
    bool win32=ImGui_ImplWin32_Init(window);
    bool dx11=win32 && ImGui_ImplDX11_Init(device.Get(),deviceContext.Get());
    if (!dx11) {
        if (win32) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context); context=nullptr; ImGui::SetCurrentContext(previous); return false;
    }
    SetLastError(0);
    auto old=SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WindowProc));
    if (!old && GetLastError()!=0) {
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context); context=nullptr; ImGui::SetCurrentContext(previous); return false;
    }
    originalWndProc=reinterpret_cast<WNDPROC>(old);
    input13::Install(window);
    initialized=true;
    ImGui::SetCurrentContext(previous);
    spdlog::info("DX11 overlay initialized; menu key VK=0x{:02X}; Input-fix.6 (foreground + mouse polling).",Engine::Get().Hotkey());
    return true;
}
void DrawOverlay(IDXGISwapChain* swapchain) {
    std::scoped_lock lock(uiMutex);
    if (!initialized && !initAttempted) {
        if (!InitializeImGui(swapchain)) spdlog::error("ImGui initialization failed; rendering is bypassed.");
    }
    if (!initialized || !context) return;
    PollMenuHotkey();
    Engine::Get().RequestTick();
    ContextScope scope(context);
    auto& io=ImGui::GetIO(); io.MouseDrawCursor=Engine::Get().MenuOpen();
    RECT client{};
    if (IsIconic(window) || (GetClientRect(window,&client) && (client.right<=client.left || client.bottom<=client.top))) {
        // Minimized/zero-sized targets are transient, not a failed overlay.
        // Keep the native world pause; discard input state until visible again.
        input13::Frame(false,Engine::Get().Hotkey());
        return;
    }
    UpdateWndProcState();
    io.FontGlobalScale=Engine::Get().Snapshot().fontScale;
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame();
    PollMouseInput();
    input13::Frame(Engine::Get().MenuOpen(),Engine::Get().Hotkey());
    ImGui::NewFrame();
    if (Engine::Get().MenuOpen()) menu.Draw();
    ImGui::Render();
    if (!Engine::Get().MenuOpen()) {
        menuDrawLogged=false;
        drawFailureLogged=false;
        return;
    }
    ComPtr<ID3D11Texture2D> backBuffer;
    const HRESULT bufferResult=swapchain->GetBuffer(0,IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(bufferResult)) {
        if (!drawFailureLogged) spdlog::error("Menu back-buffer lookup failed: HRESULT=0x{:08X}.",static_cast<std::uint32_t>(bufferResult));
        drawFailureLogged=true;
        Engine::Get().SetMenuOpen(false); Engine::Get().RequestTick();
        return;
    }
    ComPtr<ID3D11RenderTargetView> renderTarget;
    const HRESULT targetResult=device->CreateRenderTargetView(backBuffer.Get(),nullptr,renderTarget.GetAddressOf());
    if (FAILED(targetResult)) {
        if (!drawFailureLogged) spdlog::error("Menu render-target creation failed: HRESULT=0x{:08X}.",static_cast<std::uint32_t>(targetResult));
        drawFailureLogged=true;
        Engine::Get().SetMenuOpen(false); Engine::Get().RequestTick();
        return;
    }
    // No persistent back-buffer reference: ResizeBuffers is not obstructed by this plugin.
    std::array<ID3D11RenderTargetView*,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> previousTargets{};
    ID3D11DepthStencilView* previousDepth=nullptr;
    deviceContext->OMGetRenderTargets(static_cast<UINT>(previousTargets.size()),previousTargets.data(),&previousDepth);
    auto* target=renderTarget.Get(); deviceContext->OMSetRenderTargets(1,&target,nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    deviceContext->OMSetRenderTargets(static_cast<UINT>(previousTargets.size()),previousTargets.data(),previousDepth);
    for (auto* item:previousTargets) if (item) item->Release();
    if (previousDepth) previousDepth->Release();
    const auto* drawData=ImGui::GetDrawData();
    if (!menuDrawLogged && drawData && drawData->TotalVtxCount>0) {
        spdlog::info("Input-fix.6: menu draw submitted ({} vertices, display {} x {}).",drawData->TotalVtxCount,drawData->DisplaySize.x,drawData->DisplaySize.y);
        menuDrawLogged=true;
    }
}
HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* swapchain, UINT interval, UINT flags) {
    if (swapchain==hookedSwapchain && !(flags & DXGI_PRESENT_TEST)) {
        try { DrawOverlay(swapchain); }
        catch (const std::exception& e) { Engine::Get().SetMenuOpen(false); Engine::Get().RequestTick(); spdlog::error("Overlay C++ exception: {}; closing paused panel.",e.what()); }
    }
    return originalPresent(swapchain,interval,flags);
}
}
bool Install() {
    if (installAttempted) return originalPresent!=nullptr;
    installAttempted=true;
    auto* renderer=RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer) { spdlog::error("Renderer is unavailable."); return false; }
    auto* swapchain=reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows[0].swapChain);
    if (!swapchain) { spdlog::error("Renderer swapchain is unavailable."); return false; }
    auto** table=*reinterpret_cast<void***>(swapchain);
    // IDXGISwapChain::Present is COM vtable slot 8, not a Skyrim executable offset.
    auto** slot=&table[8];
    DWORD protection{};
    if (!VirtualProtect(slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&protection)) {
        spdlog::error("Cannot update the swapchain Present slot ({}).",GetLastError()); return false;
    }
    hookedSwapchain=swapchain;
    originalPresent=reinterpret_cast<PresentFn>(*slot);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(&Present));
    DWORD ignored{}; VirtualProtect(slot,sizeof(void*),protection,&ignored);
    FlushInstructionCache(GetCurrentProcess(),slot,sizeof(void*));
    spdlog::info("Present hook installed with previous-hook chaining. Overlay interoperability is untested.");
    return true;
}
}
