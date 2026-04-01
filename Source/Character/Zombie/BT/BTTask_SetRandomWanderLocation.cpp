#include "BTTask_SetRandomWanderLocation.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
UBTTask_SetRandomWanderLocation::UBTTask_SetRandomWanderLocation()
{
	NodeName = TEXT("Set Random Wander Location");
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SetRandomWanderLocation, BlackboardKey));
}

EBTNodeResult::Type UBTTask_SetRandomWanderLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Pawn || !BlackboardComponent)
	{
		return EBTNodeResult::Failed;
	}
	const FVector Origin = Pawn->GetActorLocation();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	if (!NavSys) return EBTNodeResult::Failed;
	const int32 MaxTries = 8;
	FNavLocation OutLocation;
	bool bFound = false;

	for (int32 i = 0; i < MaxTries; ++i)
	{
		if (NavSys->GetRandomPointInNavigableRadius(Origin, WanderRadius, OutLocation))
		{
			const float Distance2D = FVector::Dist2D(Origin, OutLocation.Location);
			if (Distance2D >= MinDistanceFromOrigin)
			{
				bFound = true;
				break;
			}
		}
	}

	if (!bFound) return EBTNodeResult::Failed;
	BlackboardComponent->SetValueAsVector(BlackboardKey.SelectedKeyName, OutLocation.Location);
	return EBTNodeResult::Succeeded;
}