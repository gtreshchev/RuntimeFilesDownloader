// Georgy Treshchev 2023.

#include "FileToMemoryDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Runtime/Launch/Resources/Version.h"


UFileToMemoryDownloader* UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete)
{
	return DownloadFileToMemory(URL, Timeout, ContentType, FOnDownloadProgressNative::CreateLambda([OnProgress](int32 BytesReceived, int32 ContentLength)
	{
		OnProgress.ExecuteIfBound(BytesReceived, ContentLength);
	}), FOnFileToMemoryDownloadCompleteNative::CreateLambda([OnComplete](const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result)
	{
		OnComplete.ExecuteIfBound(DownloadedContent, Result);
	}));
}

UFileToMemoryDownloader* UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());

	Downloader->AddToRoot();

	Downloader->OnDownloadProgressNative = OnProgress;
	Downloader->OnDownloadCompleteNative = OnComplete;
	
	GetContentSize(URL, Timeout, FOnGetDownloadContentLengthNative::CreateWeakLambda(Downloader, [Downloader, URL, Timeout, ContentType](int32 ContentLength)
	{
		Downloader->EstimatedContentLength = ContentLength;
		Downloader->DownloadFileToMemory(URL, Timeout, ContentType);
	}));

	return Downloader;
}

void UFileToMemoryDownloader::BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const
{
	if (OnDownloadCompleteNative.IsBound())
	{
		OnDownloadCompleteNative.Execute(DownloadedContent, Result);
	}

	if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Execute(DownloadedContent, Result);
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
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::InvalidURL);
		RemoveFromRoot();
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest{FHttpModule::Get().CreateRequest()};
#else
	const TSharedRef<IHttpRequest> HttpRequest{FHttpModule::Get().CreateRequest()};
#endif

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	HttpRequest->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	if (!ContentType.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Content-Type"), ContentType);
	}

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFileToMemoryDownloader::OnComplete_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &UBaseFilesDownloader::OnProgress_Internal);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to initiate the download: request processing error"));
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::DownloadFailed);
		RemoveFromRoot();
		return;
	}

	HttpDownloadRequestPtr = HttpRequest;
}

void UFileToMemoryDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	RemoveFromRoot();
	HttpDownloadRequestPtr.Reset();

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to temporary memory"));

		if (!Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to complete the download: the response is not valid"));
		}
		else
		{
			if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to complete the download: the status is '%d', must be within the range of 200-206"), Response->GetResponseCode());
			}
		}

		if (!bWasSuccessful)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to complete the download: was not successful"));
		}

		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::DownloadFailed);
		return;
	}

	const TArray<uint8> ReturnBytes(Response->GetContent().GetData(), Response->GetContentLength());
	BroadcastResult(ReturnBytes, EDownloadToMemoryResult::SuccessDownloading);
}
