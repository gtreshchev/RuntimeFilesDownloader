// Georgy Treshchev 2022.

#pragma once

#include "Http.h"

#include "RuntimeFilesDownloaderLibrary.h"

#include "FileToStorageDownloader.generated.h"

/**
 * Possible results from a download request
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
	UPROPERTY(BlueprintAssignable, Category = "File To Storage Downloader|Delegates")
	FOnFileToStorageDownloadComplete OnDownloadComplete;

private:
	/**
	 * Single-cast delegate broadcast on download completed
	 */
	FOnSingleCastFileToStorageDownloadComplete OnSingleCastDownloadComplete;

public:
	/**
	 * Download the file and save it to the device disk. Call only if you want to use multi-cast delegates
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Storage Downloader|Main", meta = (DisplayName = "Download File To Storage"))
	static UFileToStorageDownloader* BP_DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnSingleCastDownloadProgress& OnProgress, const FOnSingleCastFileToStorageDownloadComplete& OnComplete);

	/**
	 * Download the file and save it to the device disk. It is recommended to call only if you want to use single-cast delegates
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 */
	void DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType);


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
