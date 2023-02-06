// Georgy Treshchev 2023.

#include "FileToMemoryDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Runtime/Launch/Resources/Version.h"


void UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());

	Downloader->AddToRoot();

	Downloader->OnDownloadProgress = OnProgress;
	Downloader->OnDownloadComplete = OnComplete;

	Downloader->DownloadFileToMemory(URL, Timeout, ContentType);
}

void UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());

	Downloader->AddToRoot();

	Downloader->OnDownloadProgressNative = OnProgress;
	Downloader->OnDownloadCompleteNative = OnComplete;

	Downloader->DownloadFileToMemory(URL, Timeout, ContentType);
}

void UFileToMemoryDownloader::BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const
{
	if (OnDownloadCompleteNative.IsBound())
	{
		OnDownloadCompleteNative.Execute(DownloadedContent, Result);
	}
	else if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Execute(DownloadedContent, Result);
	}
	else
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You did not bind to a delegate to get download result"));
	}
}

void UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided an URL to download the file"));
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::InvalidURL);
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
	}

	HttpDownloadRequest = &HttpRequest.Get();
}

void UFileToMemoryDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

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

	const TArray<uint8> ReturnBytes{TArray<uint8>(Response->GetContent().GetData(), Response->GetContentLength())};
	BroadcastResult(ReturnBytes, EDownloadToMemoryResult::SuccessDownloading);
}
