#include "IsometricCameraPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "CineCameraComponent.h"
#include "GameFramework/PlayerController.h"

AIsometricCameraPawn::AIsometricCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // 弹簧臂
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    RootComponent = CameraBoom;
    CameraBoom->SetRelativeRotation(FRotator(-45.0f, 45.0f, 0.0f));
    CameraBoom->TargetArmLength = 2500.0f;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = false;

    // 相机（使用 CineCamera 组件以支持电影级参数）
    FollowCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    // RelativeRotation = (0,0,0)，继承 SpringArm 旋转，自然形成俯视

    // 运行时状态初始化
    TargetArmLength = 2500.0f;
    TargetFOV = 90.0f;
    CurrentFOV = 90.0f;
    TargetDOFFocalDistance = DefaultDOFFocalDistance;
    TargetDOFFocalRegion = DefaultDOFFocalRegion;
    TargetExposureBias = 0.0f;
    TargetBloomIntensity = 0.675f;
    TargetMotionBlurAmount = 0.0f;
    TargetVignetteIntensity = 0.0f;
    TargetChromaticAberrationIntensity = 0.0f;
    TargetYawRotation = 45.0f;
    CurrentYawRotation = 45.0f;
    bRightMousePressed = false;
    RightClickPanSpeed = 2.0f;

    // 后处理默认设置
    FollowCamera->PostProcessBlendWeight = 1.0f;
}

void AIsometricCameraPawn::BeginPlay()
{
    Super::BeginPlay();
    TargetLocation = GetActorLocation();

    // 同步 CurrentFOV 与 CineCameraComponent 的实际 FOV（避免初始状态不一致）
    if (UCineCameraComponent* CineCam = GetCineCamera())
    {
        CurrentFOV = CineCam->GetHorizontalFieldOfView();
        TargetFOV = CurrentFOV;
    }

    // 显示鼠标光标，并确保视角绑定到本 Pawn
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetViewTarget(this);
    }
}

void AIsometricCameraPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateEdgeScroll(DeltaTime); // 向 InputDirection 累加边缘输入
    UpdateRightClickPan(DeltaTime); // 处理右键拖拽平移

    if (!InputDirection.IsNearlyZero())
    {
        InputDirection.Z = 0.0f;
        InputDirection.Normalize();

        // 基于相机 Yaw 的水平面移动（Pitch 锁定为 0，保持平面移动）
        const FRotator YawRotation(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
        const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDir   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        FVector DeltaMove = (ForwardDir * InputDirection.X + RightDir * InputDirection.Y)
                            * MovementSpeed * DeltaTime;
        TargetLocation += DeltaMove;
        InputDirection = FVector::ZeroVector; // 每帧清零，避免累积
    }

    // 边界限制（在 TargetLocation 上硬截断，再由 InterpTo 平滑追赶，
    // 边界处会有轻微"粘滞感"——这是预期行为，非 bug）
    if (bEnableBounds)
    {
        TargetLocation.X = FMath::Clamp(TargetLocation.X, BoundsMin.X, BoundsMax.X);
        TargetLocation.Y = FMath::Clamp(TargetLocation.Y, BoundsMin.Y, BoundsMax.Y);
    }

    // 平滑插值（位置用 Lerp，ArmLength 用 FInterpTo）
    FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = FMath::Lerp(CurrentLocation, TargetLocation,
        FMath::Clamp(DeltaTime * MovementSmoothing, 0.0f, 1.0f));
    SetActorLocation(NewLocation);

    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength,
        TargetArmLength, DeltaTime, ZoomSmoothing);

    // Yaw 旋转平滑插值
    CurrentYawRotation = FMath::FInterpTo(CurrentYawRotation, TargetYawRotation, DeltaTime, RotationSmoothing);
    if (CameraBoom)
    {
        FRotator NewRotation = CameraBoom->GetRelativeRotation();
        NewRotation.Yaw = CurrentYawRotation;
        CameraBoom->SetRelativeRotation(NewRotation);
    }

    // FOV 平滑插值（CineCameraComponent 通过焦距控制 FOV，SetFieldOfView 会被覆盖）
    if (FollowCamera)
    {
        CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, FOVSmoothing);

        if (UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(FollowCamera))
        {
            // FOV = 2 * atan(SensorWidth / (2 * FocalLength))
            // => FocalLength = SensorWidth / (2 * tan(FOV / 2))
            const float SensorWidth = CineCam->Filmback.SensorWidth;
            const float FocalLength = SensorWidth / (2.0f * FMath::Tan(FMath::DegreesToRadians(CurrentFOV) / 2.0f));
            CineCam->SetCurrentFocalLength(FMath::Clamp(FocalLength, 1.0f, 1000.0f));
        }
        else
        {
            FollowCamera->SetFieldOfView(CurrentFOV);
        }

        // DOF 平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance)
        {
            float CurrentFocalDistance = FollowCamera->PostProcessSettings.DepthOfFieldFocalDistance;
            float NewFocalDistance = FMath::FInterpTo(CurrentFocalDistance, TargetDOFFocalDistance, DeltaTime, DOFSmoothing);
            FollowCamera->PostProcessSettings.DepthOfFieldFocalDistance = NewFocalDistance;
        }
        if (FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalRegion)
        {
            float CurrentFocalRegion = FollowCamera->PostProcessSettings.DepthOfFieldFocalRegion;
            float NewFocalRegion = FMath::FInterpTo(CurrentFocalRegion, TargetDOFFocalRegion, DeltaTime, DOFSmoothing);
            FollowCamera->PostProcessSettings.DepthOfFieldFocalRegion = NewFocalRegion;
        }

        // 曝光平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_AutoExposureBias)
        {
            float CurrentExposure = FollowCamera->PostProcessSettings.AutoExposureBias;
            float NewExposure = FMath::FInterpTo(CurrentExposure, TargetExposureBias, DeltaTime, PostProcessSmoothing);
            FollowCamera->PostProcessSettings.AutoExposureBias = NewExposure;
        }

        // Bloom 平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_BloomIntensity)
        {
            float CurrentBloom = FollowCamera->PostProcessSettings.BloomIntensity;
            float NewBloom = FMath::FInterpTo(CurrentBloom, TargetBloomIntensity, DeltaTime, PostProcessSmoothing);
            FollowCamera->PostProcessSettings.BloomIntensity = NewBloom;
        }

        // Motion Blur 平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount)
        {
            float CurrentBlur = FollowCamera->PostProcessSettings.MotionBlurAmount;
            float NewBlur = FMath::FInterpTo(CurrentBlur, TargetMotionBlurAmount, DeltaTime, PostProcessSmoothing);
            FollowCamera->PostProcessSettings.MotionBlurAmount = NewBlur;
        }

        // Vignette 平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_VignetteIntensity)
        {
            float CurrentVignette = FollowCamera->PostProcessSettings.VignetteIntensity;
            float NewVignette = FMath::FInterpTo(CurrentVignette, TargetVignetteIntensity, DeltaTime, PostProcessSmoothing);
            FollowCamera->PostProcessSettings.VignetteIntensity = NewVignette;
        }

        // Chromatic Aberration 平滑插值
        if (FollowCamera->PostProcessSettings.bOverride_SceneFringeIntensity)
        {
            float CurrentCA = FollowCamera->PostProcessSettings.SceneFringeIntensity;
            float NewCA = FMath::FInterpTo(CurrentCA, TargetChromaticAberrationIntensity, DeltaTime, PostProcessSmoothing);
            FollowCamera->PostProcessSettings.SceneFringeIntensity = NewCA;
        }
    }
}

void AIsometricCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &AIsometricCameraPawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AIsometricCameraPawn::MoveRight);
    PlayerInputComponent->BindAxis("Zoom", this, &AIsometricCameraPawn::Zoom);
    PlayerInputComponent->BindAxis("RotateYaw", this, &AIsometricCameraPawn::RotateYaw);
    PlayerInputComponent->BindAction("RightMouseButton", IE_Pressed, this, &AIsometricCameraPawn::OnRightMousePressed);
    PlayerInputComponent->BindAction("RightMouseButton", IE_Released, this, &AIsometricCameraPawn::OnRightMouseReleased);
}

void AIsometricCameraPawn::MoveForward(float Value)
{
    InputDirection.X += Value;
}

void AIsometricCameraPawn::MoveRight(float Value)
{
    InputDirection.Y += Value;
}

void AIsometricCameraPawn::SetCameraTargetLocation(FVector NewLocation)
{
    TargetLocation = NewLocation;
}

void AIsometricCameraPawn::SetCameraTargetZoom(float NewArmLength)
{
    TargetArmLength = FMath::Clamp(NewArmLength, MinZoomDistance, MaxZoomDistance);
}

void AIsometricCameraPawn::ForceInstantState(FVector NewLocation, float NewArmLength)
{
    TargetLocation = NewLocation;
    TargetArmLength = FMath::Clamp(NewArmLength, MinZoomDistance, MaxZoomDistance);
    SetActorLocation(TargetLocation);
    if (CameraBoom)
    {
        CameraBoom->TargetArmLength = TargetArmLength;
    }
}

void AIsometricCameraPawn::SetCameraTargetFOV(float NewFOV)
{
    TargetFOV = FMath::Clamp(NewFOV, MinFOV, MaxFOV);
}

float AIsometricCameraPawn::GetCameraFOV() const
{
    if (UCineCameraComponent* CineCam = GetCineCamera())
    {
        return CineCam->GetHorizontalFieldOfView();
    }
    return FollowCamera ? FollowCamera->FieldOfView : 90.0f;
}

