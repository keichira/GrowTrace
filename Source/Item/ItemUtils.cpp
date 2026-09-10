#include "ItemUtils.h"
#include "../Utils/StringUtils.h"
#include "ItemInfo.h"

const char* GetItemSlipperyType(int8 type)
{
    static const char* ITEM_SLIP_TYPE_STR[] = {"NONE", "SLIPPERT", "NO_SLIP"};
    if (type < sizeof(ITEM_SLIP_TYPE_STR) / sizeof(ITEM_SLIP_TYPE_STR[0]))
        return ITEM_SLIP_TYPE_STR[type];
    return "NONE";
}

const char* GetItemTypeRawStr(uint8 type)
{
    static const char* ITEM_TYPE_STR[] = {"TYPE_FIST",
                                          "TYPE_WRENCH",
                                          "TYPE_USER_DOOR",
                                          "TYPE_LOCK",
                                          "TYPE_GEMS",
                                          "TYPE_TREASURE",
                                          "TYPE_DEADLY",
                                          "TYPE_TRAMPOLINE",
                                          "TYPE_CONSUMABLE",
                                          "TYPE_GATEWAY",
                                          "TYPE_SIGN",
                                          "TYPE_SFX_WITH_EXTRA_FRAME",
                                          "TYPE_BOOMBOX",
                                          "TYPE_DOOR",
                                          "TYPE_PLATFORM",
                                          "TYPE_BEDROCK",
                                          "TYPE_LAVA",
                                          "TYPE_NORMAL",
                                          "TYPE_BACKGROUND",
                                          "TYPE_SEED",
                                          "TYPE_CLOTHES",
                                          "TYPE_NORMAL_WITH_EXTRA_FRAME",
                                          "TYPE_BACKGD_SFX_EXTRA_FRAME",
                                          "TYPE_BACK_BOOMBOX",
                                          "TYPE_BOUNCY",
                                          "TYPE_POINTY",
                                          "TYPE_PORTAL",
                                          "TYPE_CHECKPOINT",
                                          "TYPE_MUSICNOTE",
                                          "TYPE_ICE",
                                          "TYPE_RACE_FLAG",
                                          "TYPE_SWITCHEROO",
                                          "TYPE_CHEST",
                                          "TYPE_MAILBOX",
                                          "TYPE_BULLETIN",
                                          "TYPE_PINATA",
                                          "TYPE_COMPONENT",
                                          "TYPE_DICE",
                                          "TYPE_PROVIDER",
                                          "TYPE_LAB",
                                          "TYPE_ACHIEVEMENT",
                                          "TYPE_WEATHER_MACHINE",
                                          "TYPE_SCOREBOARD",
                                          "TYPE_SUNGATE",
                                          "TYPE_PROFILE",
                                          "TYPE_DEADLY_IF_ON",
                                          "TYPE_HEART_MONITOR",
                                          "TYPE_DONATION_BOX",
                                          "TYPE_TOYBOX",
                                          "TYPE_MANNEQUIN",
                                          "TYPE_CAMERA",
                                          "TYPE_MAGICEGG",
                                          "TYPE_TEAM",
                                          "TYPE_GAME_GEN",
                                          "TYPE_XENONITE",
                                          "TYPE_DRESSUP",
                                          "TYPE_CRYSTAL",
                                          "TYPE_BURGLAR",
                                          "TYPE_COMPACTOR",
                                          "TYPE_SPOTLIGHT",
                                          "TYPE_WIND",
                                          "TYPE_DISPLAY_BLOCK",
                                          "TYPE_VENDING",
                                          "TYPE_FISHTANK",
                                          "TYPE_PETFISH",
                                          "TYPE_SOLAR",
                                          "TYPE_FORGE",
                                          "TYPE_GIVING_TREE",
                                          "TYPE_GIVING_TREE_STUMP",
                                          "TYPE_STEAMPUNK",
                                          "TYPE_STEAM_LAVA_IF_ON",
                                          "TYPE_STEAM_ORGAN",
                                          "TYPE_TAMAGOTCHI",
                                          "TYPE_SEWING",
                                          "TYPE_FLAG",
                                          "TYPE_LOBSTER_TRAP",
                                          "TYPE_ARTCANVAS",
                                          "TYPE_BATTLE_CAGE",
                                          "TYPE_PET_TRAINER",
                                          "TYPE_STEAM_ENGINE",
                                          "TYPE_LOCK_BOT",
                                          "TYPE_WEATHER_SPECIAL",
                                          "TYPE_SPIRIT_STORAGE",
                                          "TYPE_DISPLAY_SHELF",
                                          "TYPE_VIP_DOOR",
                                          "TYPE_CHAL_TIMER",
                                          "TYPE_CHAL_FLAG",
                                          "TYPE_FISH_MOUNT",
                                          "TYPE_PORTRAIT",
                                          "TYPE_WEATHER_SPECIAL2",
                                          "TYPE_FOSSIL",
                                          "TYPE_FOSSIL_PREP",
                                          "TYPE_DNA_MACHINE",
                                          "TYPE_BLASTER",
                                          "TYPE_VALHOWLA",
                                          "TYPE_CHEMSYNTH",
                                          "TYPE_CHEMTANK",
                                          "TYPE_STORAGE",
                                          "TYPE_OVEN",
                                          "TYPE_SUPER_MUSIC",
                                          "TYPE_GEIGERCHARGE",
                                          "TYPE_ADVENTURE_RESET",
                                          "TYPE_TOMB_ROBBER",
                                          "TYPE_FACTION",
                                          "TYPE_RED_FACTION",
                                          "TYPE_GREEN_FACTION",
                                          "TYPE_BLUE_FACTION",
                                          "TYPE_ARTIFACT",
                                          "TYPE_TRAMPOLINE_MOMENTUM",
                                          "TYPE_FISHGOTCHI_TANK",
                                          "TYPE_FISHING_BLOCK",
                                          "TYPE_SUCKER",
                                          "TYPE_PLANTER",
                                          "TYPE_ROBOT",
                                          "TYPE_COMMAND",
                                          "TYPE_LUCKY_TICKET",
                                          "TYPE_STATS_BLOCK",
                                          "TYPE_FIELD_NODE",
                                          "TYPE_OUIJA_BOARD",
                                          "TYPE_ARCHITECT_MACHINE",
                                          "TYPE_STARSHIP",
                                          "TYPE_AUTODELETE",
                                          "TYPE_BOOMBOX2",
                                          "TYPE_AUTO_ACTION_BREAK",
                                          "TYPE_AUTO_ACTION_HARVEST",
                                          "TYPE_AUTO_ACTION_HARVEST_SUCK",
                                          "TYPE_LIGHTNING_CLOUD",
                                          "TYPE_PHASED_BLOCK",
                                          "TYPE_MUD",
                                          "TYPE_ROOT_CUTTING",
                                          "TYPE_PASSWORD_STORAGE",
                                          "TYPE_PHASED_BLOCK_2",
                                          "TYPE_BOMB",
                                          "TYPE_PVE_NPC",
                                          "TYPE_INFINITY_WEATHER_MACHINE",
                                          "TYPE_SLIME",
                                          "TYPE_ACID",
                                          "TYPE_COMPLETIONIST",
                                          "TYPE_PUNCH_TOGGLE",
                                          "TYPE_ANZU_BLOCK",
                                          "TYPE_FEEDING_BLOCK",
                                          "TYPE_KRANKENS_BLOCK",
                                          "TYPE_FRIENDS_ENTRANCE",
                                          "TYPE_PEARLS"};
    if (type < sizeof(ITEM_TYPE_STR) / sizeof(ITEM_TYPE_STR[0]))
        return ITEM_TYPE_STR[type];
    return "TYPE_NORMAL";
}

