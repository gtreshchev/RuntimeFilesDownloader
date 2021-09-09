// Georgy Treshchev 2021.

#include "RuntimeFilesDownloaderLibrary.h"

#include "Containers/UnrealString.h"
#include "ImageUtils.h"

bool URuntimeFilesDownloaderLibrary::CancelDownload()
{
	if (!HttpDownloadRequest) return false;

	if (HttpDownloadRequest->GetStatus() != EHttpRequestStatus::Processing) return false;

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

void URuntimeFilesDownloaderLibrary::BroadcastProgress(const int32 BytesReceived, const int32 ContentLength)
{
	OnSingleCastDownloadProgress.ExecuteIfBound(BytesReceived, ContentLength);

	OnDownloadProgress.Broadcast(BytesReceived, ContentLength);
}

void URuntimeFilesDownloaderLibrary::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	const FHttpResponsePtr Response = Request->GetResponse();
	if (Response.IsValid())
	{
		const int32 FullSize = Response->GetContentLength();
		BroadcastProgress(BytesReceived, FullSize);
	}
}
