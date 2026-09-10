#pragma once

#include "../Precompiled.h"

uint8* zLibInflateToMemory(uint8* pCompData, uint32 compSize, uint32 decompSize);
uint8* zLibDefalteToMemory(uint8* pData, uint32 size, uint32& compSize);