const char* GetItemMaterialRawStr(uint8 mat)
{
    static const char* ITEM_MATERIAL_STR[] = {"MATERIAL_WOOD", "MATERIAL_GLASS", "MATERIAL_ROCK", "MATERIAL_METAL"};
    if (mat < 4)
        return ITEM_MATERIAL_STR[mat];
    return "MATERIAL_WOOD";
}

const char* GetItemStorageTypeRawStr(uint8 storage)
{
    static const char* ITEM_STORAGE_STR[] = {"STORAGE_SINGLE_FRAME_ALONE",
                                             "STORAGE_SINGLE_FRAME",
                                             "STORAGE_SMART_EDGE",
                                             "STORAGE_SMART_EDGE_HORIZ",
                                             "STORAGE_SMART_CLING",
                                             "STORAGE_SMART_EDGE_OUTER",
                                             "STORAGE_RANDOM",
                                             "STORAGE_SMART_EDGE_VERT",
                                             "STORAGE_SMART_EDGE_HORIZ_CAVE",
                                             "STORAGE_SMART_CLING2",
                                             "STORAGE_SMART_EDGE_DIAGON"};
    if (storage < 11)
        return ITEM_STORAGE_STR[storage];
    return "STORAGE_SINGLE_FRAME_ALONE";
}

