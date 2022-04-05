// Georgy Treshchev 2022.

#include "FileToMemoryDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Runtime/Launch/Resources/Version.h"


void UFileToMemoryDownloader::BP_DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnSingleCastDownloadProgress& OnProgress, const FOnSingleCastFileToMemoryDownloadComplete& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());

	Downloader->AddToRoot();

	Downloader->OnSingleCastDownloadProgress = OnProgress;
	Downloader->OnSingleCastDownloadComplete = OnComplete;

	Downloader->DownloadFileToMemory(URL, Timeout, ContentType);
}

void UFileToMemoryDownloader::BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const
{
	OnSingleCastDownloadComplete.ExecuteIfBound(DownloadedContent, Result);

	if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Broadcast(DownloadedContent, Result);
	}

	if (!OnSingleCastDownloadComplete.IsBound() && !OnDownloadComplete.IsBound())
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

	if (Timeout < 0) Timeout = 0;

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest{FHttpModule::Get().CreateRequest()};
#else
	const TSharedRef<IHttpRequest> HttpRequest {FHttpModule::Get().CreateRequest()};
#endif

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	HttpRequest->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The functionality to set Timeout has been available since version 4.26. Please update the engine version for this support"));
#endif

	if (!ContentType.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Content-Type"), ContentType);
	}

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFileToMemoryDownloader::OnComplete_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Process the request
	HttpRequest->ProcessRequest();

	HttpDownloadRequest = &HttpRequest.Get();
}

void UFileToMemoryDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	HttpDownloadRequest = nullptr;

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::DownloadFailed);

		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to memory"));

		if (!Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Response is not valid"));
		}

		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Status code is not Ok"));
		}

		if (!bWasSuccessful)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Download failed"));
		}

		return;
	}

	const TArray<uint8> ReturnBytes{TArray<uint8>(Response->GetContent().GetData(), Response->GetContentLength())};

	Response.Reset();

	BroadcastResult(ReturnBytes, EDownloadToMemoryResult::SuccessDownloading);

	RemoveFromRoot();
}
