#include "pch.h"
#include "OrganizeMyGarageOS.h"

#include "OmgView.h"
#include "GarageModel.h"
#include "InventoryModel.h"
#include "RandomPresetSelector.h"
#include "utils/parser.h"

BAKKESMOD_PLUGIN(OrganizeMyGarageOS, "Garage organizer", plugin_version, PLUGINTYPE_FREEPLAY)

void OrganizeMyGarageOS::onLoad()
{
	CVarManagerSingleton::getInstance().SetCvarManager(cvarManager);
	persistentStorage = std::make_shared<PersistentStorage>(this, "OMG", true, true);

	inventoryModel = std::make_shared<InventoryModel>(gameWrapper);
	garageModel = std::make_shared<GarageModel>(gameWrapper, inventoryModel);
	garageModel->RefreshPresets();
	garageModel->RefreshEquippedIndex();
	auto randomPresetSelector = std::make_shared<RandomPresetSelector>(gameWrapper, persistentStorage, garageModel, inventoryModel);
	view = std::make_shared<OmgView>(garageModel, inventoryModel, gameWrapper, randomPresetSelector);

	MonitorPresetChanges();
	RegisterConsoleCommands();
}

void OrganizeMyGarageOS::MonitorPresetChanges() const
{
	gameWrapper->HookEventPost("Function TAGame.GFxData_LoadoutSets_TA.HandleLoadoutLoaded", [this](...)
	{
		garageModel->RefreshEquippedIndex();
	});
}

void OrganizeMyGarageOS::RegisterConsoleCommands() const
{
	cvarManager->registerNotifier("omg_preset_equip", [&](std::vector<std::string> args)
	{
		if (args.size() != 2)
		{
			cvarManager->log("Usage:" + args[0] + "<preset_name>");
			return;
		}
		garageModel->EquipPreset(args[1]);
	}, "", 0);

	cvarManager->registerNotifier("omg_preset_delete", [&](std::vector<std::string> args)
	{
		if (args.size() != 2)
		{
			cvarManager->log("Usage:" + args[0] + "<preset_name>");
			return;
		}
		garageModel->DeletePreset(args[1]);
	}, "", 0);

	cvarManager->registerNotifier("omg_preset_add", [&](std::vector<std::string> args)
	{
		auto newPreset = garageModel->AddPreset();
		cvarManager->log("New preset is named: " + newPreset.GetName());
	}, "", 0);


	cvarManager->registerNotifier("omg_preset_copy", [&](std::vector<std::string> args)
	{
		if (args.size() != 2)
		{
			cvarManager->log("Usage:" + args[0] + "<preset_name>");
			return;
		}
		garageModel->CopyPreset(args[1]);
	}, "", 0);

	cvarManager->registerNotifier("omg_preview_team", [&](std::vector<std::string> args)
	{
		if (args.size() != 2)
		{
			cvarManager->log("Usage:" + args[0] + "<preset_name>");
			return;
		}
		int teamIndex = get_safe_int(args[1]);
		auto save = gameWrapper->GetUserLoadoutSave();
		save.SetPreviewTeam(teamIndex);
	}, "", 0);
}


void OrganizeMyGarageOS::RenderSettings()
{
	view->Render();
}

void OrganizeMyGarageOS::RenderWindow()
{
	view->Render();
}
