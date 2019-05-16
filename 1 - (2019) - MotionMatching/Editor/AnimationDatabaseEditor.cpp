// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimationDatabaseEditor.h"

#include "EditorViewportClient.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "SSingleObjectDetailsPanel.h"
#include "SEditorViewport.h"
#include "ScopedTransaction.h"
#include "MotionMatchingEditor.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SScrubControlPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Classes/AnimPreviewInstance.h"
#include "BaseToolkit.h"
#include "AssetEditorToolkit.h"
#include "AnimationDatabaseEditorCommands.h"
#include "Slate/SAnimationContextView.h"
#include "Commands.h"
#include "AnimationDatabase.h"
#include "Animation/Skeleton.h"


#define LOCTEXT_NAMESPACE "MotionFieldEditor"

const FName FAnimationDatabaseEditor::ToolkitFName(TEXT("AnimationDatabaseEditor"));
const FName FAnimationDatabaseEditor::PropertiesTabId(TEXT("AnimationDatabaseEditor_Properties"));
const FName FAnimationDatabaseEditor::AnimationContextTabId(TEXT("AnimationDatabaseEditor_AnimationContext"));
const FName FAnimationDatabaseEditor::AnimationDatabaseEditorAppIdentifier(TEXT("AnimationDatabaseEditorApp"));

FAnimationDatabaseEditor::~FAnimationDatabaseEditor()
{
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);

	DetailsView.Reset();
	PropertiesTab.Reset();
}

void FAnimationDatabaseEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AnimationDatabaseEditor", "AnimationDatabase Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FAnimationDatabaseEditor::SpawnPropertiesTab))
		.SetDisplayName(LOCTEXT("PropertiesTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(AnimationContextTabId, FOnSpawnTab::CreateSP(this, &FAnimationDatabaseEditor::SpawnAnimationContextTab))
		.SetDisplayName(LOCTEXT("AnimationContextTab", "Animations"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FAnimationDatabaseEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(AnimationContextTabId);
}

void FAnimationDatabaseEditor::InitAnimationDatabaseEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkistHost, UAnimationDatabase* InAnimationDatabase)
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;

	SetAnimationDatabase(InAnimationDatabase);
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	// Register and bind the toolbar commands
	FAnimationDatabaseEditorCommands::Register();
	BindCommands();

	const FDetailsViewArgs DetailViewArgs(bIsUpdatable, bIsLockable, true, FDetailsViewArgs::ObjectsUseNameArea, false);
	DetailsView = PropertyEditorModule.CreateDetailView(DetailViewArgs);

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_AnimationDatabaseEditor_Layout_v1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell(true)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Vertical)
			->SetSizeCoefficient(0.2f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(PropertiesTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab(AnimationContextTabId, ETabState::OpenedTab)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	FAssetEditorToolkit::InitAssetEditor(
		InMode, 
		InToolkistHost, 
		FAnimationDatabaseEditor::AnimationDatabaseEditorAppIdentifier, 
		StandaloneDefaultLayout, 
		bCreateDefaultStandaloneMenu, 
		bCreateDefaultToolbar, 
		InAnimationDatabase);

	// Set the animation database to edit in details view
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InAnimationDatabase);
	}

	PopulateToolbar();
	RegenerateMenusAndToolbars();
}

FName FAnimationDatabaseEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FAnimationDatabaseEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Animation Database Editor");
}

FString FAnimationDatabaseEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimationDatabase ").ToString();
}

FLinearColor FAnimationDatabaseEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.5f, 0.0f, 0.0f, 0.5f);
}

void FAnimationDatabaseEditor::OnProcessAllClicked()
{
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("OnProcessAll", "On Process All still needs to be implemented"));
}

void FAnimationDatabaseEditor::OnClearAllClicked()
{
	GetAnimationDatabase()->ClearAllFrameData();
	AnimationContextView->RepopulateAnimationView();
}

void FAnimationDatabaseEditor::AddSourceAnimations(TArray<UAnimSequence*> InAnimations)
{
	GetAnimationDatabase()->AddSourceAnimations(InAnimations);
	AnimationContextView->RepopulateAnimationView();
}

