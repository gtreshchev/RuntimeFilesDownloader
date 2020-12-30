// Respirant 2020.

#pragma once
#include "Http.h"

#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

#include "GenericPlatform/GenericPlatformFile.h"

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
	* Bind to know when the download is on progress.
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
	UFUNCTION(BlueprintCallable, Category = "RuntimeFilesDownloader")
		static URuntimeFilesDownloaderBPLibrary* CreateDownloader();

	/**
	* Starts downloading a file and saves it when done.
	*
	* @param Url		The file Url to be downloaded.
	* @param SavePath	The absolute path and file name to save the downloaded file.
	* @return Returns itself.
	*/
	UFUNCTION(BlueprintCallable, Category = "RuntimeFilesDownloader")
		URuntimeFilesDownloaderBPLibrary* DownloadFile(const FString & URL, FString SavePath);

private:

	void OnReady(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnProgress_Internal(FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived);
};

