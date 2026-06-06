#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "IsometricCameraPawn.generated.h"

class USpringArmComponent;
class UCineCameraComponent;

UCLASS()
class ISOMETRICCAMERA_API AIsometricCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    AIsometricCameraPawn();

    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // ========== 组件访问（测试/蓝图用） ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    UCineCameraComponent* GetFollowCamera() const { return FollowCamera; }

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    UCineCameraComponent* GetCineCamera() const { return FollowCamera; }

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Bounds")
    bool AreBoundsEnabled() const { return bEnableBounds; }

    // ========== 运行时公开接口（供测试/调试调用） ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    void SetCameraTargetLocation(FVector NewLocation);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    void SetCameraTargetZoom(float NewArmLength);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    void ForceInstantState(FVector NewLocation, float NewArmLength);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    float GetMinZoomDistance() const { return MinZoomDistance; }

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    float GetMaxZoomDistance() const { return MaxZoomDistance; }

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|Debug")
    float GetCameraTargetZoom() const { return TargetArmLength; }

    // ========== FOV 控制 ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|FOV")
    void SetCameraTargetFOV(float NewFOV);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|FOV")
    float GetCameraFOV() const;

    // ========== 景深 (Depth of Field) ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|DOF")
    void SetCameraTargetDOF(float FocalDistance, float FocalRegion);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|DOF")
    float GetDOFFocalDistance() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|DOF")
    float GetDOFFocalRegion() const;

    // ========== 后处理 ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void SetPostProcessExposure(float ExposureBias);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    float GetPostProcessExposure() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void SetPostProcessBloom(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    float GetPostProcessBloom() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void ResetPostProcessSettings();

    // ========== 高级后处理实验 ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void SetPostProcessMotionBlur(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    float GetPostProcessMotionBlur() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void SetPostProcessVignette(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    float GetPostProcessVignette() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    void SetPostProcessChromaticAberration(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|PostProcess")
    float GetPostProcessChromaticAberration() const;

    // ========== CineCamera 电影级参数 ==========
    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    void SetCameraFocalLength(float InFocalLength);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    float GetCameraFocalLength() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    void SetCameraAperture(float InAperture);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    float GetCameraAperture() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    void SetCameraFocusDistance(float InFocusDistance);

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    float GetCameraFocusDistance() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    float GetCameraHorizontalFOV() const;

    UFUNCTION(BlueprintCallable, Category = "Isometric Camera|CineCamera")
    float GetCameraVerticalFOV() const;

protected:
    virtual void BeginPlay() override;

    // ========== 组件 ==========
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCineCameraComponent> FollowCamera;

    // ========== 可配置参数 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Movement")
    float MovementSpeed = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Movement")
    float MovementSmoothing = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Edge Scroll")
    float EdgeScrollThreshold = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Zoom")
    float MinZoomDistance = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Zoom")
    float MaxZoomDistance = 8000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Zoom")
    float ZoomSpeed = 2000.0f; // 每格滚轮变化 2000；如体验过快可降至 500~1000

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Zoom")
    float ZoomSmoothing = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Rotation")
    float RotationSpeed = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Rotation")
    float RotationSmoothing = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Bounds")
    bool bEnableBounds = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Bounds")
    FVector2D BoundsMin = FVector2D(-5000.0f, -5000.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|Bounds")
    FVector2D BoundsMax = FVector2D(5000.0f, 5000.0f);

    // ========== FOV 配置 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|FOV")
    float MinFOV = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|FOV")
    float MaxFOV = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|FOV")
    float FOVSmoothing = 8.0f;

    // ========== DOF 配置 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|DOF")
    float DefaultDOFFocalDistance = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|DOF")
    float DefaultDOFFocalRegion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|DOF")
    float DOFSmoothing = 5.0f;

    // ========== 后处理配置 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Isometric Camera|PostProcess")
    float PostProcessSmoothing = 5.0f;

    // ========== 运行时状态 ==========
    FVector TargetLocation;
    float TargetArmLength;
    FVector InputDirection; // 每帧累加的原始输入方向（WASD + 边缘滚动 + 右键拖拽），在 Tick 中归一化后应用

    // FOV
    float TargetFOV;
    float CurrentFOV;

    // DOF
    float TargetDOFFocalDistance;
    float TargetDOFFocalRegion;

    // Post-process
    float TargetExposureBias;
    float TargetBloomIntensity;
    float TargetMotionBlurAmount;
    float TargetVignetteIntensity;
    float TargetChromaticAberrationIntensity;

    // Rotation
    float TargetYawRotation;
    float CurrentYawRotation;

    // Right-click panning
    bool bRightMousePressed;
    FVector2D LastMousePosition;
    float RightClickPanSpeed;

    // ========== 输入回调 ==========
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Zoom(float Value);
    void RotateYaw(float Value);
    void OnRightMousePressed();
    void OnRightMouseReleased();

    // ========== 边缘检测 ==========
    void UpdateEdgeScroll(float DeltaTime);

    // ========== 右键拖拽平移 ==========
    void UpdateRightClickPan(float DeltaTime);
};
