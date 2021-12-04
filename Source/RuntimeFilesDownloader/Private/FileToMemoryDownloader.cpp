// Georgy Treshchev 2021.

#include "FileToMemoryDownloader.h"


void UFileToMemoryDownloader::BP_DownloadFileToMemory(const FString& URL, float Timeout,
                                                      const FOnSingleCastDownloadProgress& OnProgress,
                                                      const FOnSingleCastFileToMemoryDownloadComplete& OnComplete)
{
	UFileToMemoryDownloader* Downloader = NewObject<UFileToMemoryDownloader>(StaticClass());
	Downloader->AddToRoot();
	Downloader->OnSingleCastDownloadProgress = OnProgress;
	Downloader->OnSingleCastDownloadComplete = OnComplete;
	Downloader->DownloadFileToMemory(URL, Timeout);
}

void UFileToMemoryDownloader::BroadcastResult(const TArray<uint8>& DownloadedContent,
                                              EDownloadToMemoryResult Result) const
{
	OnSingleCastDownloadComplete.ExecuteIfBound(DownloadedContent, Result);
	OnDownloadComplete.Broadcast(DownloadedContent, Result);
}

void UFileToMemoryDownloader::DownloadFileToMemory(const FString& URL, float Timeout)
{
	if (URL.IsEmpty())
	{
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::InvalidURL);
		return;
	}

	if (Timeout < 0) Timeout = 0;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->SetTimeout(Timeout);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFileToMemoryDownloader::OnComplete_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Process the request
	HttpRequest->ProcessRequest();

	HttpDownloadRequest = &HttpRequest.Get();
}

void UFileToMemoryDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response,
                                                  bool bWasSuccessful)
{
	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		BroadcastResult(TArray<uint8>(), EDownloadToMemoryResult::DownloadFailed);
		return;
	}

	const TArray<uint8>& ReturnBytes = TArray<uint8>(Response->GetContent().GetData(), Response->GetContentLength());

	BroadcastResult(ReturnBytes, EDownloadToMemoryResult::SuccessDownloading);
}
