#include "pch.h"

#include "player.h"
#include "enums.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"

Shared::Status Player::status("Player");
Player::Snapshot Player::snapshot;

int Player::ownerMeetSpotIndex = 0;
int Player::ownerCrewIndex = 0;
float Player::ownerStrength = 0.0f;
std::atomic<int> Player::loadedDataVersion{ 0 };
std::atomic_bool Player::loadedDataValid{ false };
std::atomic<int> Player::loadedOwnerIndex{ -1 };
std::atomic<float> Player::loadedStrength{ 0.0f };
//std::atomic<int> Player::loadedLastUpdatedDay{ 0 };
//std::atomic<float> Player::loadedLastDaySkyLerp{ 0.0f };
int Player::seenDataVersion = 0;

/*int Player::moneyToAdd = 0;
float Player::repToAdd = 0.0f;
int Player::meetSpotIndex = 0;
float Player::nightRepToAdd = 0.0f;
int Player::debtToAdd = 0;
int Player::bettingMoneyToAdd = 0;*/

// Apply actions

void Player::ApplyRaceCrewType(int index) {
    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        const std::vector<Il2Cpp::EnumMember>& raceCrewTypes = Enums::RaceCrews();
        if (!god || index < 0 || index >= static_cast<int>(raceCrewTypes.size())) return;

        if (!Shared::SetEnumField(god, "player_raceCrew", raceCrewTypes[index])) {
            status.Set("player_raceCrew field not found");
            return;
        }

        // Persist the choice with the save system the game itself uses:
        // ES3.Save("player_raceCrew", <Race_Crews>, GodConstant.es3_settings).
        // ES3 lives in Assembly-CSharp-firstpass, so it needs that image
        Il2CppClass* es3Class = Il2Cpp::FindClass("ES3", Il2Cpp::GetImage("Assembly-CSharp-firstpass"));
        Il2CppClass* raceCrewClass = Il2Cpp::FindClass("GodConstant.Race_Crews");
        if (!es3Class || !raceCrewClass) {
            status.Set("Race crew set (ES3 unavailable)");
            return;
        }

        const MethodInfo* save = Il2Cpp::GetMethodByParamClass(es3Class, "Save", 3, 2, "ES3Settings");
        if (!save) {
            status.Set("Race crew set (ES3.Save not found)");
            return;
        }

        void* key = Il2Cpp::NewString("player_raceCrew");
        Il2CppObject* boxed = Il2Cpp::BoxValue(raceCrewClass, raceCrewTypes[index].raw.data());
        Il2CppObject* settings = Il2Cpp::GetInstanceFieldObject(god, "es3_settings");
        if (!key || !boxed || !settings) {
            status.Set("Race crew set (ES3 save failed)");
            return;
        }

        void* args[3] = {key, boxed, settings};
        Il2Cpp::Invoke(save, nullptr, args);

        status.Set("Race crew set to " + raceCrewTypes[index].name);
    });
}

