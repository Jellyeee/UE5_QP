#include "InventoryRootWidget.h"
#include "InventoryGridWidget.h"
#include "PJ_Quiet_Protocol/Character/QPCharacter.h"
#include "PJ_Quiet_Protocol/Inventory/InventoryComponent.h"
#include "InventoryDragOperation.h" 

void UInventoryRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	AQPCharacter* Character = Cast<AQPCharacter>(GetOwningPlayerPawn());
	if (!Character || !InventoryGrid) return;

	CachedInventory = Character->GetInventoryComponent();
	if (!CachedInventory) return;

	CachedInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryRootWidget::HandleInventoryChanged);
	CachedInventory->OnInventoryChanged.AddDynamic(this, &UInventoryRootWidget::HandleInventoryChanged);

	InventoryGrid->SetInventory(CachedInventory);
	InventoryGrid->RefreshGrid();
}

void UInventoryRootWidget::NativeDestruct()
{
	if (CachedInventory)
	{
		CachedInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryRootWidget::HandleInventoryChanged);
		CachedInventory = nullptr;
	}
	Super::NativeDestruct();
}

bool UInventoryRootWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!InOperation) return false;
	if (!InventoryGrid) return false;

	UInventoryDragOperation* DragOp = Cast<UInventoryDragOperation>(InOperation);
	if (!DragOp) return false;

	const FVector2D ScreenPos = InDragDropEvent.GetScreenSpacePosition();
	const FGeometry GridGeo = InventoryGrid->GetCachedGeometry();
	const bool bOverGrid = GridGeo.IsUnderLocation(ScreenPos);

	if (bOverGrid)
	{
		return InventoryGrid->HandleDropFromScreenPos(InOperation, ScreenPos);
	}

	if (DragOp->SourceInventory) {
		AQPCharacter* Character = Cast<AQPCharacter>(GetOwningPlayerPawn());
		if (!Character) return false;
		Character->DropInventoryItemAt(DragOp->FromCell);
		return true;
	}
	return false;
}

void UInventoryRootWidget::HandleInventoryChanged()
{
	if (InventoryGrid) InventoryGrid->RefreshGrid();
}
