#include "UIUtils.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace UIUtils
{
const char* GetVariantTypeName(eVariantTypes type)
{
    switch (type)
    {
        case VARIANT_TYPE_INT:
            return "INT";
        case VARIANT_TYPE_UINT:
            return "UINT";
        case VARIANT_TYPE_FLOAT:
            return "FLOAT";
        case VARIANT_TYPE_BOOL:
            return "BOOL";
        case VARIANT_TYPE_STRING:
            return "STRING";
        case VARIANT_TYPE_VECTOR2INT:
            return "VECTOR2_INT";
        case VARIANT_TYPE_VECTOR2FLOAT:
            return "VECTOR2_FLOAT";
        case VARIANT_TYPE_VECTOR3INT:
            return "VECTOR3_INT";
        case VARIANT_TYPE_VECTOR3FLOAT:
            return "VECTOR3_FLOAT";
        default:
            return "NONE";
    }
}

string GetVariantValueString(const Variant& var)
{
    switch (var.GetType())
    {
        case VARIANT_TYPE_INT:
            return std::to_string(var.GetINT());
        case VARIANT_TYPE_UINT:
            return std::to_string(var.GetUINT());
        case VARIANT_TYPE_FLOAT:
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", var.GetFloat());
            return buf;
        }
        case VARIANT_TYPE_BOOL:
            return var.GetBool() ? "true" : "false";
        case VARIANT_TYPE_STRING:
            return var.GetString();
        case VARIANT_TYPE_VECTOR2INT:
        {
            auto v = var.GetVector2Int();
            char buf[64];
            snprintf(buf, sizeof(buf), "X: %d, Y: %d", v.x, v.y);
            return buf;
        }
        case VARIANT_TYPE_VECTOR2FLOAT:
        {
            auto v = var.GetVector2Float();
            char buf[64];
            snprintf(buf, sizeof(buf), "X: %.2f, Y: %.2f", v.x, v.y);
            return buf;
        }
        case VARIANT_TYPE_VECTOR3INT:
        {
            auto v = var.GetVector3Int();
            char buf[64];
            snprintf(buf, sizeof(buf), "X: %d, Y: %d, Z: %d", v.x, v.y, v.z);
            return buf;
        }
        case VARIANT_TYPE_VECTOR3FLOAT:
        {
            auto v = var.GetVector3Float();
            char buf[64];
            snprintf(buf, sizeof(buf), "X: %.2f, Y: %.2f, Z: %.2f", v.x, v.y, v.z);
            return buf;
        }
        default:
            return "";
    }
}

// stolen :)
bool CaseInsensitiveContains(const char* haystack, const string& needleLower)
{
    if (!haystack || *haystack == '\0' || needleLower.empty())
        return true;

    for (; *haystack; ++haystack)
    {
        const char* h = haystack;
        const char* n = needleLower.c_str();
        while (*h && *n && (std::tolower(static_cast<unsigned char>(*h)) == *n))
        {
            ++h;
            ++n;
        }
        if (*n == '\0')
            return true;
    }
    return false;
}
} // namespace UIUtils