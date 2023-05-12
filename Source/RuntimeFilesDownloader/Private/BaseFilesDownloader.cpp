// Georgy Treshchev 2023.

#include "BaseFilesDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Containers/UnrealString.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Launch/Resources/Version.h"
#include "Misc/Paths.h"

bool UBaseFilesDownloader::CancelDownload()
{
	if (!HttpDownloadRequestPtr.IsValid())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to cancel download due to missing request"));
		return false;
	}

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpDownloadRequestPtr.Pin();
#else
	TSharedPtr<IHttpRequest> HttpRequest = HttpDownloadRequestPtr.Pin();
#endif

	if (HttpRequest->GetStatus() != EHttpRequestStatus::Processing)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to cancel download because download is not in progress"));
		return false;
	}

	HttpRequest->CancelRequest();
	HttpDownloadRequestPtr.Reset();
	RemoveFromRoot();
	return true;
}

void UBaseFilesDownloader::GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLength& OnComplete)
{
	GetContentSize(URL, Timeout, FOnGetDownloadContentLengthNative::CreateLambda([OnComplete](int32 ContentLength)
	{
		OnComplete.ExecuteIfBound(ContentLength);
	}));
}

void UBaseFilesDownloader::GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLengthNative& OnComplete)
{
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest{FHttpModule::Get().CreateRequest()};
#else
	const TSharedRef<IHttpRequest> HttpRequest{FHttpModule::Get().CreateRequest()};
#endif

	HttpRequest->SetVerb("HEAD");
	HttpRequest->SetURL(URL);

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	HttpRequest->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete](const FHttpRequestPtr& HttpRequest, const FHttpResponsePtr& HttpResponse, const bool bSucceeded)
	{
		if (!HttpResponse.IsValid() || HttpResponse->GetContentLength() <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get content size: content length is 0"));
			OnComplete.ExecuteIfBound(0);
			return;
		}

		const int32 ContentLength = HttpResponse->GetContentLength();
		OnComplete.ExecuteIfBound(ContentLength);
	});

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get content size: request processing error"));
		OnComplete.ExecuteIfBound(0);
	}
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

void UBaseFilesDownloader::BroadcastProgress(int32 BytesReceived, int32 ContentLength) const
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

	int32 ContentLength = Response->GetContentLength();
	if (ContentLength <= 0)
	{
		ContentLength = EstimatedContentLength;
	}

	BroadcastProgress(BytesReceived, ContentLength);
}