void AIsometricCameraPawn::SetCameraTargetDOF(float FocalDistance, float FocalRegion)
{
    TargetDOFFocalDistance = FMath::Clamp(FocalDistance, 0.0f, 100000.0f);
    TargetDOFFocalRegion = FMath::Clamp(FocalRegion, 0.0f, 100000.0f);

    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
        FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalRegion = true;
        FollowCamera->PostProcessSettings.DepthOfFieldFocalDistance = TargetDOFFocalDistance;
        FollowCamera->PostProcessSettings.DepthOfFieldFocalRegion = TargetDOFFocalRegion;
    }
}

float AIsometricCameraPawn::GetDOFFocalDistance() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.DepthOfFieldFocalDistance : 0.0f;
}

float AIsometricCameraPawn::GetDOFFocalRegion() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.DepthOfFieldFocalRegion : 0.0f;
}

void AIsometricCameraPawn::SetPostProcessExposure(float ExposureBias)
{
    TargetExposureBias = FMath::Clamp(ExposureBias, -10.0f, 10.0f);

    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
        FollowCamera->PostProcessSettings.AutoExposureBias = TargetExposureBias;
    }
}

float AIsometricCameraPawn::GetPostProcessExposure() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.AutoExposureBias : 0.0f;
}

void AIsometricCameraPawn::SetPostProcessBloom(float Intensity)
{
    TargetBloomIntensity = FMath::Clamp(Intensity, 0.0f, 10.0f);

    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_BloomIntensity = true;
        FollowCamera->PostProcessSettings.BloomIntensity = TargetBloomIntensity;
    }
}

float AIsometricCameraPawn::GetPostProcessBloom() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.BloomIntensity : 0.0f;
}

void AIsometricCameraPawn::ResetPostProcessSettings()
{
    TargetFOV = 90.0f;
    TargetDOFFocalDistance = DefaultDOFFocalDistance;
    TargetDOFFocalRegion = DefaultDOFFocalRegion;
    TargetExposureBias = 0.0f;
    TargetBloomIntensity = 0.675f;
    TargetMotionBlurAmount = 0.0f;
    TargetVignetteIntensity = 0.0f;
    TargetChromaticAberrationIntensity = 0.0f;

    if (FollowCamera)
    {
        CurrentFOV = TargetFOV;

        if (UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(FollowCamera))
        {
            const float SensorWidth = CineCam->Filmback.SensorWidth;
            const float FocalLength = SensorWidth / (2.0f * FMath::Tan(FMath::DegreesToRadians(CurrentFOV) / 2.0f));
            CineCam->SetCurrentFocalLength(FMath::Clamp(FocalLength, 1.0f, 1000.0f));
        }
        else
        {
            FollowCamera->SetFieldOfView(TargetFOV);
        }

        FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
        FollowCamera->PostProcessSettings.bOverride_DepthOfFieldFocalRegion = false;
        FollowCamera->PostProcessSettings.bOverride_AutoExposureBias = false;
        FollowCamera->PostProcessSettings.bOverride_BloomIntensity = false;
        FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = false;
        FollowCamera->PostProcessSettings.bOverride_VignetteIntensity = false;
        FollowCamera->PostProcessSettings.bOverride_SceneFringeIntensity = false;
    }
}

void AIsometricCameraPawn::Zoom(float Value)
{
    // 滚轮向前（远离用户）= Value > 0 → 拉近 = ArmLength 减小
    TargetArmLength -= Value * ZoomSpeed;
    TargetArmLength = FMath::Clamp(TargetArmLength, MinZoomDistance, MaxZoomDistance);
}

void AIsometricCameraPawn::UpdateEdgeScroll(float DeltaTime)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY))
    {
        return;
    }

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

    FVector2D EdgeInput(0.0f);

    if (MouseX < EdgeScrollThreshold)                   EdgeInput.X -= 1; // 左
    if (MouseX > ViewportSizeX - EdgeScrollThreshold)   EdgeInput.X += 1; // 右
    if (MouseY < EdgeScrollThreshold)                   EdgeInput.Y += 1; // 上
    if (MouseY > ViewportSizeY - EdgeScrollThreshold)   EdgeInput.Y -= 1; // 下

    if (!EdgeInput.IsNearlyZero())
    {
        EdgeInput.Normalize();
        // EdgeInput.X = 左/右, EdgeInput.Y = 前/后（与 WASD 统一）
        InputDirection += FVector(EdgeInput.Y, EdgeInput.X, 0.0f);
    }
}

void AIsometricCameraPawn::RotateYaw(float Value)
{
    if (FMath::IsNearlyZero(Value))
    {
        return;
    }
    TargetYawRotation += Value * RotationSpeed;
}

