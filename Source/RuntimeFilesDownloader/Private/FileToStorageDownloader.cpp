// Georgy Treshchev 2021.

#include "FileToStorageDownloader.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"


UFileToStorageDownloader* UFileToStorageDownloader::BP_DownloadFileToStorage(
	const FString& URL, const FString& SavePath, float Timeout, const FOnSingleCastDownloadProgress& OnProgress,
	const FOnSingleCastFileToStorageDownloadComplete& OnComplete)
{
	UFileToStorageDownloader* Downloader = NewObject<UFileToStorageDownloader>(StaticClass());
	Downloader->AddToRoot();
	Downloader->OnSingleCastDownloadProgress = OnProgress;
	Downloader->OnSingleCastDownloadComplete = OnComplete;
	Downloader->DownloadFileToStorage(URL, SavePath, Timeout);
	return Downloader;
}

void UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout)
{
	if (URL.IsEmpty())
	{
		BroadcastResult(EDownloadToStorageResult::InvalidURL);
		return;
	}

	if (SavePath.IsEmpty())
	{
		BroadcastResult(EDownloadToStorageResult::InvalidSavePath);
		return;
	}

	if (Timeout < 0) Timeout = 0;

	FileSavePath = SavePath;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->SetTimeout(Timeout);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UFileToStorageDownloader::OnComplete_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Process the request
	HttpRequest->ProcessRequest();

	HttpDownloadRequest = &HttpRequest.Get();
}

void UFileToStorageDownloader::BroadcastResult(EDownloadToStorageResult Result) const
{
	OnSingleCastDownloadComplete.ExecuteIfBound(Result);
	OnDownloadComplete.Broadcast(Result);
}


void UFileToStorageDownloader::OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		BroadcastResult(EDownloadToStorageResult::DownloadFailed);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create save directory if not existent
	FString Path, Filename, Extension;
	FPaths::Split(FileSavePath, Path, Filename, Extension);
	if (!PlatformFile.DirectoryExists(*Path))
	{
		if (!PlatformFile.CreateDirectoryTree(*Path))
		{
			BroadcastResult(EDownloadToStorageResult::DirectoryCreationFailed);
			return;
		}
	}

	// Delete file if it already exists
	if (!FileSavePath.IsEmpty() && FPaths::FileExists(*FileSavePath))
	{
		IFileManager& FileManager = IFileManager::Get();
		FileManager.Delete(*FileSavePath);
	}

	// Open / Create the file
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (FileHandle)
	{
		// Write the file
		FileHandle->Write(Response->GetContent().GetData(), Response->GetContentLength());
		// Close the file
		delete FileHandle;

		BroadcastResult(EDownloadToStorageResult::SuccessDownloading);
	}
	else
	{
		BroadcastResult(EDownloadToStorageResult::SaveFailed);
	}
}
