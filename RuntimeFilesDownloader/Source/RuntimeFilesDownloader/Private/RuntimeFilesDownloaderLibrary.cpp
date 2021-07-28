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

bool URuntimeFilesDownloaderLibrary::DownloadFile(const FString& URL, const FString& SavePath, float TimeOut)
{
	if (URL.IsEmpty() || SavePath.IsEmpty() || TimeOut <= 0)
	{
		return false;
	}

	FileURL = URL;
	FileSavePath = SavePath;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(FileURL);
	HttpRequest->SetTimeout(TimeOut);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnReady_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Process the request
	HttpRequest->ProcessRequest();

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
	Request->OnProcessRequestComplete().Unbind();

	if (Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()) && bWasSuccessful)
	{
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
	else
	{
		OnResult.Broadcast(EDownloadResult::DownloadFailed);
	}
}
