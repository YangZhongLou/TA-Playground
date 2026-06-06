#include "CameraRigActor.h"
#include "Components/SplineComponent.h"
#include "IsometricCameraPawn.h"
#include "GameFramework/SpringArmComponent.h"

ACameraRigActor::ACameraRigActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    RootComponent = SplineComponent;

    // 默认创建一个矩形路径（4 个点）
    SplineComponent->SetClosedLoop(true);
}

void ACameraRigActor::BeginPlay()
{
    Super::BeginPlay();

    TotalSplineLength = SplineComponent->GetSplineLength();

    if (bAutoStart && TargetCameraPawn)
    {
        StartPlayback(TargetCameraPawn);
    }
}

void ACameraRigActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsPlaying && !bIsPaused && TargetCameraPawn)
    {
        UpdateCameraAlongPath(DeltaTime);
    }
}

void ACameraRigActor::StartPlayback(AIsometricCameraPawn* TargetPawn)
{
    if (!TargetPawn || TotalSplineLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    TargetCameraPawn = TargetPawn;
    bIsPlaying = true;
    bIsPaused = false;
    bReverseDirection = false;
    CurrentDistance = 0.0f;
}

void ACameraRigActor::StopPlayback()
{
    bIsPlaying = false;
    bIsPaused = false;
    CurrentDistance = 0.0f;
    bReverseDirection = false;

    // 恢复相机到原始位置（可选——由调用者决定是否恢复）
    if (TargetCameraPawn)
    {
        // 不强制恢复，让相机停在当前位置
        TargetCameraPawn = nullptr;
    }
}

void ACameraRigActor::PausePlayback()
{
    bIsPaused = true;
}

void ACameraRigActor::ResumePlayback()
{
    bIsPaused = false;
}

void ACameraRigActor::SetPlaybackSpeed(float NewSpeed)
{
    PlaybackSpeed = FMath::Max(NewSpeed, 0.0f);
}

void ACameraRigActor::SetPlaybackPosition(float NormalizedPosition)
{
    NormalizedPosition = FMath::Clamp(NormalizedPosition, 0.0f, 1.0f);
    CurrentDistance = NormalizedPosition * TotalSplineLength;
}

float ACameraRigActor::GetPlaybackPosition() const
{
    if (TotalSplineLength <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return CurrentDistance / TotalSplineLength;
}

void ACameraRigActor::AddPathPoint(FVector Location, FVector TangentIn, FVector TangentOut)
{
    int32 Index = SplineComponent->GetNumberOfSplinePoints();
    SplineComponent->AddSplinePoint(Location, ESplineCoordinateSpace::World);
    SplineComponent->SetTangentsAtSplinePoint(Index, TangentIn, TangentOut, ESplineCoordinateSpace::World);
    TotalSplineLength = SplineComponent->GetSplineLength();
}

void ACameraRigActor::ClearPath()
{
    SplineComponent->ClearSplinePoints();
    TotalSplineLength = 0.0f;
    CurrentDistance = 0.0f;
}

int32 ACameraRigActor::GetPathPointCount() const
{
    return SplineComponent->GetNumberOfSplinePoints();
}

void ACameraRigActor::SetPathPointLocation(int32 Index, FVector Location)
{
    if (Index >= 0 && Index < SplineComponent->GetNumberOfSplinePoints())
    {
        SplineComponent->SetLocationAtSplinePoint(Index, Location, ESplineCoordinateSpace::World);
        TotalSplineLength = SplineComponent->GetSplineLength();
    }
}

void ACameraRigActor::UpdateCameraAlongPath(float DeltaTime)
{
    float Direction = bReverseDirection ? -1.0f : 1.0f;
    CurrentDistance += Direction * PlaybackSpeed * DeltaTime;

    // 边界处理
    if (CurrentDistance >= TotalSplineLength)
    {
        if (PlaybackMode == ECameraRigPlaybackMode::Loop)
        {
            CurrentDistance = 0.0f;
        }
        else if (PlaybackMode == ECameraRigPlaybackMode::PingPong)
        {
            CurrentDistance = TotalSplineLength;
            bReverseDirection = true;
        }
        else // Once
        {
            CurrentDistance = TotalSplineLength;
            FinishPlayback();
            return;
        }
    }
    else if (CurrentDistance <= 0.0f)
    {
        if (PlaybackMode == ECameraRigPlaybackMode::Loop)
        {
            CurrentDistance = TotalSplineLength;
        }
        else if (PlaybackMode == ECameraRigPlaybackMode::PingPong)
        {
            CurrentDistance = 0.0f;
            bReverseDirection = false;
        }
        else // Once
        {
            CurrentDistance = 0.0f;
            FinishPlayback();
            return;
        }
    }

    // 采样样条位置和切线
    FVector NewLocation = SplineComponent->GetLocationAtDistanceAlongSpline(
        CurrentDistance, ESplineCoordinateSpace::World);

    TargetCameraPawn->SetCameraTargetLocation(NewLocation);

    // 如果启用路径朝向，根据切线调整相机 Yaw
    if (bOrientToPath)
    {
        FVector Tangent = SplineComponent->GetTangentAtDistanceAlongSpline(
            CurrentDistance, ESplineCoordinateSpace::World);
        Tangent.Z = 0.0f;
        if (!Tangent.IsNearlyZero())
        {
            Tangent.Normalize();
            float TargetYaw = FMath::Atan2(Tangent.Y, Tangent.X) * (180.0f / PI);
            // 转换为 SpringArm 的相对 Yaw（基础偏移 45°）
            // 这里直接用切线方向作为 Yaw
            if (USpringArmComponent* Boom = TargetCameraPawn->GetCameraBoom())
            {
                FRotator NewRotation = Boom->GetRelativeRotation();
                NewRotation.Yaw = FMath::FInterpTo(NewRotation.Yaw, TargetYaw, DeltaTime, RotationSmoothing);
                Boom->SetRelativeRotation(NewRotation);
            }
        }
    }
}

void ACameraRigActor::FinishPlayback()
{
    bIsPlaying = false;
    TargetCameraPawn = nullptr;
}
