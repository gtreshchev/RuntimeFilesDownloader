// Georgy Treshchev 2021.

#pragma once

#include "Http.h"

#include "RuntimeFilesDownloaderLibrary.generated.h"

/**
 * Multi-cast delegate broadcast on download progress
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiDownloadProgress, const int32, BytesReceived, const int32,
                                             ContentLength);

/**
 * Single-cast delegate broadcast on download progress
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnSingleCastDownloadProgress, const int32, BytesReceived, const int32,
                                   ContentLength);


/**
 * Base class for downloading files. Use inherited classes depending on how you want to download the file
 */
UCLASS()
class RUNTIMEFILESDOWNLOADER_API URuntimeFilesDownloaderLibrary : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Multi-cast delegate broadcast on download progress
	 */
	UPROPERTY(BlueprintAssignable, Category = "Download File To Memory")
	FOnMultiDownloadProgress OnDownloadProgress;

	/**
	 * Single-cast delegate broadcast on download progress
	 */
	FOnSingleCastDownloadProgress OnSingleCastDownloadProgress;

	/**
	 * File downloading progress internal callback
	 */
	void OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);

	/**
	 * Canceling the current download.
	 *
	 * @return Whether the cancellation was successful or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader")
	bool CancelDownload();

	/**
	 * Convert bytes to string
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader")
	static FString BytesToString(const TArray<uint8>& Bytes);

	/**
	 * Convert bytes to texture
	 */
	UFUNCTION(BlueprintCallable, Category = "Runtime Files Downloader")
	static UTexture2D* BytesToTexture(const TArray<uint8>& Bytes);


protected:
	/**
	 * Using Http download request
	 */
	IHttpRequest* HttpDownloadRequest;

	/**
	 * Broadcast the progress both multi-cast and single-cast delegates
	 * @note To get the download percentage, divide the BytesReceived value by the ContentLength
	 */
	void BroadcastProgress(const int32 BytesReceived, const int32 ContentLength);
};
