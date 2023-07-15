// Georgy Treshchev 2023.

#pragma once

#include "BaseFilesDownloader.h"
#include "FileToMemoryDownloader.generated.h"

/**
* Possible results from a download request
*/
UENUM(BlueprintType, Category = "File To Memory Downloader")
enum class EDownloadToMemoryResult : uint8
{
	Success,
	/** Downloaded successfully, but there was no Content-Length header in the response and thus downloaded by payload */
	SucceededByPayload,
	Cancelled,
	DownloadFailed,
	InvalidURL
};

/** Static delegate to track download completion */
DECLARE_DELEGATE_TwoParams(FOnFileToMemoryDownloadCompleteNative, const TArray64<uint8>&, EDownloadToMemoryResult);

/** Dynamic delegate to track download completion */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent, EDownloadToMemoryResult, Result);

/** Static delegate to track chunk download completion */
DECLARE_DELEGATE_OneParam(FOnFileToMemoryChunkDownloadCompleteNative, const TArray64<uint8>&);

/** Dynamic delegate to track chunk download completion */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnFileToMemoryChunkDownloadComplete, const TArray<uint8>&, DownloadedContent);

/** Static delegate to track download completion */
DECLARE_DELEGATE_OneParam(FOnFileToMemoryAllChunksDownloadCompleteNative, EDownloadToMemoryResult);

/** Dynamic delegate to track download completion */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnFileToMemoryAllChunksDownloadComplete, EDownloadToMemoryResult, Result);

/**
 * Downloads a file into temporary memory (RAM) and outputs a byte array
 */
UCLASS(BlueprintType, Category = "Runtime Files Downloader|Memory")
class RUNTIMEFILESDOWNLOADER_API UFileToMemoryDownloader : public UBaseFilesDownloader
{
	GENERATED_BODY()

protected:
	/** Static delegate for monitoring the completion of the download */
	FOnFileToMemoryDownloadCompleteNative OnDownloadComplete;

	/** Static delegate for monitoring the completion of the chunk download */
	FOnFileToMemoryChunkDownloadCompleteNative OnChunkDownloadComplete;

	/** Static delegate for monitoring the full completion of the chunks download */
	FOnFileToMemoryAllChunksDownloadCompleteNative OnAllChunksDownloadComplete;

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
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Memory")
	static UFileToMemoryDownloader* DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnComplete A delegate that will be called to broadcast that the download is complete
	 */
	static UFileToMemoryDownloader* DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Continuously broadcasts the download result per chunk
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnChunkDownloadComplete Delegate for broadcasting the completion of the download. Will be called for each chunk
	 * @param OnAllChunksDownloadComplete Delegate for broadcasting the completion of the download of all chunks
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Memory")
	static UFileToMemoryDownloader* DownloadFileToMemoryPerChunk(const FString& URL, float Timeout, const FString& ContentType, int32 MaxChunkSize, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryChunkDownloadComplete& OnChunkDownloadComplete, const FOnFileToMemoryAllChunksDownloadComplete& OnAllChunksDownloadComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Continuously broadcasts the download result per chunk. Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 * @param OnProgress A delegate that will be called to broadcast the download progress
	 * @param OnChunkDownloadComplete Delegate for broadcasting the completion of the download. Will be called for each chunk
	 * @param OnAllChunksDownloadComplete Delegate for broadcasting the completion of the download of all chunks
	 */
	static UFileToMemoryDownloader* DownloadFileToMemoryPerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryChunkDownloadCompleteNative& OnChunkDownloadComplete, const FOnFileToMemoryAllChunksDownloadCompleteNative& OnAllChunksDownloadComplete);

	//~ Begin UBaseFilesDownloader Interface
	virtual bool CancelDownload() override;
	//~ End UBaseFilesDownloader Interface

protected:
	/**
	 * Download the file and save it as a byte array in temporary memory (RAM)
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 */
	void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Continuously broadcasts the download result per chunk. Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 */
	void DownloadFileToMemoryPerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize);
};
