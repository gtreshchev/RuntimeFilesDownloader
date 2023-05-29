// Georgy Treshchev 2023.

#include "FileToStorageDownloader.h"

#include "RuntimeChunkDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete)
{
	return DownloadFileToStorage(URL, SavePath, Timeout, ContentType, FOnDownloadProgressNative::CreateLambda([OnProgress](int64 BytesReceived, int64 ContentLength, float ProgressRatio)
	{
		OnProgress.ExecuteIfBound(BytesReceived, ContentLength, ProgressRatio);
	}), FOnFileToStorageDownloadCompleteNative::CreateLambda([OnComplete](EDownloadToStorageResult Result)
	{
		OnComplete.ExecuteIfBound(Result);
	}));
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

bool UFileToStorageDownloader::CancelDownload()
{
	if (RuntimeChunkDownloaderPtr.IsValid())
	{
		RuntimeChunkDownloaderPtr->CancelDownload();
		return true;
	}
	return false;
}

void UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided an URL to download the file"));
		BroadcastResult(EDownloadToStorageResult::InvalidURL);
		RemoveFromRoot();
		return;
	}

	if (SavePath.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided a path to save the file"));
		BroadcastResult(EDownloadToStorageResult::InvalidSavePath);
		RemoveFromRoot();
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

	FileSavePath = SavePath;

	RuntimeChunkDownloaderPtr = MakeShared<FRuntimeChunkDownloader>();
	RuntimeChunkDownloaderPtr->DownloadFile(URL, Timeout, ContentType, [this](int64 BytesReceived, int64 ContentLength)
	{
		BroadcastProgress(BytesReceived, ContentLength, static_cast<float>(BytesReceived) / ContentLength);
	}).Next([this](TArray64<uint8> DownloadedContent) mutable
	{
		OnComplete_Internal(MoveTemp(DownloadedContent));
	});
}

void UFileToStorageDownloader::BroadcastResult(EDownloadToStorageResult Result) const
{
	if (OnDownloadCompleteNative.IsBound())
	{
		OnDownloadCompleteNative.Execute(Result);
	}

	if (OnDownloadComplete.IsBound())
	{
		OnDownloadComplete.Execute(Result);
	}

	if (!OnDownloadCompleteNative.IsBound() && !OnDownloadComplete.IsBound())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not bound any delegates to get the result of the download"));
	}
}

void UFileToStorageDownloader::OnComplete_Internal(TArray64<uint8> DownloadedContent)
{
	RemoveFromRoot();

	if (!DownloadedContent.IsValidIndex(0))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to storage"));
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

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (!FileHandle)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while saving the file '%s'"), *FileSavePath);
		BroadcastResult(EDownloadToStorageResult::SaveFailed);
		return;
	}

	if (!FileHandle->Write(DownloadedContent.GetData(), DownloadedContent.Num()))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath);
		delete FileHandle;
		BroadcastResult(EDownloadToStorageResult::SaveFailed);
		return;
	}

	delete FileHandle;
	BroadcastResult(EDownloadToStorageResult::SuccessDownloading);
}
