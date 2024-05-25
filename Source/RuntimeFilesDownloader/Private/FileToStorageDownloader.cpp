// Georgy Treshchev 2024.

#include "FileToStorageDownloader.h"

#include "FileToMemoryDownloader.h"
#include "RuntimeChunkDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete)
{
	return DownloadFileToStorage(URL, SavePath, Timeout, ContentType, bForceByPayload, FOnDownloadProgressNative::CreateLambda([OnProgress](int64 BytesReceived, int64 ContentSize, float ProgressRatio)
	{
		OnProgress.ExecuteIfBound(BytesReceived, ContentSize, ProgressRatio);
	}), FOnFileToStorageDownloadCompleteNative::CreateLambda([OnComplete](EDownloadToStorageResult Result, const FString& SavedPath, UFileToStorageDownloader* Downloader)
	{
		OnComplete.ExecuteIfBound(Result, SavedPath, Downloader);
	}));
}

UFileToStorageDownloader* UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload, const FOnDownloadProgressNative& OnProgress, const FOnFileToStorageDownloadCompleteNative& OnComplete)
{
	UFileToStorageDownloader* Downloader = NewObject<UFileToStorageDownloader>(StaticClass());
	Downloader->AddToRoot();
	Downloader->OnDownloadProgress = OnProgress;
	Downloader->OnDownloadComplete = OnComplete;
	Downloader->DownloadFileToStorage(URL, SavePath, Timeout, ContentType, bForceByPayload);
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

void UFileToStorageDownloader::DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, bool bForceByPayload)
{
	if (URL.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided an URL to download the file"));
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidURL, SavePath, this);
		RemoveFromRoot();
		return;
	}

	if (SavePath.IsEmpty())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("You have not provided a path to save the file"));
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidSavePath, SavePath, this);
		RemoveFromRoot();
		return;
	}

	if (Timeout < 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The specified timeout (%f) is less than 0, setting it to 0"), Timeout);
		Timeout = 0;
	}

	FileSavePath = SavePath;

	auto OnProgress = [this](int64 BytesReceived, int64 ContentSize)
	{
		BroadcastProgress(BytesReceived, ContentSize, ContentSize <= 0 ? 0 : static_cast<float>(BytesReceived) / ContentSize);
	};

	auto OnResult = [this](FRuntimeChunkDownloaderResult&& Result) mutable
	{
		OnComplete_Internal(Result.Result, MoveTemp(Result.Data));
	};

	RuntimeChunkDownloaderPtr = MakeShared<FRuntimeChunkDownloader>();

	if (bForceByPayload)
	{
		RuntimeChunkDownloaderPtr->DownloadFileByPayload(URL, Timeout, ContentType, OnProgress).Next(OnResult);
	}
	else
	{
		RuntimeChunkDownloaderPtr->DownloadFile(URL, Timeout, ContentType, TNumericLimits<TArray<uint8>::SizeType>::Max(), OnProgress).Next(OnResult);
	}
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
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::Cancelled, FileSavePath, this);
			break;
		case EDownloadToMemoryResult::DownloadFailed:
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DownloadFailed, FileSavePath, this);
			break;
		case EDownloadToMemoryResult::InvalidURL:
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::InvalidURL, FileSavePath, this);
			break;
		}
		return;
	}

	if (!DownloadedContent.IsValidIndex(0))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("An error occurred while downloading the file to storage"));
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DownloadFailed, FileSavePath, this);
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
				OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::DirectoryCreationFailed, FileSavePath, this);
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
			OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed, FileSavePath, this);
			return;
		}
	}

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileSavePath);
	if (!FileHandle)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while saving the file '%s'"), *FileSavePath);
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed, FileSavePath, this);
		return;
	}

	if (!FileHandle->Write(DownloadedContent.GetData(), DownloadedContent.Num()))
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Something went wrong while writing the response data to the file '%s'"), *FileSavePath);
		delete FileHandle;
		OnDownloadComplete.ExecuteIfBound(EDownloadToStorageResult::SaveFailed, FileSavePath, this);
		return;
	}

	delete FileHandle;
	OnDownloadComplete.ExecuteIfBound(Result == EDownloadToMemoryResult::SucceededByPayload ? EDownloadToStorageResult::SucceededByPayload : EDownloadToStorageResult::Success, FileSavePath, this);
}
