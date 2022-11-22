// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor.h"
#include <functional>
#include <future>
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

FCriticalSection CS;

// Sets default values
ATestActor::ATestActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	Arr = GetUrl();

	Thread = new FThreadActor(Arr, number, this);
}

void ATestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

UTexture2D* ATestActor::LoadTexture2D_FromFile(const FString& FullFilePath)
{
	UTexture2D* LoadedT2D = nullptr;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	TArray<uint8> RawFileData;

	UE_LOG(LogTemp, Warning, TEXT("%s"), *FullFilePath);

	if (!FFileHelper::LoadFileToArray(RawFileData, *FullFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("failed loadtexture2d from file"));
		return nullptr;
	}

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{
		TArray<uint8> UncompressedBRGA;

		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBRGA))
		{
			LoadedT2D = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);

			if (!LoadedT2D)
			{
				return nullptr;
			}
			void* TextureData = LoadedT2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedBRGA.GetData(), UncompressedBRGA.Num());
			LoadedT2D->PlatformData->Mips[0].BulkData.Unlock();

			LoadedT2D->UpdateResource();

			UE_LOG(LogTemp, Warning, TEXT("success loadtexture2d from file"));
		}
	}

	return LoadedT2D;
}

TArray<UTexture2D*> ATestActor::GetImages()
{
	TArray<UTexture2D*> Ret;
	for (int32 i = 0; i < Arr.Num(); i++)
	{
		const FString Replace = FString::Printf(TEXT("%03d.png"), i);

		const FString Path = FPaths::ProjectContentDir() + "Image/ " + Replace;

		Ret.Emplace(LoadTexture2D_FromFile(Path));
	}
	return Ret;
}

// Called every frame
void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, FString::Printf(TEXT("number : %d"), number));
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange, FString::Printf(TEXT("thread count : %d"), ThreadCount));

	if(number < Arr.Num() && ThreadCount < (std::thread::hardware_concurrency() * 2) + 1)
	{
		Thread = new FThreadActor(Arr, number, this);
		ThreadCount++;
		number++;
	}
}

int32 ATestActor::Add(int32 a, int32 b)
{
	//UE_LOG(LogTemp, Warning, TEXT("%d"), a+b);
	return a + b;
}

TArray<FString> ATestActor::GetUrl()
{
	FString StringToLoad;
	FFileHelper::LoadFileToString(StringToLoad, *(FPaths::ProjectContentDir()+"/latest.json"));

	TArray<FString> Ret;

	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(StringToLoad);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		TArray<TSharedPtr<FJsonValue>> ScenesArrayField = JsonObject->GetArrayField("scenes");

		for (const auto& i : ScenesArrayField)
		{
			TSharedPtr<FJsonValue> SceneValue = i;
			TSharedPtr<FJsonObject> SceneObject = SceneValue->AsObject();

			TArray<TSharedPtr<FJsonValue>> LayerArrayField = SceneObject->GetArrayField("layers");
			for (const auto& Layer : LayerArrayField)
			{
				TSharedPtr<FJsonValue> LayerField = Layer;
				TSharedPtr<FJsonObject> LayerObject = LayerField->AsObject();

				Ret.Emplace(LayerObject->GetStringField("resizedImg"));
				//Ret.Emplace(LayerObject->GetStringField("originImg"));
			}
		}
		//DataNum = ResizedImgURLArr.Num() - 1;
		UE_LOG(LogTemp, Warning, TEXT("Success GetImgURL"));
		return Ret;
	}
	UE_LOG(LogTemp, Warning, TEXT("Failed get imageURL"));
	return TArray<FString>();
}

FThreadActor::FThreadActor()
{
	Thread = FRunnableThread::Create(this, TEXT("Test Thread"));
	ThreadId = Thread->GetThreadID();
	UE_LOG(LogTemp, Warning, TEXT("Thread Id : %d"), ThreadId);
}

FThreadActor::FThreadActor(const int32 n, ATestActor* Test): number(n), TestActor(Test)
{
	Thread = FRunnableThread::Create(this, TEXT("Test Thread"));
	ThreadId = Thread->GetThreadID();
	UE_LOG(LogTemp, Warning, TEXT("Thread Id : %d"), ThreadId);
}

FThreadActor::FThreadActor(const TArray<FString> URLs, const int32 n, ATestActor* Test) : number(n), TestActor(Test), URL(URLs)
{
	HttpModule = &FHttpModule::Get();
	Thread = FRunnableThread::Create(this, TEXT("Download Thread"));
	ThreadId = Thread->GetThreadID();
	//UE_LOG(LogTemp, Warning, TEXT("Thread Id : %d"), ThreadId);
}

FThreadActor::~FThreadActor()
{
	if(Thread)
	{
		UE_LOG(LogTemp, Error, TEXT("~Thread"));
		Thread->WaitForCompletion();
		Thread->Kill();
		delete Thread;
		Thread = nullptr;
	}
}

bool FThreadActor::Init()
{
	//UE_LOG(LogTemp, Warning, TEXT("Init thread"));
	return true;
}

uint32 FThreadActor::Run()
{
	if(TestActor)
	{
		FScopeLock ScopeLock(&CS);
		DownloadImage();

		//auto f1 = std::async([]
		//	{
		//		FPlatformProcess::Sleep(4.f);
		//		UE_LOG(LogTemp, Warning, TEXT("Async 1"));
		//		return 5;
		//	});

		////UE_LOG(LogTemp, Warning, TEXT("%d"), f1.get());

		//auto f2 = std::async([]
		//	{
		//		FPlatformProcess::Sleep(4.f);
		//		UE_LOG(LogTemp, Warning, TEXT("Async 2"));
		//	});
	}
	return 0;
}

void FThreadActor::Exit()
{
	if(TestActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Thread exit"));
	}

}

void FThreadActor::DownloadImage()
{
	if(TestActor)
	{
		auto RequestComplete = [&](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			FString FileNameStr;
			URL[number].Split("Img_", nullptr, &FileNameStr);
			const FString Replace = FString::Printf(TEXT("%03d.png"), number);

			const FString Path = FPaths::ProjectContentDir() + "Image/" + Replace;

			if (bWasSuccessful)
			{
				if (FFileHelper::SaveArrayToFile(Response->GetContent(), *Path))
				{
					TestActor->ThreadCount--;
					UE_LOG(LogTemp, Warning, TEXT("Successfully downloaded the file to : %s"), *Path);
					delete this;
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Error downloading the file Code : %s"), Response->GetResponseCode());
			}
		};

		const auto Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb("GET");
		Request->SetURL(URL[number]);
		Request->OnProcessRequestComplete().BindLambda(RequestComplete);
		Request->ProcessRequest();
	}
}