const char* GetItemCollisionTypeRawStr(uint8 coll)
{
    static const char* ITEM_COLLISION_STR[] = {
        "COLLISION_NONE",      "COLLISION_SOLID",           "COLLISION_JUMP_THROUGH", "COLLISION_GATEWAY",
        "COLLISION_IF_OFF",    "COLLISION_ONE_WAY",         "COLLISION_VIP",          "COLLISION_JUMP_DOWN",
        "COLLISION_ADVENTURE", "COLLISION_IF_ON",           "COLLISION_FACTION",      "COLLISION_GUILD",
        "COLLISION_CLOUD",     "COLLISION_FRIENDS_ENTRANCE"};
    if (coll < 14)
        return ITEM_COLLISION_STR[coll];
    return "COLLISION_NONE";
}

const char* GetItemBodyPartRawStr(uint8 bodyPart)
{
    static const char* ITEM_BODY_PART_STR[] = {"HAT",  "SHIRT", "PANT", "SHOE",     "FACEITEM",
                                               "HAND", "BACK",  "HAIR", "CHESTITEM"};
    if (bodyPart < 9)
        return ITEM_BODY_PART_STR[bodyPart];
    return "HAIR";
}

const char* GetItemVisualEffectRawStr(uint8 fx)
{
    static const char* VISUAL_EFFECT_STR[] = {"VISUAL_EFFECT_NORMAL",
                                              "VISUAL_EFFECT_FLAME_LICK",
                                              "VISUAL_EFFECT_SMOKING",
                                              "VISUAL_EFFECT_GLOW_TINT",
                                              "VISUAL_EFFECT_ANIM",
                                              "VISUAL_EFFECT_BUBBLES",
                                              "VISUAL_EFFECT_PET",
                                              "VISUAL_EFFECT_PET_ANIM",
                                              "VISUAL_EFFECT_NO_ARMS",
                                              "VISUAL_EFFECT_WAVEY",
                                              "VISUAL_EFFECT_WAVEY_ANIM",
                                              "VISUAL_EFFECT_BOTHARMS",
                                              "VISUAL_EFFECT_LOWHAIR",
                                              "VISUAL_EFFECT_UNDERFACE",
                                              "VISUAL_EFFECT_SKINTINT",
                                              "VISUAL_EFFECT_MASK",
                                              "VISUAL_EFFECT_ANIM_MASK",
                                              "VISUAL_EFFECT_LOWHAIR_MASK",
                                              "VISUAL_EFFECT_GHOST",
                                              "VISUAL_EFFECT_PULSE",
                                              "VISUAL_EFFECT_COLORIZE",
                                              "VISUAL_EFFECT_COLORIZE_TO_SHIRT",
                                              "VISUAL_EFFECT_COLORIZE_ANIM",
                                              "VISUAL_EFFECT_HIGHFACE",
                                              "VISUAL_EFFECT_HIGHFACE_ANIM",
                                              "VISUAL_EFFECT_RAINBOW_SHIFT",
                                              "VISUAL_EFFECT_BACKFORE",
                                              "VISUAL_EFFECT_COLORIZE_WITH_SKIN",
                                              "VISUAL_EFFECT_NO_RENDER",
                                              "VISUAL_EFFECT_SPIN",
                                              "VISUAL_EFFECT_OFFHAND",
                                              "VISUAL_EFFECT_WINGED",
                                              "VISUAL_EFFECT_SINK",
                                              "VISUAL_EFFECT_DARKNESS",
                                              "VISUAL_EFFECT_LIGHTSOURCE",
                                              "VISUAL_EFFECT_LIGHT_IF_ON",
                                              "VISUAL_EFFECT_DISCOLOR",
                                              "VISUAL_EFFECT_STEP_SPIN",
                                              "VISUAL_EFFECT_PETCOLORED",
                                              "VISUAL_EFFECT_SILKFOOT",
                                              "VISUAL_EFFECT_TILTY",
                                              "VISUAL_EFFECT_TILTY_DARK",
                                              "VISUAL_EFFECT_NEXT_FRAME_IF_ON",
                                              "VISUAL_EFFECT_WOBBLE",
                                              "VISUAL_EFFECT_SCROLL",
                                              "VISUAL_EFFECT_LIGHTSOURCE_PULSE",
                                              "VISUAL_EFFECT_BUBBLE_MACHINE",
                                              "VISUAL_EFFECT_VERYLOWHAIR",
                                              "VISUAL_EFFECT_VERYLOWHAIR_MASK"};
    if (fx < 49)
        return VISUAL_EFFECT_STR[fx];
    return "VISUAL_EFFECT_NORMAL";
}

