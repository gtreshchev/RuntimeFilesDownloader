// Georgy Treshchev 2023.

#include "FileToStorageDownloader.h"

#include "FileToMemoryDownloader.h"
#include "RuntimeChunkDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete)
{
	return DownloadFileToStorage(URL, SavePath, Timeout, ContentType, FOnDownloadProgressNative::CreateLambda([OnProgress](int64 BytesReceived, int64 ContentSize, float ProgressRatio)
	{
		OnProgress.ExecuteIfBound(BytesReceived, ContentSize, ProgressRatio);
	}), FOnFileToStorageDownloadCompleteNative::CreateLambda([OnComplete](EDownloadToStorageResult Result)
	{
		OnComplete.ExecuteIfBound(Result);
	}));
}

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToStorageDownloadCompleteNative& OnComplete)
{
	UFileToStorageDownloader* Downloader = NewObject<UFileToStorageDownloader>(StaticClass());
	Downloader->AddToRoot();
	Downloader->OnDownloadProgress = OnProgress;
	Downloader->OnDownloadComplete = OnComplete;
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
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidURL);
		RemoveFromRoot();
		return;
	}

	if (SavePath.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided a path to save the file"));
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidSavePath);
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
	RuntimeChunkDownloaderPtr->DownloadFile(URL, Timeout, ContentType, TNumericLimits<TArray<uint8>::SizeType>::Max(), [this](int64 BytesReceived, int64 ContentSize)
	{
		BroadcastProgress(BytesReceived, ContentSize, ContentSize <= 0 ? 0 : static_cast<float>(BytesReceived) / ContentSize);
	}).Next([this](FRuntimeChunkDownloaderResult Result) mutable
	{
		OnComplete_Internal(Result.Result, Result.Data);
	});
}

void UFileToStorageDownloader::OnComplete_Internal(EDownloadToMemoryResult Result, TArray64<uint8> DownloadedContent)
{
	RemoveFromRoot();

	if (Result != EDownloadToMemoryResult::Success && Result != EDownloadToMemoryResult::SucceededByPayload)
	{
		// TODO: redesign in a more elegant way
		switch (Result)
		{
		case EDownloadToMemoryResult::Cancelled:
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::Cancelled);
			break;
		case EDownloadToMemoryResult::DownloadFailed:
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DownloadFailed);
			break;
		case EDownloadToMemoryResult::InvalidURL:
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidURL);
			break;
		}
		return;
	}

	if (!DownloadedContent.IsValidIndex(0))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to storage"));
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DownloadFailed);
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
				OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DirectoryCreationFailed);
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
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed);
			return;
		}
	}

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (!FileHandle)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while saving the file '%s'"), *FileSavePath);
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed);
		return;
	}

	if (!FileHandle->Write(DownloadedContent.GetData(), DownloadedContent.Num()))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath);
		delete FileHandle;
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed);
		return;
	}

	delete FileHandle;
	OnDownloadComplete.ExecuteIfBound(Result == EDownloadToMemoryResult::SucceededByPayload ? EDownloadToStorageResult::SucceededByPayload : EDownloadToStorageResult::Success);
}
