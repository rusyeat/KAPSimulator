#include "KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/World/KAPStructure.h"

namespace KAPReg
{
	TArray<TWeakObjectPtr<AKAPNPC>>& NPCs() { static TArray<TWeakObjectPtr<AKAPNPC>> A; return A; }
	TArray<TWeakObjectPtr<AKAPStructure>>& Structures() { static TArray<TWeakObjectPtr<AKAPStructure>> A; return A; }
	TArray<TWeakObjectPtr<AActor>>& Interactables() { static TArray<TWeakObjectPtr<AActor>> A; return A; }
}