void FAnimationDatabaseEditor::RemoveSourceAnimationAtIndex(const int InAnimationIndex)
{
	GetAnimationDatabase()->RemoveSourceAnimationAtIndex(InAnimationIndex);
	AnimationContextView->RepopulateAnimationView();
}

void FAnimationDatabaseEditor::SetAnimationDatabase(UAnimationDatabase* InAnimationDatabase)
{
	AnimationDatabase = InAnimationDatabase;
}

//////////////////////////////////////////////////////////////////////////
// Begin Animation Database Getters
//////////////////////////////////////////////////////////////////////////

UAnimationDatabase* FAnimationDatabaseEditor::GetAnimationDatabase() const
{
	return AnimationDatabase;
}

FString FAnimationDatabaseEditor::GetSkeletonName() const
{
	return GetAnimationDatabase()->GetSkeleton()->GetName();
}

TArray<UAnimSequence*> FAnimationDatabaseEditor::GetSourceAnimations() const
{
	return GetAnimationDatabase()->GetSourceAnimations();
}

TArray<FName> FAnimationDatabaseEditor::GetMotionMatchingBones() const
{
	return GetAnimationDatabase()->GetMotionMatchingBones();
}

//////////////////////////////////////////////////////////////////////////
// Begin Animation Database Getters
//////////////////////////////////////////////////////////////////////////

void FAnimationDatabaseEditor::BindCommands()
{
	const FAnimationDatabaseEditorCommands& Commands = FAnimationDatabaseEditorCommands::Get();
	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();

	UICommandList->MapAction(Commands.ProcessAll, FExecuteAction::CreateSP(this, &FAnimationDatabaseEditor::OnProcessAllClicked));
	UICommandList->MapAction(Commands.ClearAll, FExecuteAction::CreateSP(this, &FAnimationDatabaseEditor::OnClearAllClicked));
}

TSharedRef<SDockTab> FAnimationDatabaseEditor::SpawnPropertiesTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PropertiesTabId);

	// Hack - still need to figure out how to create a custom tab
	TSharedPtr<FAnimationDatabaseEditor> AnimationDatabaseEditorPtr = SharedThis(this);
	AnimationContextView = SNew(SAnimationContextView, AnimationDatabaseEditorPtr);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("GenericDetailsTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			// Hack - still need to figure out how to create a custom tab
			AnimationContextView.ToSharedRef()
			//DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FAnimationDatabaseEditor::SpawnAnimationContextTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AnimationContextTabId);

	TSharedPtr<FAnimationDatabaseEditor> AnimationDatabaseEditorPtr = SharedThis(this);
	AnimationContextView = SNew(SAnimationContextView, AnimationDatabaseEditorPtr);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("GenericAnimationContextTitle", "Animation Context"))
		.TabColorScale(GetTabColorScale())
		[
			AnimationContextView.ToSharedRef()
		];
}

void FAnimationDatabaseEditor::HandleAssetPostImport(class UFactory* InFactory, UObject* InObject)
{
	//if (EditingObjects.Contains(InObject))
	//{
	//	// The details panel likely needs to be refreshed if an asset was imported again
	//	DetailsView->SetObjects(EditingObjects);
	//}
}

void FAnimationDatabaseEditor::PopulateToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Command");
			{
				// Add additional functionality to the toolbar
				ToolbarBuilder.AddToolBarButton(FAnimationDatabaseEditorCommands::Get().ProcessAll);
				ToolbarBuilder.AddToolBarButton(FAnimationDatabaseEditorCommands::Get().ClearAll);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension("Asset", EExtensionHook::After, GetToolkitCommands(), FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar));

	AddToolbarExtender(ToolbarExtender);

	FMotionMatchingEditorModule* MotionMatchingEditorModule = &FModuleManager::LoadModuleChecked<FMotionMatchingEditorModule>("MotionMatchingEditor");
	AddToolbarExtender(MotionMatchingEditorModule->GetAnimationDatabaseEditorToolBarExtensibility()->GetAllExtenders());
}

#undef LOCTEXT_NAMESPACE


