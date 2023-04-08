#pragma once
#include <set>

struct OnlineProdData {
	int prodId = 0;
	std::string name;
	ProductInstanceID instanceId = {0,0};
	std::string paint;
	int slot = -1;
	bool canEquip = false;
	std::set<int> compatibleBodies;

	[[nodiscard]] bool IsBodyCompatible(int body) const;

	void SetOfflineProductData(ProductWrapper& productWrapper);
};

struct PaintData
{
	int primaryId = 0;
	int accentId = 0;
	LinearColor primaryColor = {};
	LinearColor accentColor = {};
};

struct PresetData {
	std::string name;
	PaintData color1;
	PaintData color2;
	std::vector<OnlineProdData> loadout = {};
	std::vector<OnlineProdData> loadout2 = {};
};