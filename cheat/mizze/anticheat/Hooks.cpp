// Hooks.cpp - the managed hook table.
//
// Every entry is a managed class and method of the game. The native
// implementation is never touched: the pointer inside the method info is
// redirected, so the code pages of the game stay byte identical and a memory
// checksum over them still matches.
//
// The table was generated from the IL2CPP metadata of the game, so the names
// and the argument counts come from the game itself.
#include "Hooks.h"

#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

namespace mizze {
namespace {

// The detours replace the call, so the arguments are simply not consumed.
bool DetourReturnFalse() { return false; }
bool DetourReturnTrue() { return true; }
void DetourSkip() {}

void* detourFor(Action action) {
  switch (action) {
    case Action::ReturnFalse: return reinterpret_cast<void*>(&DetourReturnFalse);
    case Action::ReturnTrue: return reinterpret_cast<void*>(&DetourReturnTrue);
    case Action::Skip: return reinterpret_cast<void*>(&DetourSkip);
  }
  return nullptr;
}

const HookSpec kSpecs[] = {
    // --- the client never reports anything -------------------------------
    {"COW.GameFacade", "IsEmulator", Action::ReturnFalse, 0,
     "client side emulator verdict"},
    {"COW.GameFacade", "NeedffantihackEmulatorCheck", Action::ReturnFalse, 0,
     "gate that runs the FF AntiHack SDK"},
    {"COW.GameConfig", "get_CheckHacker", Action::ReturnFalse, 0,
     "master switch of the hack check"},
    {"COW.EventLogger", "LogReportCheat", Action::Skip, 10,
     "sends the cheat report to the server"},
    {"COW.EventLogger", "LogReportCheatInHistory", Action::Skip, 8,
     "records the cheat report in history"},
    {"COW.EventLogger", "SendEventTypeHackClientFix", Action::Skip, 1,
     "reports the client side hack fix"},
    {"COW.EventLogger", "OnSystemScreenShot", Action::Skip, 0,
     "reports a screenshot to the server"},

    // --- login gates ------------------------------------------------------
    {"COW.UIModelLogin", "GetffantihackEnabled", Action::ReturnFalse, 0,
     "login gate: anti hack enabled"},
    {"COW.UIModelLogin", "GetEffectiveffantihackEnabled", Action::ReturnFalse, 0,
     "login gate: effective anti hack flag"},
    {"COW.UIModelLogin", "GetffantihackEnabledTempValue", Action::ReturnFalse, 1,
     "cached anti hack flag"},
    {"COW.UIModelLogin", "ProcessffantihackGGP", Action::Skip, 1,
     "login gate: runs the anti hack process"},
    {"COW.UIModelLogin", "ProcessffantihackOpen", Action::Skip, 1,
     "anti hack notice the server pushes at login"},

    // --- the server verdict never lands on the client ---------------------
    {"COW.UIModelLogin", "ProcessAccountForbidden", Action::Skip, 1,
     "the ban dialog the server triggers at login"},
    {"COW.UIModelLogin", "ShowEmailRegisterForbiddenTips", Action::Skip, 0,
     "forbidden notice at login"},
    {"COW.LobbyServiceConnectionHandler", "OnGroupQuickMatchCreditPunish",
     Action::Skip, 2, "credit punish notification from the server"},
    {"COW.LobbyServiceConnectionHandler", "OnGroupJoinHackerGroupNtf",
     Action::Skip, 1, "you joined a hacker group notice"},
    {"COW.LobbyServiceConnectionHandler", "OnMsgMatchMakingCdtHackerNtf",
     Action::Skip, 1, "matchmaking cheat notification"},
    {"COW.UIAntiHackWindowController", "ShowBanListWeb", Action::Skip, 0,
     "ban list web view"},
    {"COW.UIAntiHackWindowController", "ShowAntiHackWeb", Action::Skip, 0,
     "anti hack web view"},
    {"COW.UIHudMatchResultItemBaseController", "OnAddMatchBlackList",
     Action::Skip, 1, "adds an entry to the match blacklist"},
    {"COW.UIHudMatchResultItemBaseController", "OnGetMatchBlackListCount",
     Action::ReturnFalse, 1, "blacklist counter"},
    {"COW.UIModelProfile", "IsProfileDataPunished", Action::ReturnFalse, 1,
     "profile punishment flag"},
    {"COW.UIModelGroup", "CheckNewMemberCreditScorePunishTip", Action::Skip, 1,
     "credit score punish tip"},
    {"COW.UIHudCSMatchResultController",
     "CheckNeedShowQuickReportAndGetPunishedPlayName", Action::Skip, 1,
     "quick report and punished player name"},
    {"COW.UIHudLadderMatchResultController", "InitAntiHackProtect",
     Action::Skip, 1, "anti hack protect UI on the result screen"},

    // --- no emulator marks anywhere in the UI -----------------------------
    {"COW.UIModelGroup", "CheckEmulatorWarningEnable", Action::ReturnFalse, 0,
     "emulator warning switch"},
    {"COW.UIModelGroup", "CheckAndShowEmulatorWarningTip", Action::Skip, 1,
     "emulator warning tip"},
    {"COW.UIHudReplayMatchIDController", "SetEmulatorIcon", Action::Skip, 1,
     "emulator icon in the replay header"},
    {"COW.MobileReplayManager", "GetReplayIsEmulatorPool", Action::ReturnFalse, 0,
     "emulator pool flag of a replay"},
    {"COW.UILobbyLWObserveNavigationController", "OnAntihackBtnClick",
     Action::Skip, 0, "anti hack button in the lobby"},
    {"COW.UILobbyV2SubSystemController", "OnAntiHackCenterBtnClick",
     Action::Skip, 0, "anti hack centre button"},
    {"COW.UILobbyV2SubSystemController", "RefreshAntiHackRedPoint",
     Action::Skip, 0, "anti hack red dot in the lobby"},
    {"COW.UIProfileController", "RefreshCustomProfilePunishState",
     Action::Skip, 0, "custom profile punish state"},

    // --- the windows never open -------------------------------------------
    {"COW.UIModelAntiHackWindow", "ProcessAntiHackCenterDesc", Action::Skip, 1,
     "anti hack centre description"},
    {"COW.UINavigationUtil", "GotoV2AntiHackWindow", Action::Skip, 0,
     "opens the anti hack window"},
    {"COW.UIDebugConsoleController", "SetAntiHackButton", Action::Skip, 0,
     "debug console anti hack button"},
    {"COW.GameFacade", "OnSystemScreenShot", Action::Skip, 0,
     "game facade screenshot report"},
};

const int kSpecCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

// Every hook that was applied, so it can be restored if a hotfix takes the
// original implementation back.
struct Installed {
  void* method_info;
  void* detour;
  const char* label;
};

std::vector<Installed> g_installed;

void splitType(const char* type, std::string* ns, std::string* cls) {
  ns->clear();
  cls->clear();
  const char* dot = std::strchr(type, '.');
  if (!dot) {
    *cls = type;
    return;
  }
  *ns = std::string(type, dot - type);
  *cls = std::string(dot + 1);
}

bool applyOne(const HookSpec& spec, const Il2CppApi& api) {
  void* detour = detourFor(spec.action);
  if (!detour) return false;
  std::string ns;
  std::string cls;
  splitType(spec.type, &ns, &cls);
  void* klass = api.classFromName(ns.c_str(), cls.c_str());
  if (!klass) return false;
  void* info = api.methodFromName(klass, spec.method, spec.arg_count);
  if (!info) return false;
  if (!api.setMethodPointer(info, detour)) return false;
  for (Installed& entry : g_installed) {
    if (entry.method_info == info) {
      entry.detour = detour;
      return true;
    }
  }
  g_installed.push_back({info, detour, spec.method});
  return true;
}

}  // namespace

int hookSpecCount() { return kSpecCount; }

const HookSpec& hookSpecAt(int index) { return kSpecs[index]; }

HookStats installHooks(const Il2CppApi& api) {
  HookStats stats;
  if (!api.ready()) return stats;

  std::vector<int> pending;
  for (int i = 0; i < kSpecCount; ++i) {
    if (applyOne(kSpecs[i], api)) {
      stats.installed++;
    } else {
      pending.push_back(i);
    }
  }

  // The runtime may still be loading its metadata: retry instead of giving
  // up on the first attempt.
  for (int round = 0; round < 120 && !pending.empty(); ++round) {
    usleep(250 * 1000);
    std::vector<int> still;
    for (int index : pending) {
      if (applyOne(kSpecs[index], api)) {
        stats.installed++;
      } else {
        still.push_back(index);
      }
    }
    pending.swap(still);
  }

  stats.failed = static_cast<int>(pending.size());
  stats.skipped = 0;
  return stats;
}

int reapplyHooks(const Il2CppApi& api) {
  int restored = 0;
  for (const Installed& entry : g_installed) {
    if (api.methodPointerMatches(entry.method_info, entry.detour)) continue;
    if (api.setMethodPointer(entry.method_info, entry.detour)) restored++;
  }
  return restored;
}

}  // namespace mizze
