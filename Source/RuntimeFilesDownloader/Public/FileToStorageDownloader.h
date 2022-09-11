// Georgy Treshchev 2022.

#pragma once

#include "Http.h"
#include "BaseFilesDownloader.h"
#include "FileToStorageDownloader.generated.h"

/** Possible results from a download request */
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


/** Static delegate broadcast after the download is complete */
DECLARE_DELEGATE_OneParam(FOnFileToStorageDownloadCompleteNative, EDownloadToStorageResult);

/** Dynamic delegate broadcast after the download is complete */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnFileToStorageDownloadComplete, EDownloadToStorageResult, Result);

/**
 * Library for downloading files to storage
 */
UCLASS(BlueprintType, Category = "File To Storage Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToStorageDownloader : public UBaseFilesDownloader
{
	GENERATED_BODY()

	/** Static delegate to track download completion */
	FOnFileToStorageDownloadCompleteNative OnDownloadCompleteNative;

	/** Dynamic delegate to track download completion */
	FOnFileToStorageDownloadComplete OnDownloadComplete;

public:
	/**
	 * Download the file and save it to the device disk. Recommended for Blueprints only
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Storage Downloader|Main", meta = (DisplayName = "Download File To Storage"))
	static UFileToStorageDownloader* BP_DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete);

	/**
	 * Download the file and save it to the device disk. Recommended for C++ only
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 * @note Please consider using this function in C++ project
	 */
	static UFileToStorageDownloader* DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToStorageDownloadCompleteNative& OnComplete);

private:
	/**
	 * Download the file and save it to the device disk. Recommended for C++ only
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout Maximum waiting time in case of zero download progress, sec
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 */
	void DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType);

	/** The path where to save the downloaded file */
	FString FileSavePath;

	/**
	 * Broadcast the download result
	 */
	void BroadcastResult(EDownloadToStorageResult Result) const;

	/**
	 * File downloading finished internal callback
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
