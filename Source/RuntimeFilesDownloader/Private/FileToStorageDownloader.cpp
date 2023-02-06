// Georgy Treshchev 2023.

#include "FileToStorageDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"


UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete)
{
	UFileToStorageDownloader* Downloader{NewObject<UFileToStorageDownloader>(StaticClass())};

	Downloader->AddToRoot();

	Downloader->OnDownloadProgress = OnProgress;
	Downloader->OnDownloadComplete = OnComplete;

	Downloader->DownloadFileToStorage(URL, SavePath, Timeout, ContentType);

	return Downloader;
}

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToStorageDownloadCompleteNative& OnComplete)
{
	UFileToStorageDownloader* Downloader{NewObject<UFileToStorageDownloader>(StaticClass())};

	Downloader->AddToRoot();

	Downloader->OnDownloadProgressNative = OnProgress;
	Downloader->OnDownloadCompleteNative = OnComplete;

	Downloader->DownloadFileToStorage(URL, SavePath, Timeout, ContentType);

	return Downloader;
}

void UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided an URL to download the file"));
		BroadcastResult(EDownloadToStorageResult::InvalidURL);
		return;
	}

	if (SavePath.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided a path to save the file"));
		BroadcastResult(EDownloadToStorageResult::InvalidSavePath);
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

	FileSavePath = SavePath;

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

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFileToStorageDownloader::OnComplete_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &UBaseFilesDownloader::OnProgress_Internal);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to initiate the download: request processing error"));
		BroadcastResult(EDownloadToStorageResult::DownloadFailed);
		RemoveFromRoot();
	}

	HttpDownloadRequest = &HttpRequest.Get();
}

void UFileToStorageDownloader::BroadcastResult(EDownloadToStorageResult Result) const
{
	if (OnDownloadCompleteNative.IsBound())
	{
		OnDownloadCompleteNative.Execute(Result);
	}
	else if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Execute(Result);
	}
	else
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You did not bind to a delegate to get download result"));
	}
}


void UFileToStorageDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to storage"));

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

		BroadcastResult(EDownloadToStorageResult::DownloadFailed);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create save directory if it does not exist
	{
		FString Path, Filename, Extension;
		FPaths::Split(FileSavePath, Path, Filename, Extension);
		if (!PlatformFile.DirectoryExists(*Path))
		{
			if (!PlatformFile.CreateDirectoryTree(*Path))
			{
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to create a directory '%s' to save the downloaded file"), *Path);
				BroadcastResult(EDownloadToStorageResult::DirectoryCreationFailed);
				return;
			}
		}
	}

	// Delete the file if it already exists
	if (FPaths::FileExists(*FileSavePath))
	{
		IFileManager& FileManager = IFileManager::Get();
		if (!FileManager.Delete(*FileSavePath))
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while deleting the existing file '%s'"), *FileSavePath);
			BroadcastResult(EDownloadToStorageResult::SaveFailed);
			return;
		}
	}

	if (IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath))
	{
		if (!FileHandle->Write(Response->GetContent().GetData(), Response->GetContentLength()))
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath);
			delete FileHandle;
			BroadcastResult(EDownloadToStorageResult::SaveFailed);
			return;
		}

		delete FileHandle;
		BroadcastResult(EDownloadToStorageResult::SuccessDownloading);
	}
	else
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while saving the file '%s'"), *FileSavePath);
		BroadcastResult(EDownloadToStorageResult::SaveFailed);
	}
}