const char* GetItemFlagRawStr(int32 flag)
{
    static const char* ITEM_FLAG_STR[] = {"FLIPPABLE", "EDITABLE", "SEEDLESS",    "PERMANENT", "DROPLESS",
                                          "NOSELF",    "RANDGROW", "WORLDLOCKED", "BETA",      "AUTOPICKUP",
                                          "MOD",       "PUBLIC",   "FOREGROUND",  "HOLIDAY",   "UNTRADEABLE"};
    if (flag >= 0 && flag < 15)
        return ITEM_FLAG_STR[flag];
    return "UNTRADEABLE";
}

const char* GetFxFlagRawStr(int32 flag)
{
    static const char* ITEM_FX_STR[] = {"MULTI_ANIM",
                                        "PING_PONG_ANIM",
                                        "OVERLAY_OBJECT",
                                        "OFFSET_UP",
                                        "DUAL_LAYER",
                                        "MULTI_ANIM2",
                                        "USE_SKIN_TINT",
                                        "SEED_TINT_LAYER1",
                                        "SEED_TINT_LAYER2",
                                        "RAINBOW_TINT_LAYER1",
                                        "RAINBOW_TINT_LAYER2",
                                        "GLOW",
                                        "NO_ARMS",
                                        "FRONT_ARM_PUNCH",
                                        "RENDER_OFFHAND",
                                        "SLOWFALL_OBJECT",
                                        "REPLACEMENT_SPRITE",
                                        "ORB_FLOAT",
                                        "RENDER_FX_VARIANT_VERSION"};
    if (flag >= 0 && flag < 19)
        return ITEM_FX_STR[flag];
    return "";
}

const char* GetFlag2RawStr(int32 flag)
{
    static const char* ITEM_FLAG2_STR[] = {
        "ROBOT_DEADLY",      "ROBOT_SHOOT_LEFT",     "ROBOT_SHOOT_RIGHT", "ROBOT_SHOOT_DOWN",    "ROBOT_SHOOT_UP",
        "ROBOT_CAN_SHOOT",   "ROBOT_LAVA",           "ROBOT_POINTY",      "ROBOT_SHOOT_DEADLY",  "GUILD_ITEM",
        "GUILD_FLAG",        "STARSHIP_HELM",        "STARSHIP_REACTOR",  "STARSHIP_VIEWSCREEN", "SMOD",
        "TILE_DEADLY_IF_ON", "LONG_HAND_ITEM64x32",  "GEMLESS",           "TRANSMUTABLE",        "DUNGEON_ITEM",
        "ONE_IN_WORLD",      "ONLY_FOR_WORLD_OWNER", "PVE_MELEE",         "PVE_RANGED",          "PVE_AUTOAIM",
        "NO_UPGRADE"};
    if (flag >= 0 && flag < 26)
        return ITEM_FLAG2_STR[flag];
    return "";
}

