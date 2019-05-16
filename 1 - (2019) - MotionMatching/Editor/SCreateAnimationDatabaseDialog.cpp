// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SCreateAnimationDatabaseDialog.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "AnimationDatabaseFactory.h"
#include "AssetData.h"
#include "EditorStyleSet.h"
#include "Internationalization.h"

#include "Animation/Skeleton.h"
#include "Slate/SBonePickerItem.h"
#include "Misc/MessageDialog.h"


#define LOCTEXT_NAMESPACE "CreateAnimationDatabaseDialog"


void SCreateAnimationDatabaseDialog::Construct(const FArguments& InArgs, const bool Ow)
{
	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.WidthOverride(500.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					// Create the User Interface for the Skeleton Picker Container
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Content()
					[
						SAssignNew(SkeletonContainer, SVerticalBox)
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Content()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(0.4f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(0.0f, 3.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MotionBoneDescription", "Pick Bones for Motion Matching:" LINE_TERMINATOR
									"Please make sure you only select bones such as Legs, Feet, Hands, Arms, Spines and Head." LINE_TERMINATOR
									"This will allow for the most accurate calculations for Motion Matching." //LINE_TERMINATOR LINE_TERMINATOR
									//"I Recommend only selecting the following bones:" LINE_TERMINATOR 
									//"Pelvis, Spine_01, Spine_02, Spine_03, Neck_01, Head, Clavicle_l, " LINE_TERMINATOR  
									//"Upperarm_l, Lowerarm_l, Hand_l, Clavicle_r, Upperarm_r," LINE_TERMINATOR 
									//"Lowerarm_r, Hand_r,  Thigh_l, Calf_l, Foot_l, Ball_l, Thigh_r," LINE_TERMINATOR 
									//"Calf_r, Foot_r, Ball_r."
								))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.1f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("BoneName", "Your Skeleton Bones"))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("IsMotionBone", "Use Bone for Motion Matching"))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(0.9f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(SkeletonBoneContainer, SVerticalBox)
							]
						]
					]
				]
				// Begin Confirm and Cancel Buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(8.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SCreateAnimationDatabaseDialog::OnConfirmClicked)
						.Text(LOCTEXT("CreateAnimationDatabaseConfirm", "Confirm"))
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SCreateAnimationDatabaseDialog::OnCancelClicked)
						.Text(LOCTEXT("CreateAnimationDatabaseCancel", "Cancel"))
					]
				]
				// End Confirm and Cancel Buttons
			]
		]
	];

	CreateSkeletonPicker();
	CreateBonePicker();
}

bool SCreateAnimationDatabaseDialog::ConfigureProperties(TWeakObjectPtr<UAnimationDatabaseFactory> InAnimationDatabaseFactory)
{
	AnimationDatabaseFactory = InAnimationDatabaseFactory;

	TSharedRef<SWindow> Window = SNew(SWindow)
	.Title(LOCTEXT("CreateAnimationDatabaseOptions", "Create Animation Database"))
	.ClientSize(FVector2D(500.0f, 700.0f))
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	[
		AsShared()
	];

	// Cache the current Animation Database Modal Window
	SkeletonSelectionModalWindow = Window;

	GEditor->EditorAddModalWindow(Window);
	AnimationDatabaseFactory.Reset();

	return bConfirmClicked;
}

void SCreateAnimationDatabaseDialog::OnSkeletonSelectionChanged(const FAssetData& AssetData)
{
	SelectedSkeleton = AssetData;
	CreateBonePicker();
}

void SCreateAnimationDatabaseDialog::CloseDialog(bool bValidSkeletonSelected /*= false*/)
{
	bConfirmClicked = bValidSkeletonSelected;
	if (SkeletonSelectionModalWindow.IsValid())
	{
		SkeletonSelectionModalWindow.Pin()->RequestDestroyWindow();
	}
}

FReply SCreateAnimationDatabaseDialog::OnCancelClicked()
{
	CloseDialog();
	return FReply::Handled();
}

FReply SCreateAnimationDatabaseDialog::OnConfirmClicked()
{
	AnimationDatabaseFactory->MotionMatchingBones.Empty();

	// Pass the selected Bones for Motion Matching
	if (BonePickerItems.Num() > 0)
	{
		for (const TSharedPtr<SBonePickerItem>& BoneItem : BonePickerItems)
		{
			if (BoneItem.IsValid() && BoneItem->IsBoneUsedForMotionMatching())
			{
				const FName BoneName = BoneItem.Get()->GetBoneName();
				AnimationDatabaseFactory->MotionMatchingBones.Add(BoneName);
			}
		}
	}

	// Pass the selected Skeleton for Motion Matching
	if (AnimationDatabaseFactory.IsValid())
	{
		AnimationDatabaseFactory->MotionMatchingSkeleton = Cast<USkeleton>(SelectedSkeleton.GetAsset());
	}

	if (!SelectedSkeleton.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ValidSkeletonRequired", "You must specify a valid Skeleton for the Animation Database."));
		return FReply::Handled();
	}

	CloseDialog(true);

	return FReply::Handled();
}

void SCreateAnimationDatabaseDialog::CreateBonePicker()
{
	BonePickerItems.Empty();
	SkeletonBoneContainer->ClearChildren();

	if (SelectedSkeleton.IsValid())
	{
		// Load the asset if it is not already loaded
		if (!SelectedSkeleton.IsAssetLoaded())
		{
			SelectedSkeleton.GetPackage()->FullyLoad();
		}

		// Get the Selected Skeleton and loop through the bones
		if (USkeleton* Skeleton = Cast<USkeleton>(SelectedSkeleton.GetAsset()))
		{
			for (int BoneIndex = 1; BoneIndex < Skeleton->GetReferenceSkeleton().GetNum(); ++BoneIndex)
			{
				const FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
				BonePickerItems.Add(SNew(SBonePickerItem, BoneIndex, false, BoneName));

				// Add the Bone Item to the Container
				SkeletonBoneContainer->AddSlot()
				[
					BonePickerItems.Last().ToSharedRef()
				];
			}
		}
	}
}

void SCreateAnimationDatabaseDialog::CreateSkeletonPicker()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SCreateAnimationDatabaseDialog::OnSkeletonSelectionChanged);
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;
	//AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SMotionFieldCreateDialog::FilterSkeletonBasedOnParentClass);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetSelection = SelectedSkeleton;

	SkeletonContainer->ClearChildren();
	SkeletonContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 10.0f, 0.0f, 10.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("TargetSkeleton", "Pick a skeleton that will be used for Motion Matching: " LINE_TERMINATOR "This should be the Skeleton that you are using for your Character."))
		.ShadowOffset(FVector2D(1.0f, 1.0f))
	];

	SkeletonContainer->AddSlot()
	[
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
	];
}

#undef LOCTEXT_NAMESPACE
