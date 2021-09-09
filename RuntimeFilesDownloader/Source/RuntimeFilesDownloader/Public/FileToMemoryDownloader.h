// Georgy Treshchev 2021.

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
 * Multi-cast delegate broadcast on download completed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent,
                                             EDownloadToMemoryResult, Result);

/**
 * Single-cast delegate broadcast on download completed
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnSingleCastFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent,
                                   EDownloadToMemoryResult, Result);

/**
 * Library for downloading files to memory. Downloads a file into RAM and outputs an array of bytes.
 */
UCLASS(BlueprintType, Category = "File To Memory Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToMemoryDownloader : public URuntimeFilesDownloaderLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Multi-cast delegate broadcast on download completed
	 */
	UPROPERTY(BlueprintAssignable, Category = "File To Disk Downloader")
	FOnFileToMemoryDownloadComplete OnDownloadComplete;

	/**
	 * Single-cast delegate broadcast on download completed
	 */
	FOnSingleCastFileToMemoryDownloadComplete OnSingleCastDownloadComplete;

	/**
	 * Download the file and save it to the physical memory (RAM). Only multi-cast delegates are used
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 * @param OnProgress Delegate broadcast on download progress
	 * @param OnComplete Delegate broadcast on download complete
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Memory Downloader",
		meta = (DisplayName = "Download File To Memory"))
	static void BP_DownloadFileToMemory(const FString& URL, float Timeout,
	                                    const FOnSingleCastDownloadProgress& OnProgress,
	                                    const FOnSingleCastFileToMemoryDownloadComplete& OnComplete);

	/**
	 * Download the file and save it to the physical memory (RAM). Only single-cast delegates are used
	 *
	 * @param URL The file URL to be downloaded
	 * @param Timeout Maximum waiting time in case of zero download progress, in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Memory Downloader",
		meta = (DisplayName = "Download File To Memory"))
	void DownloadFileToMemory(const FString& URL, float Timeout = 5);

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
