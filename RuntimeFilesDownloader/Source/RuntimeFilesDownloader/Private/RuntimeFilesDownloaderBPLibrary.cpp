// Respirant 2020.

#include "RuntimeFilesDownloaderBPLibrary.h"
#include "RuntimeFilesDownloader.h"

URuntimeFilesDownloaderBPLibrary::URuntimeFilesDownloaderBPLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

URuntimeFilesDownloaderBPLibrary::URuntimeFilesDownloaderBPLibrary() :
	FileUrl(TEXT(""))
	, FileSavePath(TEXT(""))
{
}

URuntimeFilesDownloaderBPLibrary::~URuntimeFilesDownloaderBPLibrary()
{
}

URuntimeFilesDownloaderBPLibrary* URuntimeFilesDownloaderBPLibrary::CreateDownloader()
{
	URuntimeFilesDownloaderBPLibrary* Downloader = NewObject<URuntimeFilesDownloaderBPLibrary>();
	return Downloader;
}

URuntimeFilesDownloaderBPLibrary* URuntimeFilesDownloaderBPLibrary::DownloadFile(const FString& URL, FString SavePath)
{
	FileUrl = URL;
	FileSavePath = SavePath;

	TSharedRef< IHttpRequest, ESPMode::ThreadSafe > HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(URL);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &URuntimeFilesDownloaderBPLibrary::OnReady);
	HttpRequest->OnRequestProgress().BindUObject(this, &URuntimeFilesDownloaderBPLibrary::OnProgress_Internal);

	// Execute the request
	HttpRequest->ProcessRequest();
	AddToRoot();

	return this;
}

void URuntimeFilesDownloaderBPLibrary::OnReady_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
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
				OnResult.Broadcast(EDownloadResult::DirectoryCreationFailed);
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

			OnResult.Broadcast(EDownloadResult::Success);
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

void URuntimeFilesDownloaderBPLibrary::OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
{
	int32 FullSize = Request->GetContentLength();
	OnProgress.Broadcast(BytesSent, BytesReceived, FullSize);
}

