// Georgy Treshchev 2021.

#pragma once

#include "Http.h"

#include "RuntimeFilesDownloaderLibrary.h"

#include "FileToStorageDownloader.generated.h"

/**
 * Possible results of downloading a file to device storage
 */
UENUM(BlueprintType, Category = "File To Storage Downloader")
enum class EDownloadToStorageResult : uint8
{
	SuccessDownloading UMETA(DisplayName = "Success"),
	DownloadFailed,
	SaveFailed,
	DirectoryCreationFailed,
	InvalidURL,
	InvalidSavePath
};


/**
 * Multi-cast delegate broadcast on download completed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFileToStorageDownloadComplete, EDownloadToStorageResult, Result);

/**
 * Single-cast delegate broadcast on download completed
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSingleCastFileToStorageDownloadComplete, EDownloadToStorageResult, Result);

/**
 * Library for downloading files to storage
 */
UCLASS(BlueprintType, Category = "File To Storage Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToStorageDownloader : public URuntimeFilesDownloaderLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Multi-cast delegate broadcast on download completed
	 */
	UPROPERTY(BlueprintAssignable, Category = "File To Storage Downloader")
	FOnFileToStorageDownloadComplete OnDownloadComplete;

	/**
	 * Single-cast delegate broadcast on download completed
	 */
	FOnSingleCastFileToStorageDownloadComplete OnSingleCastDownloadComplete;

	/**
	 * Download the file and save it to the device disk. Only single-cast delegates are used
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Storage Downloader", meta = (DisplayName = "Download File To Storage"))
	static UFileToStorageDownloader* BP_DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FOnSingleCastDownloadProgress& OnProgress, const FOnSingleCastFileToStorageDownloadComplete& OnComplete);


	/**
	 * Download the file and save it to the device disk. Only multi-cast delegates are used
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 */
	void DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout);
	

private:
	/**
	 * The path where to save the downloaded file
	 */
	FString FileSavePath;

	/**
	 * Broadcast the result both multi-cast and single-cast delegates
	 */
	void BroadcastResult(EDownloadToStorageResult Result) const;

	/**
	 * File downloading finished internal callback
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
