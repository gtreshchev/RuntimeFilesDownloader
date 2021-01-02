// Respirant 2020.

#pragma once

#include "Http.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "RuntimeFilesDownloaderLibrary.generated.h"

/**
 * Possible results from a download request.
 */
UENUM(BlueprintType, Category = "RuntimeFilesDownloader")
enum DownloadResult
{
	SuccessDownloading			UMETA(DisplayName = "Success"),
	DownloadFailed				UMETA(DisplayName = "Download failed"),
	SaveFailed					UMETA(DisplayName = "Save failed"),
	DirectoryCreationFailed		UMETA(DisplayName = "Directory creation failed")
};

/**
 * Declare delegate which will be called during the download process
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProgress, const int32, BytesSent, const int32, BytesReceived, const int32, ContentLength);

/**
 * Declare a delegate that will be called on the download result
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResult, TEnumAsByte < DownloadResult >, Result);

UCLASS(BlueprintType, Category = "RuntimeFilesDownloader")
class RUNTIMEFILESDOWNLOADER_API URuntimeFilesDownloaderLibrary : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Bind to know when the download is on progress.
	 */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeFilesDownloader")
		FOnProgress OnProgress;

	/**
	 * Bind to know when the download is complete (even if it fails).
	 */
	UPROPERTY(BlueprintAssignable, Category = "RuntimeFilesDownloader")
		FOnResult OnResult;

	/**
	 * The URL used to start this download.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RuntimeFilesDownloader")
		FString FileUrl;

	/**
	 * The path set to save the downloaded file.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RuntimeFilesDownloader")
		FString FileSavePath;

	/**
	 * Basic class methods
	 */
	URuntimeFilesDownloaderLibrary();
	~URuntimeFilesDownloaderLibrary();

	/**
	 * Instantiates a FileDownloader object, starts downloading and saves it when done.
	 *
	 * @return The RuntimeFilesDownloader object. Bind to it's OnResult event to know when it is in the process of downloading and has been downloaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeFilesDownloader")
		static URuntimeFilesDownloaderLibrary* CreateDownloader();

	/**
	 * Starts downloading a file and saves it when done.
	 *
	 * @param URL		The file Url to be downloaded.
	 * @param SavePath	The absolute path and file name to save the downloaded file.
	 * @return			Returns itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuntimeFilesDownloader")
		URuntimeFilesDownloaderLibrary* DownloadFile(const FString & URL, FString SavePath);

private:
	/**
	 * File downloading progress callback
	 */
	void OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);

	/**
	 * File downloading finished callback
	 */
	void OnReady_Internal(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
