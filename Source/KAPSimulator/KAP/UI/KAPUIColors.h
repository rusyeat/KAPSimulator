// Интерфейстің ортақ түстері — бір жерде (әр .cpp-да қайта анықтаса, unity-құрастыруда атаулар қақтығысады)
#pragma once

#include "CoreMinimal.h"

namespace KAPUI
{
	inline const FLinearColor Amber(0.95f, 0.76f, 0.3f), Panel(0.07f, 0.06f, 0.045f, 0.8f), Muted(0.55f, 0.51f, 0.44f), Txtc(0.94f, 0.9f, 0.82f);
	inline const FLinearColor CGreen(0.37f, 0.83f, 0.54f), CRed(1.f, 0.35f, 0.29f), CYel(1.f, 0.82f, 0.29f), COrange(1.f, 0.54f, 0.23f), CBlue(0.44f, 0.71f, 1.f);
}
using namespace KAPUI;
