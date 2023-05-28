// Georgy Treshchev 2023.

#include "FileToMemoryDownloader.h"
#include "RuntimeChunkDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"
#include "Runtime/Launch/Resources/Version.h"

UFileToMemoryDownloader* UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete)
{
	return DownloadFileToMemory(URL, Timeout, ContentType, FOnDownloadProgressNative::CreateLambda([OnProgress](int64 BytesReceived, int64 ContentLength, float Progress)
	{
		OnProgress.ExecuteIfBound(BytesReceived, ContentLength, Progress);
	}), FOnFileToMemoryDownloadCompleteNative::CreateLambda([OnComplete](const TArray64<uint8>& DownloadedContent, EDownloadToMemoryResult Result)
	{
		if (DownloadedContent.Num() > TNumericLimits<int32>::Max())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("The size of the downloaded content exceeds the maximum limit for an int32 array. Maximum length: %d, Retrieved length: %lld\nA standard byte array can hold a maximum of 2 GB of data. If you need to download more than 2 GB of data into memory, consider using the C++ native equivalent instead of the Blueprint dynamic delegate"), TNumericLimits<int32>::Max(), DownloadedContent.Num());
			OnComplete.ExecuteIfBound(TArray<uint8>(), EDownloadToMemoryResult::DownloadFailed);
			return;
		}
		OnComplete.ExecuteIfBound(TArray<uint8>(DownloadedContent), Result);
	}));
}

UFileToMemoryDownloader* UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());
	Downloader->AddToRoot();
	Downloader->OnDownloadProgressNative = OnProgress;
	Downloader->OnDownloadCompleteNative = OnComplete;
	Downloader->DownloadFileToMemory(URL, Timeout, ContentType);
	return Downloader;
}

bool UFileToMemoryDownloader::CancelDownload()
{
	if (RuntimeChunkDownloaderPtr.IsValid())
	{
		RuntimeChunkDownloaderPtr->CancelDownload();
		return true;
	}
	return false;
}

void UFileToMemoryDownloader::BroadcastResult(const TArray64<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const
{
	if (OnDownloadCompleteNative.IsBound())
	{
		OnDownloadCompleteNative.Execute(DownloadedContent, Result);
	}

	if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Execute(TArray<uint8>(DownloadedContent), Result);
	}

	if (!OnDownloadCompleteNative.IsBound() && !OnDownloadComplete.IsBound())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not bound any delegates to get the result of the download"));
	}
}

void UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided an URL to download the file"));
		BroadcastResult(TArray64<uint8>(), EDownloadToMemoryResult::InvalidURL);
		RemoveFromRoot();
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

	RuntimeChunkDownloaderPtr = MakeShared<FRuntimeChunkDownloader>();
	RuntimeChunkDownloaderPtr->DownloadFile(URL, Timeout, ContentType, [this](int64 BytesReceived, int64 ContentLength)
	{
		BroadcastProgress(BytesReceived, ContentLength, static_cast<float>(BytesReceived) / ContentLength);
	}).Next([this](TArray64<uint8> DownloadedContent) mutable
	{
		OnComplete_Internal(MoveTemp(DownloadedContent));
	});
}

void UFileToMemoryDownloader::OnComplete_Internal(TArray64<uint8> DownloadedContent)
{
	RemoveFromRoot();
	if (!DownloadedContent.IsValidIndex(0))
	{
		BroadcastResult(TArray64<uint8>(), EDownloadToMemoryResult::DownloadFailed);
		return;
	}
	BroadcastResult(DownloadedContent, EDownloadToMemoryResult::SuccessDownloading);
}
