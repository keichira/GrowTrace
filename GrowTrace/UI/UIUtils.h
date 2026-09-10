#pragma once

#include "../Editor/Panel.h"
#include "Packet/GamePacket.h"
#include "Precompiled.h"
#include "Proton/ProtonUtils.h"
#include "imgui.h"

namespace UIUtils
{
const char* GetVariantTypeName(eVariantTypes type);
string GetVariantValueString(const Variant& var);
bool CaseInsensitiveContains(const char* haystack, const string& needleLower);

} // namespace UIUtils