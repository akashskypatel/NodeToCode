// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "N2CBlueprintParentMigrationToolset.generated.h"

USTRUCT(BlueprintType)
struct FN2CBlueprintParentMigrationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bPlanReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bCompiled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bSaved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	bool bAlreadyAtTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString AssetObjectPath;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString CurrentParentClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString TargetParentClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString PlanHash;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString BackupPath;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	TArray<FString> Changes;

	UPROPERTY(BlueprintReadOnly, Category = "Node to Code")
	FString Error;
};

/** Exposes guarded Blueprint parent migrations to native C++ classes. */
UCLASS(BlueprintType, Hidden)
class UN2CBlueprintParentMigrationToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Dry-runs and optionally applies a Blueprint reparent to a native class that derives from its current parent.
	 * The plan is tied to the on-disk package hash; applying requires the reviewed dry-run hash. A package backup is
	 * created before reparent, compile, and save. The Blueprint's graphs, local variables, widget/SCS trees, and
	 * explicitly implemented interfaces remain owned by the Blueprint.
	 */
	UFUNCTION(Category = "Node to Code", meta = (AICallable))
	static FN2CBlueprintParentMigrationResult ReparentBlueprintToNativeParent(
		const FString& BlueprintAssetPath,
		const FString& TargetNativeClassPath,
		bool bExternalReferencesReviewed,
		bool bApply,
		const FString& ExpectedPlanHash);
};
