// Respirant 2020.

#pragma once
#include "Http.h"

#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuntimeFilesDownloaderBPLibrary.generated.h"

UENUM(BlueprintType, Category = "RuntimeFilesDownloader")
enum class EDownloadResult : uint8
{
	Success,
	DownloadFailed,
	SaveFailed,
	DirectoryCreationFailed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResult, const EDownloadResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProgress, const int32, BytesSent, const int32, BytesReceived, const int32, ContentLength);

UCLASS()
class RUNTIMEFILESDOWNLOADER_API URuntimeFilesDownloaderBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* Bind to know when the download is complete (even if it fails).
	*/
	UPROPERTY(BlueprintAssignable, Category = "RuntimeFilesDownloader")
		FOnResult OnResult;
	/**
	* Bind to know when the download is complete (even if it fails).
	*/
	UPROPERTY(BlueprintAssignable, Category = "RuntimeFilesDownloader")
		FOnProgress OnProgress;

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

	URuntimeFilesDownloaderBPLibrary();
	~URuntimeFilesDownloaderBPLibrary();


	/**
	* Instantiates a FileDownloader object, starts downloading and saves it when done.
	*
	* @return The FileDownloader object. Bind to it's OnResult event to know when it's done downloading.
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Create Downloader"), Category = "RuntimeFilesDownloader")
		static URuntimeFilesDownloaderBPLibrary* MakeDownloader();

	/**
	* Starts downloading a file and saves it when done. Bind to the OnResult
	* event to know when the download is done (preferrably, before calling this function).
	*
	* @param Url		The file Url to be downloaded.
	* @param SavePath	The absolute path and file name to save the downloaded file.
	* @return Returns itself.
	*/
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Download File"), Category = "RuntimeFilesDownloader")
		URuntimeFilesDownloaderBPLibrary* DownloadFile(const FString & Url, FString SavePath);

private:

	void OnReady(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);
};

