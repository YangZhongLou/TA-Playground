#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraRigActor.generated.h"

class USplineComponent;
class AIsometricCameraPawn;

UENUM(BlueprintType)
enum class ECameraRigPlaybackMode : uint8
{
    Once,
    Loop,
    PingPong
};

UCLASS()
class ISOMETRICCAMERA_API ACameraRigActor : public AActor
{
    GENERATED_BODY()

public:
    ACameraRigActor();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // ========== 播放控制 ==========
    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void StartPlayback(AIsometricCameraPawn* TargetPawn);

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void StopPlayback();

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void PausePlayback();

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void ResumePlayback();

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void SetPlaybackSpeed(float NewSpeed);

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    float GetPlaybackSpeed() const { return PlaybackSpeed; }

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void SetPlaybackPosition(float NormalizedPosition);

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    float GetPlaybackPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    bool IsPlaying() const { return bIsPlaying; }

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    ECameraRigPlaybackMode GetPlaybackMode() const { return PlaybackMode; }

    UFUNCTION(BlueprintCallable, Category = "Camera Rig")
    void SetPlaybackMode(ECameraRigPlaybackMode NewMode) { PlaybackMode = NewMode; }

    // ========== 路径编辑（蓝图/Editor 用） ==========
    UFUNCTION(BlueprintCallable, Category = "Camera Rig|Path")
    void AddPathPoint(FVector Location, FVector TangentIn, FVector TangentOut);

    UFUNCTION(BlueprintCallable, Category = "Camera Rig|Path")
    void ClearPath();

    UFUNCTION(BlueprintCallable, Category = "Camera Rig|Path")
    int32 GetPathPointCount() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Rig|Path")
    void SetPathPointLocation(int32 Index, FVector Location);

    UFUNCTION(BlueprintCallable, Category = "Camera Rig|Path")
    USplineComponent* GetSplineComponent() const { return SplineComponent; }

protected:
    // ========== 组件 ==========
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Rig")
    TObjectPtr<USplineComponent> SplineComponent;

    // ========== 可配置参数 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
    float PlaybackSpeed = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
    ECameraRigPlaybackMode PlaybackMode = ECameraRigPlaybackMode::Loop;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
    bool bAutoStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
    bool bOrientToPath = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rig")
    float RotationSmoothing = 5.0f;

    // ========== 运行时状态 ==========
    UPROPERTY()
    TObjectPtr<AIsometricCameraPawn> TargetCameraPawn;

    float CurrentDistance = 0.0f;
    float TotalSplineLength = 0.0f;
    bool bIsPlaying = false;
    bool bIsPaused = false;
    bool bReverseDirection = false;

    void UpdateCameraAlongPath(float DeltaTime);
    void FinishPlayback();
};
