// Georgy Treshchev 2022.

#pragma once

#include "Http.h"

#include "RuntimeFilesDownloaderLibrary.h"

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

/**
 * Multi-cast delegate to track download completion
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent, EDownloadToMemoryResult, Result);

/**
 * Single-cast delegate to track download completion
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnSingleCastFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent, EDownloadToMemoryResult, Result);

/**
 * Library for downloading files to memory. Downloads a file into RAM and outputs an array of bytes
 */
UCLASS(BlueprintType, Category = "File To Memory Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToMemoryDownloader : public URuntimeFilesDownloaderLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Multi-cast delegate to track download completion
	 */
	UPROPERTY(BlueprintAssignable, Category = "File To Memory Downloader|Delegates")
	FOnFileToMemoryDownloadComplete OnDownloadComplete;

	/**
	 * Single-cast delegate to track download completion
	 */
	FOnSingleCastFileToMemoryDownloadComplete OnSingleCastDownloadComplete;

	/**
	 * Download the file and save it to the physical memory (RAM). Call only if you want to use multi-cast delegates
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Memory Downloader|Main", meta = (DisplayName = "Download File To Memory"))
	static void BP_DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnSingleCastDownloadProgress& OnProgress, const FOnSingleCastFileToMemoryDownloadComplete& OnComplete);

	/**
	 * Download the file and save it to the physical memory (RAM). It is recommended to call only if you want to use single-cast delegates
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 * @param ContentType The specified string will be set to the header in the Content-Type field. Enter MIME to specify the download file type
	 */
	void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType);

private:
	/**
	 * Broadcast the result both multi-cast and single-cast delegates
	 */
	void BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const;

	/**
	 * File downloading finished internal callback
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
