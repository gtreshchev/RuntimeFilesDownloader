// Georgy Treshchev 2022.

#include "RuntimeFilesDownloaderLibrary.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Containers/UnrealString.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

bool URuntimeFilesDownloaderLibrary::CancelDownload()
{
	if (!HttpDownloadRequest)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to cancel download due to missing request"));
		return false;
	}

	if (HttpDownloadRequest->GetStatus() != EHttpRequestStatus::Processing)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to cancel download because download is not in progress"));
		return false;
	}

	HttpDownloadRequest->CancelRequest();

	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	return true;
}

FString URuntimeFilesDownloaderLibrary::BytesToString(const TArray<uint8>& Bytes)
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

UTexture2D* URuntimeFilesDownloaderLibrary::BytesToTexture(const TArray<uint8>& Bytes)
{
	return FImageUtils::ImportBufferAsTexture2D(Bytes);
}

bool URuntimeFilesDownloaderLibrary::LoadFileToArray(TArray<uint8>& Result, const FString& Filename)
{
	return FFileHelper::LoadFileToArray(Result, *Filename);
}

bool URuntimeFilesDownloaderLibrary::SaveArrayToFile(const TArray<uint8>& Bytes, const FString& Filename)
{
	return FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

bool URuntimeFilesDownloaderLibrary::LoadFileToString(FString& Result, const FString& Filename)
{
	return FFileHelper::LoadFileToString(Result, *Filename);
}

bool URuntimeFilesDownloaderLibrary::SaveStringToFile(const FString& String, const FString& Filename)
{
	return FFileHelper::SaveStringToFile(String, *Filename);
}

void URuntimeFilesDownloaderLibrary::BroadcastProgress(const int32 BytesReceived, const int32 ContentLength) const
{
	OnSingleCastDownloadProgress.ExecuteIfBound(BytesReceived, ContentLength);

	if (OnDownloadProgress.IsBound())
	{
		OnDownloadProgress.Broadcast(BytesReceived, ContentLength);
	}
}

void URuntimeFilesDownloaderLibrary::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived) const
{
	const FHttpResponsePtr Response{Request->GetResponse()};

	if (!Response.IsValid())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to report download progress because response is invalid"));
		return;
	}

	const int32 FullSize{Response->GetContentLength()};
	BroadcastProgress(BytesReceived, FullSize);
}
