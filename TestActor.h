// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "TestActor.generated.h"

class FHttpModule;
UCLASS()
class MYPROJECT_API ATestActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	class FThreadActor* Thread;

	UTexture2D* LoadTexture2D_FromFile(const FString& FullFilePath);

	UFUNCTION(BlueprintCallable)
	TArray<UTexture2D*> GetImages();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	int32 Add(int32 a, int32 b);

	int32 number = 0;

	TArray<FString> GetUrl();

	TArray<bool> DownloadCheck;

	TArray<FString> Arr;

	uint32 ThreadCount;
};

class MYPROJECT_API FThreadActor : public FRunnable
{
public:
	FThreadActor();
	FThreadActor(const int32 n,ATestActor* Test);
	FThreadActor(const TArray<FString> URLs, const int32 n, ATestActor* Test);
	virtual ~FThreadActor() override;

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

	int32 ThreadId;
	int32 number;

	ATestActor* TestActor;

	TArray<FString> URL;

	void DownloadImage();

	FHttpModule* HttpModule;

private:
	FRunnableThread* Thread;
};
