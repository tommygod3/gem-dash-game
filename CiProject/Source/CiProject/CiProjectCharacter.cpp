// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CiProjectCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h" 
#include "PaperFlipbook.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

//////////////////////////////////////////////////////////////////////////
// ACiProjectCharacter

ACiProjectCharacter::ACiProjectCharacter()
{
	//Load in flipbooks
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UPaperFlipbook> RunningAnimationAsset;
		ConstructorHelpers::FObjectFinderOptional<UPaperFlipbook> IdleAnimationAsset;
		ConstructorHelpers::FObjectFinderOptional<UPaperFlipbook> JumpingAnimationAsset;
		ConstructorHelpers::FObjectFinderOptional<UPaperFlipbook> DyingAnimationAsset;
		ConstructorHelpers::FObjectFinderOptional<UPaperFlipbook> DeadAnimationAsset;
		FConstructorStatics()
			: RunningAnimationAsset(TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Walking.Walking'"))
			, IdleAnimationAsset(TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Idle.Idle'"))
			, JumpingAnimationAsset(TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Jumping.Jumping'"))
			, DyingAnimationAsset(TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Dying.Dying'"))
			, DeadAnimationAsset(TEXT("PaperFlipbook'/Game/Assets/Flipbooks/Dead.Dead'"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	RunningAnimation = ConstructorStatics.RunningAnimationAsset.Get();
	IdleAnimation = ConstructorStatics.IdleAnimationAsset.Get();
	JumpingAnimation = ConstructorStatics.JumpingAnimationAsset.Get();
	DyingAnimation = ConstructorStatics.DyingAnimationAsset.Get();
	DeadAnimation = ConstructorStatics.DeadAnimationAsset.Get();


	// Use only Yaw from the controller and ignore the rest of the rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Set the size of our collision capsule.
	GetCapsuleComponent()->SetCapsuleRadius(90);
	GetCapsuleComponent()->SetCapsuleHalfHeight(240);

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 3000.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 0.0f);
	CameraBoom->bAbsoluteRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->RelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	

	// Create an perspective camera and attach it to the boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->ProjectionMode = ECameraProjectionMode::Perspective;
	SideViewCameraComponent->FieldOfView = 90;
	SideViewCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Prevent all automatic rotation behavior on the camera, character, and camera component
	CameraBoom->bAbsoluteRotation = true;
	SideViewCameraComponent->bUsePawnControlRotation = false;
	SideViewCameraComponent->bAutoActivate = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Configure character movement
	GetCharacterMovement()->GravityScale = 15.0f;
	GetCharacterMovement()->AirControl = 5000.0f;
	GetCharacterMovement()->JumpZVelocity = 4300.f;
	GetCharacterMovement()->GroundFriction = 500.0f;
	GetCharacterMovement()->MaxWalkSpeed = 4000.0f;
	GetCharacterMovement()->MaxFlySpeed = 4000.0f;
	GetCharacterMovement()->MaxAcceleration = 16386.0f;
	//GetCharacterMovement()->FallingLateralFriction = 10000.0f;

	// Lock character motion onto the XZ plane, so the character can't move in or out of the screen
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, -1.0f, 0.0f));

	// Behave like a traditional 2D platformer character, with a flat bottom instead of a curved capsule bottom
	// Note: This can cause a little floating when going up inclines; you can choose the tradeoff between better
	// behavior on the edge of a ledge versus inclines by setting this to true or false
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

    // 	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
    // 	TextComponent->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
    // 	TextComponent->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
    // 	TextComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    // 	TextComponent->SetupAttachment(RootComponent);


	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;
}

//////////////////////////////////////////////////////////////////////////
// Animation

void ACiProjectCharacter::UpdateAnimation()
{
	const FVector PlayerVelocity = GetVelocity();
	const float PlayerSpeedSqr = PlayerVelocity.SizeSquared();

	//What state is the player in
	UPaperFlipbook* DesiredAnimation;
	if (!dead)
	{
		if (currentHealth <= 0)
		{
			DesiredAnimation = DyingAnimation;
		}
		else if (PlayerSpeedSqr == 0.0f)
		{
			DesiredAnimation = IdleAnimation;
		}
		else if (PlayerVelocity.Z != 0.0f)
		{
			DesiredAnimation = JumpingAnimation;
		}
		else
		{
			DesiredAnimation = RunningAnimation;
		}
	}
	else
	{
		DesiredAnimation = DeadAnimation;
	}
	
	if( GetSprite()->GetFlipbook() != DesiredAnimation 	)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);
	}
}

void ACiProjectCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateCharacter();	
	if (currentHealth <= 0 && !dead)
	{
		killCharacter();
	}
}

void ACiProjectCharacter::killCharacter()
{
	dead = 1;
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	this->DisableInput(PlayerController);

}

//////////////////////////////////////////////////////////////////////////
// Input

void ACiProjectCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Note: the 'Jump' action and the 'MoveRight' axis are bound to actual keys/buttons/sticks in DefaultInput.ini (editable from Project Settings..Input)
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACiProjectCharacter::MoveRight);

	PlayerInputComponent->BindTouch(IE_Pressed, this, &ACiProjectCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ACiProjectCharacter::TouchStopped);
}

void ACiProjectCharacter::MoveRight(float Value)
{
	/*UpdateChar();*/

	// Apply the input to the character motion
	AddMovementInput(FVector(100.0f, 0.0f, 0.0f), Value);
}

void ACiProjectCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Jump on any touch
	Jump();
}

void ACiProjectCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Cease jumping once touch stopped
	StopJumping();
}

void ACiProjectCharacter::UpdateCharacter()
{
	// Update animation to match the motion
	UpdateAnimation();

	// Now setup the rotation of the controller based on the direction we are travelling
	const FVector PlayerVelocity = GetVelocity();	
	float TravelDirection = PlayerVelocity.X;
	// Set the rotation so that the character faces his direction of travel.
	if (Controller != nullptr)
	{
		if (TravelDirection < 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (TravelDirection > 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}
