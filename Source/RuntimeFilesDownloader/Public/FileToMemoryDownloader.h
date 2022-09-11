// Georgy Treshchev 2022.

#pragma once

#include "Http.h"
#include "BaseFilesDownloader.h"
#include "FileToMemoryDownloader.generated.h"

/**
* Possible results from a download request
*/
UENUM(BlueprintType, Category = "File To Memory Downloader")
enum class EDownloadToMemoryResult : uint8
{
	SuccessDownloading UMETA(DisplayName = "Success"),
	DownloadFailed,
	InvalidURL
};

/** Static delegate to track download completion */
DECLARE_DELEGATE_TwoParams(FOnFileToMemoryDownloadCompleteNative, const TArray<uint8>&, EDownloadToMemoryResult);

/** Dynamic delegate to track download completion */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent, EDownloadToMemoryResult, Result);

/**
 * Library for downloading files to memory. Downloads a file into RAM and outputs an array of bytes
 */
UCLASS(BlueprintType, Category = "File To Memory Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToMemoryDownloader : public UBaseFilesDownloader
{
	GENERATED_BODY()

	/** Static delegate to track download completion */
	FOnFileToMemoryDownloadCompleteNative OnDownloadCompleteNative;

	/** Dynamic delegate to track download completion */
	FOnFileToMemoryDownloadComplete OnDownloadComplete;

public:
	/**
	 * Download the file and save it to the physical memory. Recommended for Blueprints only
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Memory Downloader|Main", meta = (DisplayName = "Download File To Memory"))
	static void BP_DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete);
	static void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete);

private:
	/**
	 * Download the file and save it to the physical memory. Recommended for C++ only
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 */
	void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType);

	/**
	 * Broadcast the download result
	 */
	void BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const;

	/**
	 * File downloading finished internal callback
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
