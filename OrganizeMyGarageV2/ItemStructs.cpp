#include "pch.h"
#include "ItemStructs.h"

bool OnlineProdData::IsBodyCompatible(int body) const
{
	if (compatibleBodies.empty())
	{
		return true;
	}
	return compatibleBodies.contains(body);
}

void OnlineProdData::SetOfflineProductData(ProductWrapper& productWrapper)
{
	if (!productWrapper)
	{
		DEBUGLOG("null productWrapper");
		return;
	}
	prodId = productWrapper.GetID();
	//canEquip = product_wrapper.CanEquip();
	name = productWrapper.GetLongLabel().ToString();
	slot = productWrapper.GetSlot().GetSlotIndex();
	DEBUGLOG("slot: {}", slot);

	if (auto required = productWrapper.GetRequiredProduct())
	{
		compatibleBodies.insert(required.GetID());
	}

	for (auto attribute : productWrapper.GetAttributes())
	{
		//DEBUGLOG("{}, {}", attribute.GetAttributeType(), attribute.GetAttributeType());
		if (attribute.GetAttributeType() == "ProductAttribute_BodyCompatibility_TA")
		{
			auto bodyCompatibility = ProductAttribute_BodyCompatibilityWrapper(attribute.memory_address);
			for (auto body : bodyCompatibility.GetCompatibleBodies())
			{
				if (body)
				{
					compatibleBodies.insert(body.GetID());
				}
			}
		}
	}
}

std::string OnlineProdData::ToString() const
{
	return std::format("Name: {}, Id: {}, Fav: {}, Slot: {}, Lo: {}, Hi: {}",
		name, prodId, favorite, slot, instanceId.lower_bits, instanceId.upper_bits);
}
