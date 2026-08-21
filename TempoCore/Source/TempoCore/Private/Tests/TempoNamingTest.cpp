// Copyright Tempo Simulation, LLC. All Rights Reserved

#include "TempoCoreUtils.h"

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"

// Unit tests for the class-naming helpers behind Tempo's RPC naming rules: class names are
// reported as their UClass name, "_C" suffix and all; actor names never carry it; and everything is
// accepted in either spelling. See UTempoCoreUtils and the banner in TempoWorld/WorldControl.proto.
//
// These are engine-object-light (no world, no RHI) and run via:
//   Scripts/Test.sh            (runs all "Tempo." automation tests)
//   Automation RunTests Tempo.Core.Naming   (from the editor console)
//
// A Blueprint-generated UClass cannot be conjured in a unit test, so the UClass-typed helpers are
// exercised against native classes (which must come back unchanged) and the "_C" behavior is
// covered through the name-typed overloads, which is where the suffix logic actually lives.

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TempoNamingTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempoNamingStripSuffixTest,
	"Tempo.Core.Naming.StripBlueprintClassSuffix", TempoNamingTestFlags)
bool FTempoNamingStripSuffixTest::RunTest(const FString& Parameters)
{
	auto Strips = [this](const TCHAR* In, const TCHAR* Expected)
	{
		const FString Actual = UTempoCoreUtils::StripBlueprintClassSuffix(In);
		if (!Actual.Equals(Expected, ESearchCase::CaseSensitive))
		{
			AddError(FString::Printf(TEXT("Strip('%s'): expected '%s', got '%s'"), In, Expected, *Actual));
		}
	};

	Strips(TEXT("BP_Foo_C"), TEXT("BP_Foo"));
	// Already stripped, or never suffixed: unchanged.
	Strips(TEXT("BP_Foo"), TEXT("BP_Foo"));
	Strips(TEXT("StaticMeshActor"), TEXT("StaticMeshActor"));
	Strips(TEXT(""), TEXT(""));
	// Only a trailing suffix is removed, and only one of them.
	Strips(TEXT("BP_Foo_C_1"), TEXT("BP_Foo_C_1"));
	Strips(TEXT("BP_C_Foo"), TEXT("BP_C_Foo"));
	Strips(TEXT("BP_Foo_C_C"), TEXT("BP_Foo_C"));
	// A client's casing of the suffix does not matter.
	Strips(TEXT("BP_Foo_c"), TEXT("BP_Foo"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempoNamingClassNameWithoutSuffixTest,
	"Tempo.Core.Naming.ClassNameWithoutBlueprintSuffix", TempoNamingTestFlags)
bool FTempoNamingClassNameWithoutSuffixTest::RunTest(const FString& Parameters)
{
	// A native class name is left alone (minus Unreal's A/U prefix, which GetName() already drops)
	// -- the suffix logic must not touch it, even to canonicalize for matching.
	TestEqual(TEXT("native actor class"),
		UTempoCoreUtils::GetClassNameWithoutBlueprintSuffix(AActor::StaticClass()), FString(TEXT("Actor")));
	TestEqual(TEXT("native component class"),
		UTempoCoreUtils::GetClassNameWithoutBlueprintSuffix(UStaticMeshComponent::StaticClass()), FString(TEXT("StaticMeshComponent")));
	TestEqual(TEXT("null class"), UTempoCoreUtils::GetClassNameWithoutBlueprintSuffix(nullptr), FString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTempoNamingClassNameMatchesTest,
	"Tempo.Core.Naming.ClassNameMatches", TempoNamingTestFlags)
bool FTempoNamingClassNameMatchesTest::RunTest(const FString& Parameters)
{
	auto Matches = [this](const TCHAR* ClassName, const TCHAR* Requested, bool bExpected)
	{
		const bool bActual = UTempoCoreUtils::ClassNameMatches(FString(ClassName), FString(Requested));
		if (bActual != bExpected)
		{
			AddError(FString::Printf(TEXT("Matches('%s', '%s'): expected %s"),
				ClassName, Requested, bExpected ? TEXT("a match") : TEXT("no match")));
		}
	};

	// A Blueprint class is found by either spelling of its name -- "BP_Foo_C" is what the API
	// reports, and "BP_Foo" is what a client is just as likely to write.
	Matches(TEXT("BP_Foo_C"), TEXT("BP_Foo"), true);
	Matches(TEXT("BP_Foo_C"), TEXT("BP_Foo_C"), true);
	// Case-insensitively, in both spellings.
	Matches(TEXT("BP_Foo_C"), TEXT("bp_foo"), true);
	Matches(TEXT("BP_Foo_C"), TEXT("bp_foo_c"), true);
	// A native class is found by its own name...
	Matches(TEXT("StaticMeshActor"), TEXT("StaticMeshActor"), true);
	Matches(TEXT("StaticMeshActor"), TEXT("staticmeshactor"), true);
	// ...but adding a suffix it does not have is not a match. Only the class side is ever
	// stripped, never the request, so a native "Foo" and a Blueprint "Foo_C" stay distinguishable.
	Matches(TEXT("Foo"), TEXT("Foo_C"), false);
	Matches(TEXT("Foo_C"), TEXT("Foo"), true);
	// Unrelated names never match, suffix or not.
	Matches(TEXT("BP_Foo_C"), TEXT("BP_Bar"), false);
	Matches(TEXT("BP_FooBar_C"), TEXT("BP_Foo"), false);
	Matches(TEXT("BP_Foo_C"), TEXT(""), false);

	// The UClass overload agrees for native classes, and rejects the suffixed spelling of one.
	TestTrue(TEXT("native class by name"), UTempoCoreUtils::ClassNameMatches(AActor::StaticClass(), TEXT("Actor")));
	TestTrue(TEXT("native class, other case"), UTempoCoreUtils::ClassNameMatches(AActor::StaticClass(), TEXT("actor")));
	TestFalse(TEXT("native class with a suffix it lacks"),
		UTempoCoreUtils::ClassNameMatches(AActor::StaticClass(), TEXT("Actor_C")));
	TestFalse(TEXT("wrong native class"), UTempoCoreUtils::ClassNameMatches(AActor::StaticClass(), TEXT("Pawn")));
	TestFalse(TEXT("null class"), UTempoCoreUtils::ClassNameMatches(nullptr, TEXT("Actor")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
