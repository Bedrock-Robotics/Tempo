// Copyright Tempo Simulation, LLC. All Rights Reserved

#pragma once

#include "TempoCoreUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"

// Finds the actor a client named. Accepts the name Tempo reports
// (UTempoCoreUtils::GetActorIdentifier) and the raw object name, so a Blueprint actor resolves
// whether or not the caller's name carries the "_C" suffix.
AActor* GetActorWithName(const UWorld* World, const FString& Name);

UObject* GetAssetByPath(const FString& AssetPath);

template <typename T = UActorComponent>
T* GetComponentWithName(const AActor* Actor, const FString& Name)
{
	TArray<T*> Components;
	Actor->GetComponents<T>(Components);
	for (T* Component : Components)
	{
		if (Component->GetName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Component;
		}
	}

	return nullptr;
}

// Finds the subclass of T a client named. Accepts the class name Tempo reports (the UClass name,
// Blueprint "_C" suffix and all) and, for Blueprint classes, the suffix-free spelling. Native
// classes are searched first, so an exact native match always wins.
// https://kantandev.com/articles/finding-all-classes-blueprints-with-a-given-base
template <typename T>
UClass* GetSubClassWithName(const FString& Name)
{
	// C++ classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;

		// Only interested in native C++ classes
		if (!Class->IsNative())
		{
			continue;
		}
		// Ignore deprecated
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		// Check this class is a subclass of ParentClass
		if (!Class->IsChildOf(T::StaticClass()))
		{
			continue;
		}
		if (UTempoCoreUtils::ClassNameMatches(Class, Name))
		{
			return Class;
		}
	}

	// Blueprint classes
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked< FAssetRegistryModule >(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	// The asset registry is populated asynchronously at startup, so there's no guarantee it has finished.
	// This simple approach just runs a synchronous scan on the entire content directory.
	// Better solutions would be to specify only the path to where the relevant blueprints are,
	// or to register a callback with the asset registry to be notified of when it's finished populating.
	// In cooked builds the registry is already fully populated, and scanning the root mount logs a warning.
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<FString> ContentPaths;
		ContentPaths.Add(TEXT("/"));
		AssetRegistry.ScanPathsSynchronous(ContentPaths);
	}

	FName BaseClassName = T::StaticClass()->GetFName();
	FName BaseClassPkgName = T::StaticClass()->GetPackage()->GetFName();
	FTopLevelAssetPath BaseClassPath(BaseClassPkgName, BaseClassName);

	// Use the asset registry to get the set of all class names deriving from Base
	TSet<FTopLevelAssetPath> DerivedNames;
	FTopLevelAssetPath Derived;
	{
		TArray< FTopLevelAssetPath > BasePaths;
		BasePaths.Add(BaseClassPath);

		TSet< FTopLevelAssetPath > Excluded;
		AssetRegistry.GetDerivedClassNames(BasePaths, Excluded, DerivedNames);
	}
	FARFilter Filter;
	FTopLevelAssetPath BPPath(UBlueprint::StaticClass()->GetPathName());
	Filter.ClassPaths.Add(BPPath);
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray< FAssetData > AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (auto const& Asset : AssetList)
	{
		// Get the the class this blueprint generates (this is stored as a full path)
		FAssetDataTagMapSharedView::FFindTagResult GeneratedClassPathPtr = Asset.TagsAndValues.FindTag("GeneratedClass");
		{
			if (GeneratedClassPathPtr.IsSet())
			{
				// Convert path to just the name part
				const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassPathPtr.GetValue());
				const FString ClassName = FPackageName::ObjectPathToObjectName(ClassObjectPath);
				const FTopLevelAssetPath ClassPath = FTopLevelAssetPath(ClassObjectPath);

				// Check if this class is in the derived set
				if (!DerivedNames.Contains(ClassPath))
				{
					continue;
				}
				// ClassName is a generated class name, so it always ends in "_C"; match it with
				// the suffix and without, so either spelling of the request resolves.
				if (UTempoCoreUtils::ClassNameMatches(ClassName, Name))
				{
					FString N = Asset.GetObjectPathString() + TEXT("_C");
					return LoadObject<UClass>(nullptr, *N);
				}
			}
		}
	}
	return nullptr;
}