void Player::EndNight() {
    MainThread::Post([]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        if (!godClass) { status.Set("GodConstant not found"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(godClass, "nightEnd_start", 1);
        if (!method) { status.Set("nightEnd_start not found"); return; }

        bool endedNightManually = true;
        void* args[1] = {&endedNightManually};
        Il2Cpp::Invoke(method, Shared::God(), args);

        status.Set("Ended the night");
    });
}

std::string Player::MeetspotName(int32_t id) {
    // The MeetSpots_All literal is the same numeric value as
    // changeScene.targetMeetSpot, so the enum name doubles as a display name
    return Enums::NameOf(Enums::Id::MeetSpots, id, "Meetspot ");
}

Il2CppObject* Player::LoadMeetspotData(int32_t id) {
    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    Il2CppObject* god = Shared::God();
    if (!godClass || !god) return nullptr;

    const MethodInfo* method = Il2Cpp::GetMethod(godClass, "LoadMeetspotData", 1);
    if (!method) return nullptr;

    int32_t idArg = id;
    void* args[1] = {&idArg};
    return Il2Cpp::Invoke(method, god, args);
}

bool Player::SaveMeetspotData(int32_t id, Il2CppObject* data) {
    if (!data) return false;

    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    Il2CppObject* god = Shared::God();
    if (!godClass || !god) return false;

    const MethodInfo* method = Il2Cpp::GetMethod(godClass, "SaveMeetspotData", 2);
    if (!method) return false;

    int32_t idArg = id;
    void* args[2] = {&idArg, data};
    Il2Cpp::Invoke(method, god, args);
    return true;
}

void Player::SelectOwnerMeetspot(int index) {
    const std::vector<Il2Cpp::EnumMember>& meetSpots = Enums::MeetSpots();
    ownerMeetSpotIndex = index;
    if (index < 0 || index >= static_cast<int>(meetSpots.size())) return;

    int32_t id = 0;
    memcpy(&id, meetSpots[index].raw.data(), sizeof(id));

    MainThread::Post([id]() {
        Il2CppObject* data = LoadMeetspotData(id);
        if (!data) { status.Set("MeetspotData not found"); return; }

        int32_t owner = -1;
        float strength = 0.0f;
        //int32_t lastUpdatedDay = 0;
        //float lastDaySkyLerp = 0.0f;
        Il2Cpp::GetInstanceFieldValue(data, "owner", owner);
        Il2Cpp::GetInstanceFieldValue(data, "strength", strength);
        //Il2Cpp::GetInstanceFieldValue(data, "lastUpdatedDay", lastUpdatedDay);
        //Il2Cpp::GetInstanceFieldValue(data, "lastDaySkyLerp", lastDaySkyLerp);

        loadedOwnerIndex.store(Enums::IndexOf(Enums::Id::RaceCrews, &owner, sizeof(owner)), std::memory_order_relaxed);
        loadedStrength.store(strength, std::memory_order_relaxed);
        //loadedLastUpdatedDay.store(lastUpdatedDay, std::memory_order_relaxed);
        //loadedLastDaySkyLerp.store(lastDaySkyLerp, std::memory_order_relaxed);
        loadedDataValid.store(true, std::memory_order_relaxed);
        loadedDataVersion.fetch_add(1, std::memory_order_release);

        status.Set("Loaded " + MeetspotName(id));
    });
}

void Player::SaveOwnerMeetspot() {
    const int spotIndex = ownerMeetSpotIndex;
    const int crewIndex = ownerCrewIndex;
    const float strength = ownerStrength;

    const std::vector<Il2Cpp::EnumMember>& meetSpots = Enums::MeetSpots();
    const std::vector<Il2Cpp::EnumMember>& raceCrewTypes = Enums::RaceCrews();

    if (spotIndex < 0 || spotIndex >= static_cast<int>(meetSpots.size())) { status.Set("Meetspot not loaded"); return; }
    if (crewIndex < 0 || crewIndex >= static_cast<int>(raceCrewTypes.size())) { status.Set("Owner not loaded"); return; }

    int32_t id = 0;
    memcpy(&id, meetSpots[spotIndex].raw.data(), sizeof(id));
    const Il2Cpp::EnumMember crew = raceCrewTypes[crewIndex];

    MainThread::Post([id, crew, strength, crewIndex]() {
        Il2CppObject* data = LoadMeetspotData(id);
        if (!data) { status.Set("MeetspotData not found"); return; }

        Shared::SetEnumField(data, "owner", crew);
        Il2Cpp::SetInstanceFieldValue(data, "strength", strength);

        if (!SaveMeetspotData(id, data)) { status.Set("SaveMeetspotData not found"); return; }

        loadedOwnerIndex.store(crewIndex, std::memory_order_relaxed);
        loadedStrength.store(strength, std::memory_order_relaxed);
        loadedDataValid.store(true, std::memory_order_relaxed);
        loadedDataVersion.fetch_add(1, std::memory_order_release);

        status.Set("Saved owner of " + MeetspotName(id));
    });
}

void Player::UnlockAllParts() {
    MainThread::Post([]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        if (!godClass) { status.Set("GodConstant not found"); return; }

        Il2CppObject* god = Shared::God();

        Il2Cpp::SetInstanceFieldValue(god, "player_gameRep", kUnlockGameRep);

        const MethodInfo* check = Il2Cpp::GetMethod(godClass, "check_forPlayerUnlock", 0);
        if (check) Il2Cpp::Invoke(check, god, nullptr);

        status.Set("Unlocked all parts");
    });
}

// Todo: rethink how we implement it
/*void Player::AddMoneyRep() {
    const int32_t money = moneyToAdd;
    const float rep = repToAdd;
    const float nightRep = nightRepToAdd;
    const int32_t debt = debtToAdd;
    const int32_t betting = bettingMoneyToAdd;
    const int spot = meetSpotIndex;

    MainThread::Post([money, rep, nightRep, debt, betting, spot]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        if (!godClass) { status.Set("GodConstant not found"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(godClass, "changeMoneyRep", 6);
        if (!method) { status.Set("changeMoneyRep not found"); return; }

        if (meetSpots.empty() || spot < 0 || spot >= static_cast<int>(meetSpots.size())) {
            status.Set("Meet spot not loaded");
            return;
        }

        int32_t moneyArg = money;
        float repArg = rep;
        float nightRepArg = nightRep;
        int32_t debtArg = debt;
        int32_t bettingArg = betting;
        void* args[6] = {
            &moneyArg,
            &repArg,
            meetSpots[spot].raw.data(),
            &nightRepArg,
            &debtArg,
            &bettingArg,
        };
        Il2Cpp::Invoke(method, Shared::God(), args);

        status.Set("Added money / reputation");
    });
}*/

