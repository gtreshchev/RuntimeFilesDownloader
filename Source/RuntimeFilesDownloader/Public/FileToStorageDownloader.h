// Georgy Treshchev 2023.

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
 * Library for downloading files to storage. Downloads a file and saves it to permanent storage
 */
UCLASS(BlueprintType, Category = "File To Storage Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToStorageDownloader : public UBaseFilesDownloader
{
	GENERATED_BODY()

	/** Static delegate for monitoring the completion of the download */
	FOnFileToStorageDownloadCompleteNative OnDownloadCompleteNative;

	/** Dynamic delegate for monitoring the completion of the download */
	FOnFileToStorageDownloadComplete OnDownloadComplete;

public:
	/**
	 * Download the file and save it to storage
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnComplete A delegate that will be called to broadcast that the download is complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Storage Downloader|Main")
	static UFileToStorageDownloader* DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToStorageDownloadComplete& OnComplete);

	/**
	 * Download the file and save it to storage. Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnComplete A delegate that will be called to broadcast that the download is complete
	 */
	static UFileToStorageDownloader* DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToStorageDownloadCompleteNative& OnComplete);

private:
	/**
	 * Download the file and save it to the device disk
	 *
	 * @param URL The file URL to be downloaded
	 * @param SavePath The absolute path and file name to save the downloaded file
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 */
	void DownloadFileToStorage(const FString& URL, const FString& SavePath, float Timeout, const FString& ContentType);

	/** The destination path to save the downloaded file */
	FString FileSavePath;

	/**
	 * Broadcast the download result
	 */
	void BroadcastResult(EDownloadToStorageResult Result) const;

	/**
	 * Internal callback for when file downloading has finished
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
