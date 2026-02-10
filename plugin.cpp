#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Rank {
    TESGlobal* global;
    BGSListForm* questList;
    std::string id;
    std::string label;
};

std::vector<Rank> rankData;

static void Inject(BSFixedString menuName) {
    auto ui = UI::GetSingleton();
    if (!ui) return;

    GPtr<IMenu> menu = ui->GetMenu(menuName);
    if (!menu || !menu->uiMovie) {
        return;
    }

    auto movie = menu->uiMovie;

    GFxValue _root;
    movie->GetVariable(&_root, "_root");

    GFxValue data;
    movie->CreateArray(&data);
    for (auto& faction : rankData) {
        GFxValue factionObj;
        movie->CreateObject(&factionObj);
        if (faction.global) {
            factionObj.SetMember("value", faction.global->value);
        }
        factionObj.SetMember("id", GFxValue(faction.id));
        factionObj.SetMember("label", GFxValue(faction.label));
        data.PushBack(factionObj);
    }
    _root.SetMember("_FactionRanks", data);

    GFxValue args[2];
    args[0] = GFxValue("FR");
    args[1] = GFxValue(1180);
    _root.Invoke("createEmptyMovieClip", nullptr, args, 2);
    if (movie->GetVariable(&_root, "_root.FR")) {
        GFxValue args[1];
        args[0] = GFxValue("factionranks_inject.swf");
        _root.Invoke("loadMovie", nullptr, args, 1);
    }
}

static void ParseData(const json& data) {
    rankData.reserve(data.size());
    for (const auto& item : data) {
        bool validItem = item.contains("label") && item.contains("global") && item.contains("id");
        if (!validItem) continue;
        Rank faction{};
        faction.global = TESForm::LookupByEditorID<TESGlobal>(item.at("global").get<std::string>());
        if (!faction.global) validItem = false;
        if (item.contains("questList")) {
            faction.questList = TESForm::LookupByEditorID<BGSListForm>(item.at("questList").get<std::string>());
            if (!faction.questList) validItem = false;
        }
        if (validItem) {
            faction.id = item.at("id").get<std::string>();
            faction.label = item.at("label").get<std::string>();
            rankData.emplace_back(std::move(faction));
        }
    }
}

static void LoadConfigFile() {
    const std::filesystem::path file = "Data/SKSE/Plugins/FactionRanks.json";
    if (std::filesystem::exists(file)) {
        std::ifstream ifile{file};
        if (!ifile) return;
        try {
            json data = json::parse(ifile);
            if (data.is_discarded()) return;
            ParseData(data);
        } catch (...) {
        }
    }
}

// A roundabout way to create SKSE mod events
// If you know how to do this properly let me know ;)
static void DispatchEvent(Rank& a_rank) {
    auto ui = UI::GetSingleton();
    if (!ui) return;

    GPtr<IMenu> menu = ui->GetMenu(HUDMenu::MENU_NAME);
    if (!menu || !menu->uiMovie) {
        return;
    }

    auto movie = menu->uiMovie;

    GFxValue skse;
    movie->GetVariable(&skse, "_global.skse");
    GFxValue args[3];
    args[0] = "FactionRanks_RankUp";
    args[1] = a_rank.id;
    args[2] = a_rank.global->value;
    skse.Invoke("SendModEvent", nullptr, args, 3);
}

static int GetPlayerRank(Rank& a_rank) {
    if (!a_rank.questList) return 0;
    int count = 0;
    a_rank.questList->ForEachForm([&](TESForm& formItem) {
        if (formItem.GetFormType() == FormType::Quest) {
            TESQuest* quest = formItem.As<TESQuest>();
            if (quest->IsCompleted()) {
                count++;
            }
        }
        return BSContainer::ForEachResult::kContinue;
    });
    return count;
}

class MyEventSink : public BSTEventSink<MenuOpenCloseEvent>, public BSTEventSink<TESQuestStageEvent> {
public:
    BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent* event, BSTEventSource<MenuOpenCloseEvent>*) {
        if (event->menuName == JournalMenu::MENU_NAME && event->opening) {
            Inject(event->menuName);
        }
        return BSEventNotifyControl::kContinue;
    }

    BSEventNotifyControl ProcessEvent(const TESQuestStageEvent* event, BSTEventSource<TESQuestStageEvent>*) {
        if (TESQuest* quest = TESForm::LookupByID<TESQuest>(event->formID); quest) {
            if (!quest->IsCompleted()) return BSEventNotifyControl::kContinue;
            for (int i = 0; i < rankData.size(); i++) {
                if (rankData[i].questList && rankData[i].questList->HasForm(quest)) {
                    int newRank = GetPlayerRank(rankData[i]);
                    if (newRank != rankData[i].global->value) {
                        rankData[i].global->value = newRank;
                        DispatchEvent(rankData[i]);
                    }
                    break;
                }
            }
        }
        return BSEventNotifyControl::kContinue;
    }
};

static void OnLoadSaveGame() {
    for (int i = 0; i < rankData.size(); i++) {
        int rank = GetPlayerRank(rankData[i]);
        rankData[i].global->value = rank;
    }
}

static void OnDataLoad() {
    LoadConfigFile();
    if (!rankData.empty()) {
        static MyEventSink g_EventSink;
        UI::GetSingleton()->AddEventSink<MenuOpenCloseEvent>(&g_EventSink);
        ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESQuestStageEvent>(&g_EventSink);
    }
}

int GetContainerMode(StaticFunctionTag*) {
    if (auto* ui = UI::GetSingleton(); ui) {
        if (auto containerMenu = ui->GetMenu<ContainerMenu>(); containerMenu) {
            return static_cast<int>(containerMenu->GetContainerMode());
        }
    }
    return -1;
}

bool PapyrusBinder(BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("GetContainerMode", "FactionRanks_Utils", GetContainerMode);

    return false;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetPapyrusInterface()->Register(PapyrusBinder);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            OnDataLoad();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            if (!rankData.empty()) {
                OnLoadSaveGame();
            }
        }
    });

    return true;
}