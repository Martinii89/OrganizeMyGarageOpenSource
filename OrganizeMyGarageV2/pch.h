#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

#define IMGUI_USER_CONFIG "imgui_user_config.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_stdlib.h"
#include "IMGUI/imgui_searchablecombo.h"
#include "IMGUI/imgui_rangeslider.h"

#include "logging.h"

inline bool operator ==(const ProductInstanceID& lhs,const ProductInstanceID& rhs)
{
	return lhs.lower_bits == rhs.lower_bits
		&& lhs.upper_bits == rhs.upper_bits;
}