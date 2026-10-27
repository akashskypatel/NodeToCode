// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Toolsets/N2CBlueprintParentMigrationToolset.h"

#include "BlueprintEditorLibrary.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"

FN2CBlueprintParentMigrationResult UN2CBlueprintParentMigrationToolset::ReparentBlueprintToNativeParent(
    const FString& BlueprintAssetPath,
    const FString& TargetNativeClassPath,
    const bool bExternalReferencesReviewed,
    const bool bApply,
    const FString& ExpectedPlanHash)
{
    FN2CBlueprintParentMigrationResult Result;
    Result.AssetObjectPath = BlueprintAssetPath.TrimStartAndEnd();
    Result.TargetParentClassPath = TargetNativeClassPath.TrimStartAndEnd();

    auto Fail = [&Result](const FString& Message)
    {
        Result.Error = Message;
        return Result;
    };

    if (!IsInGameThread())
    {
        return Fail(TEXT("Blueprint reparenting must run on the Unreal game thread."));
    }
    if (!bExternalReferencesReviewed)
    {
        return Fail(TEXT("Review references within the migration scope before reparenting."));
    }
    if (!bApply && !ExpectedPlanHash.IsEmpty())
    {
        return Fail(TEXT("ExpectedPlanHash is only valid when applying a reviewed dry-run plan."));
    }
    if (bApply && ExpectedPlanHash.IsEmpty())
    {
        return Fail(TEXT("Run a dry run first and supply its PlanHash before applying."));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Result.AssetObjectPath);
    UClass* TargetParentClass = LoadObject<UClass>(nullptr, *Result.TargetParentClassPath);
    if (!IsValid(Blueprint) || !IsValid(Blueprint->ParentClass) || !IsValid(TargetParentClass))
    {
        return Fail(TEXT("The Blueprint, current parent, and target parent class must load."));
    }

    UClass* SourceParentClass = Blueprint->ParentClass;
    Result.CurrentParentClassPath = SourceParentClass->GetPathName();
    if (SourceParentClass == TargetParentClass)
    {
        Result.bSuccess = true;
        Result.bPlanReady = true;
        Result.bAlreadyAtTarget = true;
        return Result;
    }
    if (!TargetParentClass->HasAnyClassFlags(CLASS_Native) || TargetParentClass->ClassGeneratedBy)
    {
        return Fail(TEXT("The target parent must be a native C++ class."));
    }
    if (!TargetParentClass->IsChildOf(SourceParentClass))
    {
        return Fail(TEXT("The target native class must derive from the Blueprint's current parent to preserve its inherited contract."));
    }

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (TargetParentClass->FindPropertyByName(Variable.VarName))
        {
            return Fail(FString::Printf(
                TEXT("Local variable '%s' would shadow a property on the target parent."),
                *Variable.VarName.ToString()));
        }
    }

    UPackage* Package = Blueprint->GetOutermost();
    if (!IsValid(Package))
    {
        return Fail(TEXT("The Blueprint package could not be resolved."));
    }
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    if (!FPaths::FileExists(PackageFilename))
    {
        return Fail(TEXT("The on-disk Blueprint package could not be found."));
    }
    if (Package->IsDirty())
    {
        return Fail(TEXT("The Blueprint package must be clean on disk before reparenting."));
    }

    const FMD5Hash PackageHash = FMD5Hash::HashFile(*PackageFilename);
    if (!PackageHash.IsValid())
    {
        return Fail(TEXT("The on-disk package could not be hashed for the dry-run plan."));
    }
    FString PackageHashText;
    PackageHashText.Reserve(PackageHash.GetSize() * 2);
    for (int32 Index = 0; Index < PackageHash.GetSize(); ++Index)
    {
        PackageHashText += FString::Printf(TEXT("%02x"), PackageHash.GetBytes()[Index]);
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    FString PlanText = FString::Printf(
        TEXT("%s|%s|%s|%s|graphs=%d|variables=%d|interfaces=%d"),
        *Result.AssetObjectPath,
        *Result.CurrentParentClassPath,
        *Result.TargetParentClassPath,
        *PackageHashText,
        Graphs.Num(), Blueprint->NewVariables.Num(), Blueprint->ImplementedInterfaces.Num());

    Result.Changes.Add(FString::Printf(
        TEXT("Reparent from %s to %s while preserving Blueprint graphs, variables, components, defaults, and interfaces."),
        *Result.CurrentParentClassPath, *Result.TargetParentClassPath));
    Result.PlanHash = FMD5::HashAnsiString(*PlanText);
    Result.bPlanReady = true;
    if (!bApply)
    {
        Result.bSuccess = true;
        Result.Changes.Add(FString::Printf(TEXT("Dry-run plan hash: %s"), *Result.PlanHash));
        return Result;
    }
    if (!ExpectedPlanHash.Equals(Result.PlanHash, ESearchCase::CaseSensitive))
    {
        return Fail(TEXT("The Blueprint package or reparent plan changed after dry run; inspect a fresh plan before applying."));
    }

    const FString BackupDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintMigrationBackups"));
    IFileManager::Get().MakeDirectory(*BackupDirectory, true);
    const FString BackupFilename = FPaths::Combine(
        BackupDirectory,
        FString::Printf(TEXT("%s_%s.uasset"), *Blueprint->GetName(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    if (IFileManager::Get().Copy(*BackupFilename, *PackageFilename) != COPY_OK)
    {
        return Fail(TEXT("The Blueprint package backup could not be created; no changes were applied."));
    }
    Result.BackupPath = BackupFilename;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const bool bPackageWasReadOnly = PlatformFile.IsReadOnly(*PackageFilename);
    if (bPackageWasReadOnly && !PlatformFile.SetReadOnly(*PackageFilename, false))
    {
        Result.Error = FString::Printf(
            TEXT("The Blueprint is read-only and could not be made writable. The backup is at '%s'."),
            *Result.BackupPath);
        return Result;
    }

    UBlueprintEditorLibrary::ReparentBlueprint(Blueprint, TargetParentClass);
    Result.bApplied = true;
    if (Blueprint->ParentClass != TargetParentClass ||
        (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings))
    {
        if (bPackageWasReadOnly)
        {
            PlatformFile.SetReadOnly(*PackageFilename, true);
        }
        Result.Error = FString::Printf(
            TEXT("Reparent or compile did not complete cleanly; the package backup is at '%s'."),
            *Result.BackupPath);
        return Result;
    }
    Result.bCompiled = true;

    Package->SetDirtyFlag(true);
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.bSlowTask = false;
    if (!UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs))
    {
        if (bPackageWasReadOnly)
        {
            PlatformFile.SetReadOnly(*PackageFilename, true);
        }
        Result.Error = FString::Printf(
            TEXT("Unreal could not save the reparented Blueprint. The backup is at '%s'."),
            *Result.BackupPath);
        return Result;
    }
    if (bPackageWasReadOnly)
    {
        PlatformFile.SetReadOnly(*PackageFilename, true);
    }
    Result.bSuccess = true;
    Result.bSaved = true;
    Result.Changes.Add(FString::Printf(TEXT("Compiled and saved the reparented Blueprint; backup: %s"), *Result.BackupPath));
    return Result;
}
