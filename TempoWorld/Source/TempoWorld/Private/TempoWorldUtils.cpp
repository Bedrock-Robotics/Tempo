// Copyright Tempo Simulation, LLC. All Rights Reserved

#include "TempoWorldUtils.h"

#include "TempoCoreUtils.h"

#include "EngineUtils.h"

AActor* GetActorWithName(const UWorld* World, const FString& Name)
{
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		// Matches the identifier the API hands out and the raw object name, so a client holding a
		// Blueprint actor's "_C"-bearing name ("BP_Foo_C_1" -- what cooked builds used to report,
		// and what the editor reports before a label exists) still resolves.
		if (UTempoCoreUtils::ActorNameMatches(*ActorIt, Name))
		{
			return *ActorIt;
		}
	}

	return nullptr;
}

UObject* GetAssetByPath(const FString& AssetPath)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString NormalizedPath = AssetPath;
	// Fully-qualified paths should be of the form PackageName.AssetName. If the asset name was not supplied
	// guess that it is the same as the last element of the package name.
	if (!NormalizedPath.Contains(TEXT(".")))
	{
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		NormalizedPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
	}

	const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NormalizedPath));
	if (AssetData.IsValid())
	{
		return AssetData.GetAsset();
	}

	return nullptr;
}
