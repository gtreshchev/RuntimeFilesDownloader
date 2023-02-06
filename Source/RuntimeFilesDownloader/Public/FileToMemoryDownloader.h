// Georgy Treshchev 2023.

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
 * Library for downloading files to memory. Downloads a file into temporary memory (RAM) and outputs a byte array
 */
UCLASS(BlueprintType, Category = "File To Memory Downloader")
class RUNTIMEFILESDOWNLOADER_API UFileToMemoryDownloader : public UBaseFilesDownloader
{
	GENERATED_BODY()

	/** Static delegate for monitoring the completion of the download */
	FOnFileToMemoryDownloadCompleteNative OnDownloadCompleteNative;

	/** Dynamic delegate for monitoring the completion of the download */
	FOnFileToMemoryDownloadComplete OnDownloadComplete;

public:
	/**
	 * Download the file and save it as a byte array in temporary memory (RAM)
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnComplete Delegate for broadcasting the completion of the download
	 */
	UFUNCTION(BlueprintCallable, Category = "File To Memory Downloader|Main")
	static void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnComplete A delegate that will be called to broadcast that the download is complete
	 */
	static void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete);

private:
	/**
	 * Download the file and save it to the physical memory (RAM)
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 */
	void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType);

	/**
	 * Broadcast the download result
	 */
	void BroadcastResult(const TArray<uint8>& DownloadedContent, EDownloadToMemoryResult Result) const;

	/**
	 * Internal callback for when file downloading has finished
	 */
	void OnComplete_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
