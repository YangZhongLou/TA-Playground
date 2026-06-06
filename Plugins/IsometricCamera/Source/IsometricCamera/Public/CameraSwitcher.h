#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraSwitcher.generated.h"

UCLASS()
class ISOMETRICCAMERA_API ACameraSwitcher : public AActor
{
    GENERATED_BODY()

public:
    ACameraSwitcher();

    // ========== 相机注册 ==========
    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void RegisterCamera(AActor* CameraActor, const FString& CameraName);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void UnregisterCamera(const FString& CameraName);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void ClearCameras();

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    int32 GetCameraCount() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    TArray<FString> GetCameraNames() const;

    // ========== 切换控制 ==========
    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void SwitchToCamera(const FString& CameraName, float BlendTime = 1.0f, TEnumAsByte<EViewTargetBlendFunction> BlendFunc = VTBlend_Cubic);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void SwitchToCameraByIndex(int32 Index, float BlendTime = 1.0f, TEnumAsByte<EViewTargetBlendFunction> BlendFunc = VTBlend_Cubic);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void NextCamera(float BlendTime = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    void PreviousCamera(float BlendTime = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    FString GetCurrentCameraName() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Switcher")
    int32 GetCurrentCameraIndex() const { return CurrentCameraIndex; }

    // ========== 预设配置 ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Switcher")
    float DefaultBlendTime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Switcher")
    TEnumAsByte<EViewTargetBlendFunction> DefaultBlendFunction = VTBlend_Cubic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Switcher")
    float DefaultBlendExp = 2.0f;

protected:
    UPROPERTY()
    TArray<TObjectPtr<AActor>> RegisteredCameras;

    UPROPERTY()
    TArray<FString> CameraNames;

    int32 CurrentCameraIndex = -1;

    void PerformSwitch(AActor* TargetActor, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunc);
    int32 FindCameraIndex(const FString& CameraName) const;
};