// Snapshot / tab

void Player::RefreshSnapshot() {
    Il2Cpp::ThreadAttach();

    if (Enums::Ready()) {
        snapshot.raceCrewTypeIndex.store(Enums::SyncIndex(Enums::Id::RaceCrews, Shared::God(), "player_raceCrew"), std::memory_order_relaxed);
    }
}

void Player::RenderTab() {
    if (!Shared::BeginGameTab("Player")) return;

    status.Render();

    // Race crew types
    if (!Enums::Ready() || Enums::RaceCrews().empty()) {
        ImGui::TextUnformatted("Race crew types unavailable.");
    } else {
        Shared::RenderEnumCombo("Race crew", Enums::RaceCrews(), snapshot.raceCrewTypeIndex.load(std::memory_order_relaxed), ApplyRaceCrewType);
    }

    if (ImGui::Button("End Night")) {
        EndNight();
    }
    ImGui::SameLine();
    if (ImGui::Button("Unlock All Parts")) {
        UnlockAllParts();
    }

    ImGui::SeparatorText("Meetspot data");

    // A completed LoadMeetspotData/SaveMeetspotData bumps the version; seed the
    // edit fields from it once.
    const int dataVersion = loadedDataVersion.load(std::memory_order_acquire);
    if (dataVersion != seenDataVersion) {
        seenDataVersion = dataVersion;
        if (loadedDataValid.load(std::memory_order_relaxed)) {
            ownerCrewIndex = loadedOwnerIndex.load(std::memory_order_relaxed);
            ownerStrength = loadedStrength.load(std::memory_order_relaxed);
        }
    }

    if (!Enums::Ready() || Enums::MeetSpots().empty()) {
        ImGui::TextUnformatted("Meet spots unavailable.");
    } else {
        const std::vector<Il2Cpp::EnumMember>& meetSpots = Enums::MeetSpots();
        if (ownerMeetSpotIndex < 0 || ownerMeetSpotIndex >= static_cast<int>(meetSpots.size())) ownerMeetSpotIndex = 0;
        Shared::RenderEnumCombo("Meetspot", meetSpots, ownerMeetSpotIndex, SelectOwnerMeetspot);
    }

    if (!Enums::Ready() || Enums::RaceCrews().empty()) {
        ImGui::TextUnformatted("Race crews unavailable.");
    } else {
        const std::vector<Il2Cpp::EnumMember>& raceCrewTypes = Enums::RaceCrews();
        if (ownerCrewIndex < 0 || ownerCrewIndex >= static_cast<int>(raceCrewTypes.size())) ownerCrewIndex = 0;
        Shared::RenderEnumCombo("Owner", raceCrewTypes, ownerCrewIndex, [](int index) { ownerCrewIndex = index; });
    }

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("Strength", &ownerStrength, 1.0f, 10.0f, "%.1f");

    if (ImGui::Button("Save")) {
        SaveOwnerMeetspot();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        SelectOwnerMeetspot(ownerMeetSpotIndex);
    }

    /*ImGui::SeparatorText("Money / Reputation");

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Money", &moneyToAdd);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("Reputation", &repToAdd, 1.0f, 100.0f, "%.1f");
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("Night reputation", &nightRepToAdd, 1.0f, 100.0f, "%.1f");
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Debt", &debtToAdd);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Betting money", &bettingMoneyToAdd);

    EnsureMeetSpots();
    if (!meetSpotsLoaded.load(std::memory_order_acquire) || meetSpots.empty()) {
        ImGui::TextUnformatted("Meet spots unavailable.");
    } else {
        if (meetSpotIndex < 0 || meetSpotIndex >= (int)meetSpots.size()) meetSpotIndex = 0;
        RenderEnumCombo("Meet spot", meetSpots, meetSpotIndex, [](int index) { meetSpotIndex = index; });
    }

    if (ImGui::Button("Add Money / Rep")) {
        AddMoneyRep();
    }*/

    ImGui::EndTabItem();
}
