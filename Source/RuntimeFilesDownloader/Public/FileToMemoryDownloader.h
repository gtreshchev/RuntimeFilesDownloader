// Georgy Treshchev 2024.

#pragma once

#include "BaseFilesDownloader.h"
#include "FileToMemoryDownloader.generated.h"

class UFileToMemoryDownloader;

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
DECLARE_DELEGATE_ThreeParams(FOnFileToMemoryDownloadCompleteNative, const TArray64<uint8>&, EDownloadToMemoryResult, UFileToMemoryDownloader*);

/** Dynamic delegate to track download completion */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnFileToMemoryDownloadComplete, const TArray<uint8>&, DownloadedContent, EDownloadToMemoryResult, Result, UFileToMemoryDownloader*, Downloader);

/** Static delegate to track chunk download completion */
DECLARE_DELEGATE_TwoParams(FOnFileToMemoryChunkDownloadCompleteNative, const TArray64<uint8>&, UFileToMemoryDownloader*);

/** Dynamic delegate to track chunk download completion */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnFileToMemoryChunkDownloadComplete, const TArray<uint8>&, DownloadedContent, UFileToMemoryDownloader*, Downloader);

/** Static delegate to track download completion */
DECLARE_DELEGATE_TwoParams(FOnFileToMemoryAllChunksDownloadCompleteNative, EDownloadToMemoryResult, UFileToMemoryDownloader*);

/** Dynamic delegate to track download completion */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnFileToMemoryAllChunksDownloadComplete, EDownloadToMemoryResult, Result, UFileToMemoryDownloader*, Downloader);

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
	 * @param bForceByPayload If true, download the file regardless of the Content-Length header's presence (useful for servers without support for this header)
	 * @param OnProgress Delegate for download progress updates
	 * @param OnComplete Delegate for broadcasting the completion of the download
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Memory")
	static UFileToMemoryDownloader* DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, bool bForceByPayload, const FOnDownloadProgress& OnProgress, const FOnFileToMemoryDownloadComplete& OnComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param bForceByPayload If true, download the file regardless of the Content-Length header's presence (useful for servers without support for this header)
	 * @param OnProgress Delegate for download progress updates
	 * @param OnComplete Delegate for broadcasting the completion of the download
	 */
	static UFileToMemoryDownloader* DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, bool bForceByPayload, const FOnDownloadProgressNative& OnProgress, const FOnFileToMemoryDownloadCompleteNative& OnComplete);

	/**
	 * Download the file and save it as a byte array in temporary memory (RAM). Continuously broadcasts the download result per chunk
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param ContentType A string to set in the Content-Type header field. Use a MIME type to specify the file type
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes
	 * @param OnProgress Delegate for download progress updates
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
	 * @param OnProgress Delegate for download progress updates
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
	 * @param bForceByPayload If true, download the file regardless of the Content-Length header's presence (useful for servers without support for this header)
	 */
	void DownloadFileToMemory(const FString& URL, float Timeout, const FString& ContentType, bool bForceByPayload);

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
