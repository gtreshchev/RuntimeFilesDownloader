// Respirant 2020.

#include "RuntimeFilesDownloaderLibrary.h"
#include "RuntimeFilesDownloader.h"

URuntimeFilesDownloaderLibrary::URuntimeFilesDownloaderLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

URuntimeFilesDownloaderLibrary::URuntimeFilesDownloaderLibrary() :
	FileUrl(TEXT(""))
	, FileSavePath(TEXT(""))
{
}

URuntimeFilesDownloaderLibrary::~URuntimeFilesDownloaderLibrary()
{
}

URuntimeFilesDownloaderLibrary* URuntimeFilesDownloaderLibrary::CreateDownloader()
{
	URuntimeFilesDownloaderLibrary* Downloader = NewObject<URuntimeFilesDownloaderLibrary>();
	return Downloader;
}

URuntimeFilesDownloaderLibrary* URuntimeFilesDownloaderLibrary::DownloadFile(const FString& URL, FString SavePath)
{
	FileUrl = URL;
	FileSavePath = SavePath;

	TSharedRef< IHttpRequest, ESPMode::ThreadSafe > HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnReady_Internal);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderLibrary::OnProgress_Internal);

	// Execute the request
	HttpRequest->ProcessRequest();
	AddToRoot();

	return this;
}

void URuntimeFilesDownloaderLibrary::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	FHttpResponsePtr Response = Request->GetResponse();
	if (Response.IsValid())
	{
		int32 FullSize = Response->GetContentLength();
		OnProgress.Broadcast(BytesSent, BytesReceived, FullSize);
	}
}

void URuntimeFilesDownloaderLibrary::OnReady_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	RemoveFromRoot();
	Request->OnProcessRequestComplete().Unbind();

	if (Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
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
				OnResult.Broadcast(DownloadResult::DirectoryCreationFailed);
				return;
			}
		}

		// Open/Create the file
		IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
		if (FileHandle)
		{
			// Write the file
			FileHandle->Write(Response->GetContent().GetData(), Response->GetContentLength());
			// Close the file
			delete FileHandle;

			OnResult.Broadcast(DownloadResult::SuccessDownloading);
		}
		else
		{
			OnResult.Broadcast(DownloadResult::SaveFailed);
		}
	}
	else
	{
		OnResult.Broadcast(DownloadResult::DownloadFailed);
	}
}
