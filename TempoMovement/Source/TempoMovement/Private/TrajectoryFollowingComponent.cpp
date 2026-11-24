// Copyright Tempo Simulation, LLC. All Rights Reserved

#include "TrajectoryFollowingComponent.h"

#include "SplineActor.h"
#include "TrajectoryFollowingController.h"

#include "TempoMovement.h"

#include "GameFramework/Pawn.h"

UTrajectoryFollowingComponent::UTrajectoryFollowingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTrajectoryFollowingComponent::BeginPlay()
{
	Super::BeginPlay();

	StartFollowing();
}

void UTrajectoryFollowingComponent::ConfigureAndFollow(ASplineActor* InSpline, const FTrajectoryFollowingConfig& InConfig)
{
	Spline = InSpline;
	Config = InConfig;
	StartFollowing();
}

bool UTrajectoryFollowingComponent::SetSpeed(double SpeedCmS)
{
	if (!Controller)
	{
		return false;
	}
	// Keep the component's own Config in step with the controller's, so a later StartFollowing
	// (a reconfigure that leaves the speed model alone) does not resurrect the superseded speed.
	if (!Controller->SetSpeed(SpeedCmS))
	{
		return false;
	}
	Config.Speed = SpeedCmS;
	return true;
}

void UTrajectoryFollowingComponent::NotifyTrajectoryEnd(const FTrajectoryEndEvent& Event)
{
	OnTrajectoryEnd.Broadcast(Event);
}

void UTrajectoryFollowingComponent::StartFollowing()
{
	if (!Spline)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogTempoMovement, Warning, TEXT("UTrajectoryFollowingComponent on %s: owner is not a pawn; cannot follow spline."), *GetNameSafe(GetOwner()));
		return;
	}

	if (EnsureController())
	{
		Controller->FollowTrajectory(Spline, OwnerPawn, Config);
	}
}

ATrajectoryFollowingController* UTrajectoryFollowingComponent::EnsureController()
{
	if (IsValid(Controller))
	{
		return Controller;
	}

	// A controller drives a pawn, in a world that is in play.
	UWorld* World = GetWorld();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!World || !World->IsGameWorld() || !OwnerPawn)
	{
		return nullptr;
	}

	if (!ControllerClass)
	{
		ControllerClass = ATrajectoryFollowingController::StaticClass();
	}

	// A name that follows from the pawn's. A name the engine makes up differs from one process to
	// the next, so nothing that has to find this controller again in another process (a saved
	// simulation state, a log) could.
	const FString ControllerName = FString::Printf(TEXT("%s_TrajectoryFollowingController"), *OwnerPawn->GetName());
	if (ControllerName.Len() >= NAME_SIZE)
	{
		UE_LOG(LogTempoMovement, Error, TEXT("The name of pawn '%s' is too long to name its trajectory following controller after it"), *OwnerPawn->GetName());
		return nullptr;
	}

	// Don't set the pawn as the controller's owner: once the controller possesses the pawn,
	// PossessedBy() makes the pawn owned by the controller, which would form an owner loop.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = FName(*ControllerName);
	// That name and no other: with a name the engine altered to make it unique the controller would
	// be there, and not be found.
	SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;
	Controller = World->SpawnActor<ATrajectoryFollowingController>(ControllerClass, FTransform::Identity, SpawnParameters);
	if (!Controller)
	{
		UE_LOG(LogTempoMovement, Error, TEXT("Could not spawn trajectory following controller '%s'. Is the name taken?"), *ControllerName);
		return nullptr;
	}
#if WITH_EDITOR
	Controller->SetActorLabel(Controller->GetName(), /*bMarkDirty=*/false);
#endif
	return Controller;
}

void UTrajectoryFollowingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Controller)
	{
		Controller->UnPossess();
		Controller->Destroy();
		Controller = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
