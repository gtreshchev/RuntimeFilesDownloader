// Georgy Treshchev 2023.

#include "BaseFilesDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Containers/UnrealString.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

bool UBaseFilesDownloader::CancelDownload()
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

void UBaseFilesDownloader::BroadcastProgress(const int32 BytesReceived, const int32 ContentLength) const
{
	if (OnDownloadProgressNative.IsBound())
	{
		OnDownloadProgressNative.Execute(BytesReceived, ContentLength);
	}

	if (OnDownloadProgress.IsBound())
	{
		OnDownloadProgress.Execute(BytesReceived, ContentLength);
	}
}

void UBaseFilesDownloader::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived) const
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
