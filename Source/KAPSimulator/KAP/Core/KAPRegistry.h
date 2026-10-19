// Орталық тізілім — әр кадрда бүкіл әлемді аралаудың (TActorIterator) орнына жылдам тізімдер.
// Фриздарды азайтудың негізгі шарасы.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class AKAPNPC;
class AKAPStructure;

namespace KAPReg
{
	KAPSIMULATOR_API TArray<TWeakObjectPtr<AKAPNPC>>& NPCs();
	KAPSIMULATOR_API TArray<TWeakObjectPtr<AKAPStructure>>& Structures();
	KAPSIMULATOR_API TArray<TWeakObjectPtr<AActor>>& Interactables();

	/** TActorIterator<T> орнына: тек тізілімдегі нысандарды аралайды (тізім суреті — итерация кезінде жоюға болады) */
	template <class T> class TIter
	{
	public:
		TIter(UWorld*, TArray<TWeakObjectPtr<T>>& Src)
		{
			Items.Reserve(Src.Num());
			for (int32 i = Src.Num() - 1; i >= 0; i--) { if (T* X = Src[i].Get()) { if (IsValid(X)) Items.Add(X); } else Src.RemoveAtSwap(i); }
		}
		explicit operator bool() const { return Idx < Items.Num(); }
		TIter& operator++() { Idx++; while (Idx < Items.Num() && !IsValid(Items[Idx])) Idx++; return *this; }
		T* operator*() const { return Items[Idx]; }
		T* operator->() const { return Items[Idx]; }
	private:
		TArray<T*> Items;
		int32 Idx = 0;
	};

	template <class T> void Add(TArray<TWeakObjectPtr<T>>& A, T* X) { if (X) A.AddUnique(X); }
	template <class T> void Remove(TArray<TWeakObjectPtr<T>>& A, T* X) { A.RemoveAll([X](const TWeakObjectPtr<T>& P) { return !P.IsValid() || P.Get() == X; }); }
	/** Жойылған нысандарды тазалап, тірі көрсеткіштерді қайтарады */
	template <class T> void ForEach(TArray<TWeakObjectPtr<T>>& A, TFunctionRef<void(T*)> Fn)
	{
		for (int32 i = A.Num() - 1; i >= 0; i--) { if (T* X = A[i].Get()) Fn(X); else A.RemoveAtSwap(i); }
	}
}
