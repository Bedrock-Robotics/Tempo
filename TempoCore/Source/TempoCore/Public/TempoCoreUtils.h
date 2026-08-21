// Copyright Tempo Simulation, LLC. All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "TempoCoreUtils.generated.h"

UCLASS()
class TEMPOCORE_API UTempoCoreUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template <typename TEnum>
	static FString GetEnumValueAsString(const TEnum Value, bool bQualified=false)
	{
		FString ValueString = UEnum::GetValueAsString(Value);
		if (!bQualified)
		{
			if (int32 LastColonIdx; ValueString.FindLastChar(':', LastColonIdx))
			{
				ValueString.RightChopInline(LastColonIdx + 1);
			}
		}
		return ValueString;
	}

	UFUNCTION(BlueprintCallable, Category="TempoCoreUtils",  meta=(WorldContext="WorldContextObject", DeterminesOutputType="Interface"))
	static UWorldSubsystem* GetSubsystemImplementingInterface(const UObject* WorldContextObject, TSubclassOf<UInterface> Interface);

	// Is the world owning this object a PIE or Game world?
	// Note that UWorld::GetWorld() considers GamePreview and GameRPC worlds to be Game worlds, which we do not.
	UFUNCTION(BlueprintCallable, Category="TempoCoreUtils",  meta=(WorldContext="WorldContextObject"))
	static bool IsGameWorld(const UObject* WorldContextObject);

	// Calculates a tight bounding box of all the Actor's components,
	// axis-aligned with the Actor's local coordinates.
	UFUNCTION(BlueprintCallable, Category="TempoCoreUtils")
	static FBox GetActorLocalBounds(const AActor* Actor, bool bIncludeHiddenComponents);

	// --- Names over RPC ------------------------------------------------------------------------
	//
	// Unreal names the UClass a Blueprint generates with a "_C" suffix ("BP_Foo_C"), and that
	// suffix leaks into instance names too ("BP_Foo_C_1"). The rules for every Tempo API are:
	//
	//   * CLASS names are reported verbatim, suffix and all -- UClass::GetName() is the truth, and
	//     reporting it keeps a native "Foo" distinguishable from a Blueprint "Foo_C".
	//   * ACTOR and COMPONENT names are reported without the suffix, because they cannot carry it
	//     consistently: an editor actor's name is its label, which Unreal itself de-"_C"s (and
	//     which a user may rename to anything). Cooked builds are made to match.
	//   * EVERYTHING is accepted in either spelling, so a reported name feeds straight back in.
	//
	// The helpers below are how that is enforced. Report actor names via GetActorIdentifier and
	// class names via UClass::GetName(); match incoming names with ActorNameMatches (or
	// GetActorWithName) and ClassNameMatches (or GetSubClassWithName).

	// Returns a stable, round-trippable name for an actor, suitable for handing to an external
	// client and using later to look the same actor back up (e.g. via GetActorWithName).
	//
	// AActor::GetActorNameOrLabel() is not stable over an actor's lifetime in editor builds: it
	// returns the FName (which keeps the Blueprint "_C" suffix, e.g. "BP_Foo_C_0") until an actor
	// label is lazily created, after which it returns the de-"_C"'d label ("BP_Foo_0"). A name
	// returned to a client before its label exists therefore fails a later lookup. Materializing
	// the label here pins the value so every subsequent GetActorNameOrLabel() call agrees. This is
	// a no-op in cooked builds, which have no labels (GetActorNameOrLabel() is always GetName()) --
	// so those builds would otherwise report the raw, "_C"-bearing object name ("BP_Foo_C_1").
	// This strips the suffix there too, so a Blueprint actor is "BP_Foo_1" in every build type.
	// (Class names keep their suffix; only instance names are normalized. See the banner above.)
	static FString GetActorIdentifier(const AActor* Actor);

	// Does the client-supplied RequestedName name this actor? True for the identifier clients are
	// given (GetActorIdentifier) and for the raw object name, so a Blueprint actor resolves whether
	// or not the caller's name carries the "_C" suffix. Case-insensitive.
	static bool ActorNameMatches(const AActor* Actor, const FString& RequestedName);

	// A class's name with the Blueprint "_C" suffix removed ("BP_Foo_C" -> "BP_Foo"). Native class
	// names are returned unchanged, so a native class that really does end in "_C" keeps it.
	//
	// This is NOT the spelling class names are reported in -- that is UClass::GetName(). It is the
	// canonical form two spellings of the same class reduce to, for matching a client's request
	// (ClassNameMatches) and for keying a map a client can address by either spelling.
	static FString GetClassNameWithoutBlueprintSuffix(const UClass* Class);

	// Removes a trailing Blueprint "_C" from an already-stringified class name; names without it
	// are returned unchanged. For names that arrive as strings with no UClass to consult -- from
	// the asset registry, or from a client. Prefer the UClass overload above when one is in hand,
	// since it can tell a Blueprint suffix from a native name that merely ends the same way.
	static FString StripBlueprintClassSuffix(const FString& ClassName);

	// Does the client-supplied RequestedName name this class? True for both the reported UClass
	// name ("BP_Foo_C") and the suffix-free spelling ("BP_Foo"), case-insensitively. Only the class
	// side is stripped, never the request, so a native "Foo" and a Blueprint "Foo_C" stay
	// distinguishable: "Foo" matches only the former, "Foo_C" only the latter.
	static bool ClassNameMatches(const UClass* Class, const FString& RequestedName);

	// As above, for a class known only by name -- e.g. a generated-class name from the asset
	// registry, where the class may not be loaded yet. Such names always carry the suffix, so
	// stripping unconditionally is safe here.
	static bool ClassNameMatches(const FString& ClassName, const FString& RequestedName);

	template <typename BaseClass>
	static bool IsMostDerivedSubclass(UClass* Class)
	{
		// RF_NoFlags to include CDO
		for (TObjectIterator<BaseClass> DerivedClass(EObjectFlags::RF_NoFlags); DerivedClass; ++DerivedClass)
		{
			if (DerivedClass->GetClass() != Class && DerivedClass->IsA(Class))
			{
				// There is a more derived version of Class
				return false;
			}
		}

		return true;
	}

	// Wraps all BP calls in FEditorScriptExecutionGuard when in the Editor, which prevents early termination (and lurking
	// bugs due to calls silently being cancelled and returning default values) due to erroneous runaway loop detection.
	template <typename ObjectType, typename FuncType, typename... ArgTypes>
	static auto CallBlueprintFunction(ObjectType* Object, FuncType Function, ArgTypes&&... Args)
	{
		if (Object->GetWorld()->WorldType != EWorldType::Editor)
		{
			return Function(Object, Args...);
		}

		using RetValType = decltype(Function(Object, std::forward<ArgTypes>(Args)...)); // Deduce return type
		if constexpr (std::is_void_v<RetValType>)
		{
			if (!ensureMsgf(IsValid(Object), TEXT("Tried to call Blueprint function on invalid object")))
			{
				return;
			}
			{
				FEditorScriptExecutionGuard ScriptExecutionGuard;
				Function(Object, std::forward<ArgTypes>(Args)...);
			}
		}
		else
		{
			if (!ensureMsgf(IsValid(Object), TEXT("Tried to call Blueprint function on invalid object")))
			{
				return RetValType();
			}
			RetValType RetVal;
			{
				FEditorScriptExecutionGuard ScriptExecutionGuard;
				RetVal = Function(Object, std::forward<ArgTypes>(Args)...);
			}
			return RetVal;
		}
	}
};
