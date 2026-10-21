// Ортақ визуал: материал даналары (Lumen-ге дайын PBR) және пішіндерден құрастыру
#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UFont;

// Cube/Cylinder — жиегі қырланған мештер (болса); CubeLow/CylLow — арзан негізгі пішіндер (HISM, сымдар)
enum class EKAPShape : uint8 { Cube, Cylinder, Sphere, Cone, Rock, Bush, Wolf, WormSeg, Plane, CubeLow, CylLow };

struct FKAPMat
{
	FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
	float Roughness = 0.7f;
	float Metallic = 0.f;
	FLinearColor Emissive = FLinearColor::Black;
	float Grime = 0.4f; // шаң/тат деңгейі (материалда шу арқылы)
	float Rust = 0.f;    // тат дақтары (0..1)
	float Ribs = 0.f;    // гофрленген металл (0..1)
	float RibPeriod = 12.f; // гофр қадамы, см
	float Panel = 0.f;   // панель жіктері (0..1)
	float PanelW = 120.f;
	float Stripes = 0.f; // сары-қара қауіп жолақтары
	float DustTop = 0.6f; // жоғарғы беттердегі шаң
	/** Қадам дыбысы үшін беткей: 0 — авто, 1 металл, 2 ағаш, 3 бетон */
	uint8 Surface = 0;
	float VarAmt = 0.1f, VarScale = 0.015f, RustScale = 0.006f; // түс дақтарының күші/масштабы, тат масштабы
	static FKAPMat Paint(FLinearColor C, float R = 0.55f) { FKAPMat M; M.Color = C; M.Roughness = R; return M; }
	static FKAPMat Metal(FLinearColor C, float R = 0.35f) { FKAPMat M; M.Color = C; M.Roughness = R; M.Metallic = 1.f; return M; }
	static FKAPMat Glow(FLinearColor C, float Strength) { FKAPMat M; M.Color = C; M.Emissive = C * Strength; M.Grime = 0; M.DustTop = 0; return M; }
	/** Гофрленген, боялған, тозған металл (вагон, ангар, қоршау) */
	static FKAPMat Sheet(FLinearColor C, float Rust = 0.35f, float Ribs = 1.f) { FKAPMat M = Paint(C, 0.6f); M.Metallic = 0.35f; M.Rust = Rust; M.Ribs = Ribs; M.Panel = 1.f; M.Grime = 0.75f; M.VarAmt = 0.16f; M.VarScale = 0.008f; return M; }
	/** Бетон/сылақ: дақтар мен төменгі лас, жіктер */
	static FKAPMat Concrete(FLinearColor C = FLinearColor(0.5f, 0.48f, 0.44f)) { FKAPMat M = Paint(C, 0.92f); M.Grime = 0.85f; M.Panel = 0.6f; M.PanelW = 300.f; M.VarAmt = 0.09f; M.VarScale = 0.004f; M.Surface = 3; return M; }
	/** Тот басқан болат (құбыр, тірек, бөшке) */
	static FKAPMat RustySteel(FLinearColor C = FLinearColor(0.35f, 0.34f, 0.32f), float Rust = 0.6f) { FKAPMat M = Metal(C, 0.5f); M.Rust = Rust; M.Grime = 0.6f; return M; }
	static FKAPMat Hazard() { FKAPMat M = Paint(FLinearColor(0.8f, 0.6f, 0.05f), 0.6f); M.Stripes = 1.f; M.Grime = 0.6f; M.Rust = 0.2f; return M; }
};

namespace KAPVis
{
	KAPSIMULATOR_API UStaticMesh* Mesh(EKAPShape S);
	KAPSIMULATOR_API UMaterialInterface* BaseMaterial();
	KAPSIMULATOR_API UMaterialInterface* TranslucentMaterial();
	KAPSIMULATOR_API UMaterialInstanceDynamic* MakeMID(UObject* Outer, const FKAPMat& M);
	KAPSIMULATOR_API void Apply(UMaterialInstanceDynamic* MID, const FKAPMat& M);
	/** Пішін қосу. Pos, Size — сантиметрмен (Size — толық өлшем). */
	KAPSIMULATOR_API UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, EKAPShape S, FVector Pos, FVector Size, const FKAPMat& M, FRotator Rot = FRotator::ZeroRotator, bool bCollision = true);
	/** Жер биіктігі (ландшафт бойынша сызық) — см */
	KAPSIMULATOR_API float GroundZ(const UWorld* World, float X, float Y, float Fallback = 0.f);
	/** Дайын мешті қосу (масштаб — тікелей көбейткіш), материал FKAPMat бойынша */
	KAPSIMULATOR_API UStaticMeshComponent* AddMesh(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, FVector Pos, FVector Scale, const FKAPMat& M, FRotator Rot = FRotator::ZeroRotator, bool bCollision = false);
	/** Кирилл әріптері бар белгі қаріпі (F_KAP_Sign) — TextRender-ге орнату */
	KAPSIMULATOR_API UFont* SignFont();
	KAPSIMULATOR_API void SignText(UTextRenderComponent* T);
}
