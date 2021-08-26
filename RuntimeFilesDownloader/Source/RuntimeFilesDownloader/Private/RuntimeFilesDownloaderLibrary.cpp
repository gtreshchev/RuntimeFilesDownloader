// Georgy Treshchev 2021.

#include "RuntimeFilesDownloaderLibrary.h"

#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

URuntimeFilesDownloaderLibrary* URuntimeFilesDownloaderLibrary::CreateDownloader()
{
	URuntimeFilesDownloaderLibrary* Downloader = NewObject<URuntimeFilesDownloaderLibrary>();
	Downloader->AddToRoot();
	return Downloader;
}

void URuntimeFilesDownloaderLibrary::DownloadFile(const FString& URL, const FString& SavePath, float Timeout)
{
	if (URL.IsEmpty())
	{
		OnResult.Broadcast(EDownloadResult::InvalidURL);
		return;
	}

	if (SavePath.IsEmpty())
	{
		OnResult.Broadcast(EDownloadResult::InvalidSavePath);
		return;
	}

	if (Timeout < 0) Timeout = 0;

	FileSavePath = SavePath;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->SetTimeout(Timeout);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnReady_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Process the request
	HttpRequest->ProcessRequest();

	HttpDownloadRequest = &HttpRequest.Get();
}

bool URuntimeFilesDownloaderLibrary::CancelDownload()
{
	if (!HttpDownloadRequest) return false;

	if (HttpDownloadRequest->GetStatus() != EHttpRequestStatus::Processing) return false;

	HttpDownloadRequest->CancelRequest();

	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	return true;
}

void URuntimeFilesDownloaderLibrary::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	const FHttpResponsePtr Response = Request->GetResponse();
	if (Response.IsValid())
	{
		const int32 FullSize = Response->GetContentLength();
		OnProgress.Broadcast(BytesSent, BytesReceived, FullSize);
	}
}

void URuntimeFilesDownloaderLibrary::OnReady_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response,
                                                      bool bWasSuccessful)
{
	RemoveFromRoot();

	HttpDownloadRequest = nullptr;

	if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()) || !bWasSuccessful)
	{
		OnResult.Broadcast(EDownloadResult::DownloadFailed);
		return;
	}

	// Save file
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Create save directory if not existent
	FString Path, Filename, Extension;
	FPaths::Split(FileSavePath, Path, Filename, Extension);
	if (!PlatformFile.DirectoryExists(*Path))
	{
		if (!PlatformFile.CreateDirectoryTree(*Path))
		{
			OnResult.Broadcast(EDownloadResult::DirectoryCreationFailed);
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

		OnResult.Broadcast(EDownloadResult::SuccessDownloading);
	}
	else
	{
		OnResult.Broadcast(EDownloadResult::SaveFailed);
	}
}
