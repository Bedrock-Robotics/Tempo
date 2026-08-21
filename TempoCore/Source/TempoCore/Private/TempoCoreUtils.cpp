// Copyright Tempo Simulation, LLC. All Rights Reserved

#include "TempoCoreUtils.h"

#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 4
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

UWorldSubsystem* UTempoCoreUtils::GetSubsystemImplementingInterface(const UObject* WorldContextObject, TSubclassOf<UInterface> Interface)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UWorldSubsystem* SubsystemImplementingInterface = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
		TArray<UWorldSubsystem*> Subsystems = World->GetSubsystemArray<UWorldSubsystem>();
		for (UWorldSubsystem* Subsystem : Subsystems)
		{
			if (Subsystem->GetClass()->ImplementsInterface(Interface))
			{
				SubsystemImplementingInterface = Subsystem;
			}
		}
#else
		World->ForEachSubsystem<UWorldSubsystem>([&Interface, &SubsystemImplementingInterface](UWorldSubsystem* Subsystem)
		{
			if (Subsystem->GetClass()->ImplementsInterface(Interface))
			{
				SubsystemImplementingInterface = Subsystem;
			}
		});
#endif
		return SubsystemImplementingInterface;
	}

	return nullptr;
}

bool UTempoCoreUtils::IsGameWorld(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject->GetWorld();
	return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

FBox UTempoCoreUtils::GetActorLocalBounds(const AActor* Actor, bool bIncludeHiddenComponents)
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	FBox LocalBounds;

	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent->IsVisible() && !bIncludeHiddenComponents)
		{
			continue;
		}

		auto AddAggGeomToBounds = [Actor, &LocalBounds](const FKAggregateGeom& AggGeom, const FTransform& WorldTransform)
		{
			FBoxSphereBounds Bounds;
			const FTransform RelativeTransform = WorldTransform.GetRelativeTransform(Actor->GetTransform());
			AggGeom.CalcBoxSphereBounds(Bounds, RelativeTransform);
			LocalBounds += Bounds.GetBox();
		};

		if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimitiveComponent))
		{
			if (const UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent->GetPhysicsAsset())
			{
				for (const USkeletalBodySetup* SkeletalBodySetup : PhysicsAsset->SkeletalBodySetups)
				{
					AddAggGeomToBounds(SkeletalBodySetup->AggGeom, SkeletalMeshComponent->GetBoneTransform(SkeletalBodySetup->BoneName));
				}
			}
		}
		else if(const UBodySetup* BodySetup = PrimitiveComponent->BodyInstance.GetBodySetup())
		{
			AddAggGeomToBounds(BodySetup->AggGeom, PrimitiveComponent->GetComponentTransform());
		}
	}

	return LocalBounds;
}

FString UTempoCoreUtils::GetActorIdentifier(const AActor* Actor)
{
	if (!Actor)
	{
		return FString();
	}

#if WITH_EDITOR
	// Materialize the actor label now so it matches every later GetActorNameOrLabel() call,
	// including GetActorWithName lookups. See header for details.
	(void)Actor->GetActorLabel();
#endif
	const FString NameOrLabel = Actor->GetActorNameOrLabel();

	// Cooked builds have no labels, so the above is the object name, which for a Blueprint actor
	// embeds the generated class's "_C" ("BP_Foo_C_1"). Editor labels never do (Unreal's own
	// AActor::GetDefaultActorLabel strips it), so strip it here too and clients see one spelling
	// in both build types. The suffix is interior rather than trailing, so splice it out by class
	// name instead of chopping the end.
	const UClass* Class = Actor->GetClass();
	const FString ClassName = Class->GetName();
	const FString StrippedClassName = GetClassNameWithoutBlueprintSuffix(Class);
	if (StrippedClassName.Len() != ClassName.Len() && NameOrLabel.StartsWith(ClassName, ESearchCase::CaseSensitive))
	{
		return StrippedClassName + NameOrLabel.RightChop(ClassName.Len());
	}

	return NameOrLabel;
}

bool UTempoCoreUtils::ActorNameMatches(const AActor* Actor, const FString& RequestedName)
{
	return Actor
		&& (GetActorIdentifier(Actor).Equals(RequestedName, ESearchCase::IgnoreCase)
			|| Actor->GetName().Equals(RequestedName, ESearchCase::IgnoreCase));
}

FString UTempoCoreUtils::GetClassNameWithoutBlueprintSuffix(const UClass* Class)
{
	if (!Class)
	{
		return FString();
	}

	// Only Blueprint-generated classes carry the suffix. CLASS_CompiledFromBlueprint survives
	// cooking, unlike UClass::ClassGeneratedBy (the UBlueprint asset itself is editor-only).
	const FString ClassName = Class->GetName();
	return Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint) ? StripBlueprintClassSuffix(ClassName) : ClassName;
}

FString UTempoCoreUtils::StripBlueprintClassSuffix(const FString& ClassName)
{
	// Case-insensitive so a client's spelling of the suffix does not matter. Unreal always
	// generates it as "_C", so this can only over-strip a native class name ending in "_c" -- and
	// GetClassNameWithoutBlueprintSuffix never routes native classes here.
	FString Stripped = ClassName;
	Stripped.RemoveFromEnd(TEXT("_C"), ESearchCase::IgnoreCase);
	return Stripped;
}

bool UTempoCoreUtils::ClassNameMatches(const UClass* Class, const FString& RequestedName)
{
	return Class
		&& (Class->GetName().Equals(RequestedName, ESearchCase::IgnoreCase)
			|| GetClassNameWithoutBlueprintSuffix(Class).Equals(RequestedName, ESearchCase::IgnoreCase));
}

bool UTempoCoreUtils::ClassNameMatches(const FString& ClassName, const FString& RequestedName)
{
	return ClassName.Equals(RequestedName, ESearchCase::IgnoreCase)
		|| StripBlueprintClassSuffix(ClassName).Equals(RequestedName, ESearchCase::IgnoreCase);
}