void AIsometricCameraPawn::OnRightMousePressed()
{
    bRightMousePressed = true;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        float MouseX, MouseY;
        if (PC->GetMousePosition(MouseX, MouseY))
        {
            LastMousePosition = FVector2D(MouseX, MouseY);
        }
    }
}

void AIsometricCameraPawn::OnRightMouseReleased()
{
    bRightMousePressed = false;
}

void AIsometricCameraPawn::UpdateRightClickPan(float DeltaTime)
{
    if (!bRightMousePressed)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    float MouseX, MouseY;
    if (!PC->GetMousePosition(MouseX, MouseY))
    {
        return;
    }

    FVector2D CurrentMousePos(MouseX, MouseY);
    FVector2D Delta = CurrentMousePos - LastMousePosition;
    LastMousePosition = CurrentMousePos;

    if (Delta.IsNearlyZero())
    {
        return;
    }

    // 基于相机 Yaw 的水平面平移（与 WASD 一致，但由鼠标驱动）
    const FRotator YawRotation(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
    const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDir   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    // Delta.Y = 鼠标上下移动 → 前后平移（屏幕空间上=前，下=后）
    // Delta.X = 鼠标左右移动 → 左右平移（屏幕空间左=左，右=右）
    FVector DeltaMove = (ForwardDir * Delta.Y + RightDir * -Delta.X)
                        * RightClickPanSpeed;
    TargetLocation += DeltaMove;
}

// ========== CineCamera 电影级参数 ==========

void AIsometricCameraPawn::SetCameraFocalLength(float InFocalLength)
{
    if (UCineCameraComponent* CineCam = GetCineCamera())
    {
        CineCam->SetCurrentFocalLength(FMath::Clamp(InFocalLength, 1.0f, 1000.0f));
    }
}

float AIsometricCameraPawn::GetCameraFocalLength() const
{
    return FollowCamera ? FollowCamera->CurrentFocalLength : 0.0f;
}

void AIsometricCameraPawn::SetCameraAperture(float InAperture)
{
    if (UCineCameraComponent* CineCam = GetCineCamera())
    {
        CineCam->SetCurrentAperture(FMath::Clamp(InAperture, 0.1f, 64.0f));
    }
}

float AIsometricCameraPawn::GetCameraAperture() const
{
    return FollowCamera ? FollowCamera->CurrentAperture : 0.0f;
}

void AIsometricCameraPawn::SetCameraFocusDistance(float InFocusDistance)
{
    if (UCineCameraComponent* CineCam = GetCineCamera())
    {
        FCameraFocusSettings NewFocusSettings = CineCam->FocusSettings;
        NewFocusSettings.FocusMethod = ECameraFocusMethod::Manual;
        NewFocusSettings.ManualFocusDistance = FMath::Clamp(InFocusDistance, 0.0f, 100000.0f);
        CineCam->SetFocusSettings(NewFocusSettings);
    }
}

float AIsometricCameraPawn::GetCameraFocusDistance() const
{
    return FollowCamera ? FollowCamera->CurrentFocusDistance : 0.0f;
}

float AIsometricCameraPawn::GetCameraHorizontalFOV() const
{
    return FollowCamera ? FollowCamera->GetHorizontalFieldOfView() : 0.0f;
}

float AIsometricCameraPawn::GetCameraVerticalFOV() const
{
    return FollowCamera ? FollowCamera->GetVerticalFieldOfView() : 0.0f;
}

// ========== 高级后处理实验 ==========

void AIsometricCameraPawn::SetPostProcessMotionBlur(float Amount)
{
    TargetMotionBlurAmount = FMath::Clamp(Amount, 0.0f, 1.0f);
    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
        FollowCamera->PostProcessSettings.MotionBlurAmount = TargetMotionBlurAmount;
    }
}

float AIsometricCameraPawn::GetPostProcessMotionBlur() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.MotionBlurAmount : 0.0f;
}

void AIsometricCameraPawn::SetPostProcessVignette(float Intensity)
{
    TargetVignetteIntensity = FMath::Clamp(Intensity, 0.0f, 10.0f);
    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
        FollowCamera->PostProcessSettings.VignetteIntensity = TargetVignetteIntensity;
    }
}

float AIsometricCameraPawn::GetPostProcessVignette() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.VignetteIntensity : 0.0f;
}

void AIsometricCameraPawn::SetPostProcessChromaticAberration(float Intensity)
{
    TargetChromaticAberrationIntensity = FMath::Clamp(Intensity, 0.0f, 10.0f);
    if (FollowCamera)
    {
        FollowCamera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
        FollowCamera->PostProcessSettings.SceneFringeIntensity = TargetChromaticAberrationIntensity;
    }
}

float AIsometricCameraPawn::GetPostProcessChromaticAberration() const
{
    return FollowCamera ? FollowCamera->PostProcessSettings.SceneFringeIntensity : 0.0f;
}
