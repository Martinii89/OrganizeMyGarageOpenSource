#pragma once
#include "bakkesmod/wrappers/wrapperstructs.h"

#define IM_VEC4_CLASS_EXTRA                                                 \
        ImVec4(const LinearColor& f) { x = f.R; y = f.G; z = f.B; w = f.A; }     \
        operator LinearColor() const { return LinearColor(x,y,z,w); }
