// Georgy Treshchev 2024.

#pragma once

#include "Http.h"
#include "Templates/SharedPointer.h"
#include "Misc/EngineVersionComparison.h"
#include "BaseFilesDownloader.generated.h"

/** Dynamic delegate to track download progress */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnDownloadProgress, int64, BytesReceived, int64, ContentLength, float, ProgressRatio);

/** Static delegate to track download progress */
DECLARE_DELEGATE_ThreeParams(FOnDownloadProgressNative, int64, int64, float);

/** Dynamic delegate to obtain download content length */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGetDownloadContentLength, int64, ContentLength);

/** Static delegate to obtain download content length */
DECLARE_DELEGATE_OneParam(FOnGetDownloadContentLengthNative, int64);

class UTexture2D;

/**
 * Base class for downloading files. It also contains some helper functions
 */
UCLASS(BlueprintType, Category = "Runtime Files Downloader")
class RUNTIMEFILESDOWNLOADER_API UBaseFilesDownloader : public UObject
{
	GENERATED_BODY()

protected:
	/** Static delegate to track download progress */
	FOnDownloadProgressNative OnDownloadProgress;

public:
	UBaseFilesDownloader();

	/**
	 * Canceling the current download
	 *
	 * @return Whether the cancellation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Main")
	virtual bool CancelDownload();

	/**
	 * Get the content length of the file to be downloaded
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param OnComplete Delegate for broadcasting the completion of the download 
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Main")
	static void GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLength& OnComplete);

	/**
	 * Get the content length of the file to be downloaded. Suitable for use in C++
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The maximum time to wait for the download to complete, in seconds. Works only for engine versions >= 4.26
	 * @param OnComplete Delegate for broadcasting the completion of the download 
	 */
	static void GetContentSize(const FString& URL, float Timeout, const FOnGetDownloadContentLengthNative& OnComplete);

	/**
	 * Convert bytes to string
	 *
	 * @param Bytes Byte array to convert to string
	 * @return Converted string, empty on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static FString BytesToString(const TArray<uint8>& Bytes);

	/**
	 * Convert bytes to texture. This is fully engine-based functionality and may not be well optimized
	 *
	 * @param Bytes Byte array to convert to texture
	 * @return Converted texture or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static UTexture2D* BytesToTexture(const TArray<uint8>& Bytes);

	/**
	 * Load a binary file to a dynamic array with two uninitialized bytes at end as padding
	 *
	 * @param FilePath Path to the file to load
	 * @param Result Bytes representation of the loaded file
	 * @return Whether the operation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static bool LoadFileToArray(const FString& FilePath, TArray<uint8>& Result);

	/**
	 * Save a binary array to a file
	 *
	 * @param Bytes Byte array to save to file
	 * @param FilePath Path to the file to save
	 * @return Whether the operation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static bool SaveArrayToFile(const TArray<uint8>& Bytes, const FString& FilePath);

	/**
	 * Load a text file to an FString.
	 * Supports all combination of ANSI/Unicode files and platforms
	 * 
	 * @param Result String representation of the loaded file
	 * @param FilePath Path to the file to load
	 * @return Whether the operation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static bool LoadFileToString(FString& Result, const FString& FilePath);

	/**
	 * Write the string to a file.
	 * Supports all combination of ANSI/Unicode files and platforms
	 *
	 * @param String String to save to file
	 * @param FilePath Path to the file to save
	 * @return Whether the operation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static bool SaveStringToFile(const FString& String, const FString& FilePath);

	/**
	 * Returns true if this file was found, false otherwise
	 *
	 * @param FilePath Path to the file to check
	 * @return Whether the operation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader|Utilities")
	static bool IsFileExist(const FString& FilePath);

protected:
	/**
	 * Broadcast the progress both multi-cast and single-cast delegates
	 */
	void BroadcastProgress(int64 BytesReceived, int64 ContentLength, float ProgressRatio) const;

	/** Internal downloader */
	TSharedPtr<class FRuntimeChunkDownloader> RuntimeChunkDownloaderPtr;
};
