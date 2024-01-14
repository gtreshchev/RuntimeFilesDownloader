// Georgy Treshchev 2024.

#include "BaseFilesDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"
#include "Containers/UnrealString.h"
#include "ImageUtils.h"
#include "RuntimeChunkDownloader.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UBaseFilesDownloader::UBaseFilesDownloader()
{
	FWorldDelegates::OnWorldCleanup.AddWeakLambda(this, [this](UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		if (bSessionEnded)
		{
			CancelDownload();
		}
	});
}

bool UBaseFilesDownloader::CancelDownload()
{
	return true;
}

void UBaseFilesDownloader::GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLength& OnComplete)
{
	GetContentSize(URL, Timeout, FOnGetDownloadContentLengthNative::CreateLambda([OnComplete](int64 ContentSize)
	{
		OnComplete.ExecuteIfBound(ContentSize);
	}));
}

void UBaseFilesDownloader::GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLengthNative& OnComplete)
{
	UBaseFilesDownloader* FileDownloader = NewObject<UBaseFilesDownloader>();
	FileDownloader->AddToRoot();
	FileDownloader->RuntimeChunkDownloaderPtr = MakeShared<FRuntimeChunkDownloader>();
	FileDownloader->RuntimeChunkDownloaderPtr->GetContentSize(URL, Timeout).Next([FileDownloader, OnComplete](int64 ContentSize)
	{
		if (FileDownloader)
		{
			FileDownloader->RemoveFromRoot();
		}
		OnComplete.ExecuteIfBound(ContentSize);
	});
}

FString UBaseFilesDownloader::BytesToString(const TArray<uint8>& Bytes)
{
	const uint8* BytesData = Bytes.GetData();
	FString Result;
	for (int32 Count = Bytes.Num(); Count > 0; --Count)
	{
		Result += static_cast<TCHAR>(*BytesData);

		++BytesData;
	}
	return Result;
}

UTexture2D* UBaseFilesDownloader::BytesToTexture(const TArray<uint8>& Bytes)
{
	return FImageUtils::ImportBufferAsTexture2D(Bytes);
}

bool UBaseFilesDownloader::LoadFileToArray(const FString& Filename, TArray<uint8>& Result)
{
	return FFileHelper::LoadFileToArray(Result, *Filename);
}

bool UBaseFilesDownloader::SaveArrayToFile(const TArray<uint8>& Bytes, const FString& Filename)
{
	return FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

bool UBaseFilesDownloader::LoadFileToString(FString& Result, const FString& Filename)
{
	return FFileHelper::LoadFileToString(Result, *Filename);
}

bool UBaseFilesDownloader::SaveStringToFile(const FString& String, const FString& Filename)
{
	return FFileHelper::SaveStringToFile(String, *Filename);
}

bool UBaseFilesDownloader::IsFileExist(const FString& FilePath)
{
	return FPaths::FileExists(FilePath);
}

void UBaseFilesDownloader::BroadcastProgress(int64 BytesReceived, int64 ContentLength, float ProgressRatio) const
{
	if (OnDownloadProgress.IsBound())
	{
		OnDownloadProgress.Execute(BytesReceived, ContentLength, ProgressRatio);
	}
}