string BuildItemTextDefinition(const ItemInfo* pItem)
{
    if (!pItem)
        return "";

    string itemStr = "";

    if (pItem->type == ITEM_TYPE_CLOTHES)
    {
        itemStr += "add_cloth|";
        itemStr += ToString(pItem->id) + "|" + pItem->name + "|";
        itemStr += string(GetItemMaterialRawStr(pItem->material)) + "|";
        itemStr += pItem->textureFile + "|";
        itemStr += ToString(pItem->textureX) + "|" + ToString(pItem->textureY) + "|";
        itemStr += string(GetItemVisualEffectRawStr(pItem->visualEffect)) + "|";
        itemStr += string(GetItemStorageTypeRawStr(pItem->storage)) + "|";
        itemStr += string(GetItemBodyPartRawStr(pItem->bodyPart)) + "|\n";
    }
    else if (pItem->type == ITEM_TYPE_SEED)
    {
        itemStr += "set_seed|" + ToString(pItem->seed1) + "|" + ToString(pItem->seed2) + "|";
        itemStr += ToString(pItem->seedBgColor.r) + "," + ToString(pItem->seedBgColor.g) + "," +
                   ToString(pItem->seedBgColor.b) + "," + ToString(pItem->seedBgColor.a) + "|";
        itemStr += ToString(pItem->seedFgColor.r) + "," + ToString(pItem->seedFgColor.g) + "," +
                   ToString(pItem->seedFgColor.b) + "," + ToString(pItem->seedFgColor.a) + "|\n\n";
        return itemStr;
    }
    else
    {
        itemStr += "add_item|";
        itemStr += ToString(pItem->id) + "|" + pItem->name + "|";
        itemStr += string(GetItemTypeRawStr(pItem->type)) + "|";
        itemStr += string(GetItemMaterialRawStr(pItem->material)) + "|";
        itemStr += pItem->textureFile + "|";
        itemStr += ToString(pItem->textureX) + "|" + ToString(pItem->textureY) + "|";
        itemStr += string(GetItemVisualEffectRawStr(pItem->visualEffect)) + "|";
        itemStr += string(GetItemStorageTypeRawStr(pItem->storage)) + "|";
        itemStr += string(GetItemCollisionTypeRawStr(pItem->collisionType)) + "|";
        itemStr +=
            ToString(pItem->hp / 6) + "|" + ToString(pItem->restoreTime) + "|1|" + ToString(pItem->textureHash) + "|\n";
    }

    if (pItem->flags != 0)
    {
        std::vector<string> flagList;
        for (int32 i = 0; i < 16; ++i)
        {
            if (pItem->flags & (1 << i))
            {
                flagList.push_back(GetItemFlagRawStr(i));
            }
        }
        if (!flagList.empty())
        {
            itemStr += "set_flags|";
            for (auto& f : flagList)
                itemStr += f + "|";
            itemStr += "\n";
        }
    }

    if (pItem->flags2 != 0)
    {
        std::vector<string> flagList;
        for (int32 i = 0; i < 26; ++i)
        {
            if (pItem->flags2 & (1 << i))
            {
                const char* fStr = GetFlag2RawStr(i);
                if (fStr[0] != '\0')
                    flagList.push_back(fStr);
            }
        }
        if (!flagList.empty())
        {
            itemStr += "set_flags2|";
            for (auto& f : flagList)
                itemStr += f + "|";
            itemStr += "\n";
        }
    }

    if (pItem->fxFlags != 0)
    {
        std::vector<string> fxList;

        if (pItem->fxFlags & (1 << 0))
        {
            fxList.push_back(GetFxFlagRawStr(0));
            fxList.push_back(pItem->multiAnim1);
            fxList.push_back("MULTI_ANIM_END");
        }
        if (pItem->fxFlags & (1 << 5))
        {
            fxList.push_back(GetFxFlagRawStr(5));
            fxList.push_back(pItem->multiAnim2);
            fxList.push_back("MULTI_ANIM2_END");
        }
        if (pItem->fxFlags & (1 << 4))
        {
            fxList.push_back(GetFxFlagRawStr(4));
            fxList.push_back(ToString(pItem->dualAnimLayer.x) + "," + ToString(pItem->dualAnimLayer.y));
        }
        if (pItem->fxFlags & (1 << 2))
        {
            fxList.push_back(GetFxFlagRawStr(2));
            fxList.push_back(pItem->overlayTextureFile);
        }
        if (pItem->fxFlags & (1 << 20))
        {
            fxList.push_back(GetFxFlagRawStr(20));
            fxList.push_back(ToString(pItem->variantVersionItem));
        }

        uint32 specialMask = (1 << 0) | (1 << 5) | (1 << 4) | (1 << 2) | (1 << 20);

        for (int32 i = 0; i < 32; ++i)
        {
            if (pItem->fxFlags & (1 << i))
            {
                if ((1 << i) & specialMask)
                    continue;

                const char* fStr = GetFxFlagRawStr(i);
                if (fStr[0] != '\0')
                    fxList.push_back(fStr);
            }
        }

        if (!fxList.empty())
        {
            itemStr += "set_fx_flags|";
            for (auto& f : fxList)
                itemStr += f + "|";
            itemStr += "\n";
        }
    }

    if (!pItem->extraString.empty() || pItem->animMS != 200 || pItem->extraStringHash > 0)
    {
        itemStr += "set_extra|" + pItem->extraString + "|" + ToString(pItem->animMS) + "|" +
                   ToString(pItem->extraStringHash) + "|\n";
    }

    if (!pItem->configName.empty())
    {
        itemStr += "set_config_name|" + pItem->configName + "|\n";
    }

    if (pItem->maxCanHold != 200)
    {
        itemStr += "set_max_hold|" + ToString(pItem->maxCanHold) + "|\n";
    }

    if (pItem->rarity != 0)
    {
        itemStr += "set_rarity|" + ToString(pItem->rarity) + "|\n";
    }

    if (!pItem->customizedPunchParameters.empty())
    {
        itemStr += "set_custom_punch|" + pItem->customizedPunchParameters + "|\n";
    }

    return itemStr;
}