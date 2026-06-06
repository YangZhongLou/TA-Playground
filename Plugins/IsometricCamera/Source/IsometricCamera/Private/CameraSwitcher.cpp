#include "CameraSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ACameraSwitcher::ACameraSwitcher()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACameraSwitcher::RegisterCamera(AActor* CameraActor, const FString& CameraName)
{
    if (!CameraActor)
    {
        return;
    }

    int32 ExistingIndex = FindCameraIndex(CameraName);
    if (ExistingIndex >= 0)
    {
        RegisteredCameras[ExistingIndex] = CameraActor;
        CameraNames[ExistingIndex] = CameraName;
    }
    else
    {
        RegisteredCameras.Add(CameraActor);
        CameraNames.Add(CameraName);
    }
}

void ACameraSwitcher::UnregisterCamera(const FString& CameraName)
{
    int32 Index = FindCameraIndex(CameraName);
    if (Index >= 0)
    {
        RegisteredCameras.RemoveAt(Index);
        CameraNames.RemoveAt(Index);

        if (CurrentCameraIndex > Index)
        {
            CurrentCameraIndex--;
        }
        else if (CurrentCameraIndex == Index)
        {
            CurrentCameraIndex = -1;
        }

        if (CurrentCameraIndex >= RegisteredCameras.Num())
        {
            CurrentCameraIndex = RegisteredCameras.Num() - 1;
        }
    }
}

void ACameraSwitcher::ClearCameras()
{
    RegisteredCameras.Empty();
    CameraNames.Empty();
    CurrentCameraIndex = -1;
}

int32 ACameraSwitcher::GetCameraCount() const
{
    return RegisteredCameras.Num();
}

TArray<FString> ACameraSwitcher::GetCameraNames() const
{
    return CameraNames;
}

void ACameraSwitcher::SwitchToCamera(const FString& CameraName, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunc)
{
    int32 Index = FindCameraIndex(CameraName);
    if (Index >= 0)
    {
        SwitchToCameraByIndex(Index, BlendTime, BlendFunc);
    }
}

void ACameraSwitcher::SwitchToCameraByIndex(int32 Index, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunc)
{
    if (!RegisteredCameras.IsValidIndex(Index))
    {
        return;
    }

    AActor* TargetActor = RegisteredCameras[Index];
    if (!TargetActor)
    {
        return;
    }

    PerformSwitch(TargetActor, BlendTime, BlendFunc);
    CurrentCameraIndex = Index;
}

void ACameraSwitcher::NextCamera(float BlendTime)
{
    if (RegisteredCameras.Num() == 0)
    {
        return;
    }

    int32 NextIndex = CurrentCameraIndex + 1;
    if (NextIndex >= RegisteredCameras.Num())
    {
        NextIndex = 0;
    }
    SwitchToCameraByIndex(NextIndex, BlendTime, DefaultBlendFunction);
}

void ACameraSwitcher::PreviousCamera(float BlendTime)
{
    if (RegisteredCameras.Num() == 0)
    {
        return;
    }

    int32 PrevIndex = CurrentCameraIndex - 1;
    if (PrevIndex < 0)
    {
        PrevIndex = RegisteredCameras.Num() - 1;
    }
    SwitchToCameraByIndex(PrevIndex, BlendTime, DefaultBlendFunction);
}

FString ACameraSwitcher::GetCurrentCameraName() const
{
    if (RegisteredCameras.IsValidIndex(CurrentCameraIndex))
    {
        return CameraNames[CurrentCameraIndex];
    }
    return FString();
}

void ACameraSwitcher::PerformSwitch(AActor* TargetActor, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunc)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && TargetActor)
    {
        PC->SetViewTargetWithBlend(TargetActor, BlendTime, BlendFunc, DefaultBlendExp, false);
    }
}

int32 ACameraSwitcher::FindCameraIndex(const FString& CameraName) const
{
    return CameraNames.IndexOfByKey(CameraName);
}
