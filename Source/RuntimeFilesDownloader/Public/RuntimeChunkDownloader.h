// Georgy Treshchev 2023.

#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Templates/SharedPointer.h"
#include "Async/Future.h"

#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0)
template <typename InIntType>
struct TIntVector2
{
	using IntType = InIntType;
	static_assert(std::is_integral_v<IntType>, "Only an integer types are supported.");

	union
	{
		struct
		{
			IntType X, Y;
		};
	};

	TIntVector2() = default;

	TIntVector2(IntType InX, IntType InY)
		: X(InX)
		, Y(InY)
	{
	}

	explicit TIntVector2(IntType InValue)
		: X(InValue)
		, Y(InValue)
	{
	}

	bool operator==(const TIntVector2& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const TIntVector2& Other) const
	{
		return X != Other.X || Y != Other.Y;
	}
};
using FInt64Vector2 = TIntVector2<int64>;
#endif

/**
 * A class that handles downloading data by chunks from URLs
 * This class is designed to handle large files beyond the limit supported by a TArray<uint8> (i.e. more than 2 GB) by using the HTTP Range header to download the file in chunks
 */
class RUNTIMEFILESDOWNLOADER_API FRuntimeChunkDownloader : public TSharedFromThis<FRuntimeChunkDownloader>
{
public:
	FRuntimeChunkDownloader();
	virtual ~FRuntimeChunkDownloader();

	/**
	 * Download a file from the specified URL
	 * 
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param OnProgress A function that is called with the progress as BytesReceived and ContentSize
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	virtual TFuture<TArray64<uint8>> DownloadFile(const FString& URL, float Timeout, const FString& ContentType, const TFunction<void(int64, int64)>& OnProgress);

	/**
	 * Download a file by chunks, where each chunk is requested with a Range header that specifies the byte range to retrieve
	 * This method recursively calls itself until the entire file has been downloaded or until the download is canceled
	 * 
	 * @param URL The URL of the file to download
	 * @param Timeout The timeout value in seconds
	 * @param ContentType The content type of the file
	 * @param ContentSize The size of the file to download in bytes. Must be equal or less than TNumericLimits<TArray64<uint8>::SizeType>::Max()
	 * @param MaxChunkSize The maximum size of each chunk to download in bytes. Must be less than or equal to ContentSize
	 * @param OnProgress A function that is called with the progress of the download as a float between 0 and 1
	 * @param InternalContentRange A vector representing the byte range that has already been downloaded
	 * @param InternalResultData The data that has already been downloaded
	 * @return A future that resolves to the downloaded data as a TArray64<uint8>
	 */
	virtual TFuture<TArray64<uint8>> DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, int64 MaxChunkSize, const TFunction<void(int64, int64)>& OnProgress, FInt64Vector2 InternalContentRange = FInt64Vector2(), TArray64<uint8>&& InternalResultData = TArray64<uint8>());

	/**
	 * Cancel the download
	 */
	void CancelDownload();

protected:
	/** A weak pointer to the HTTP request being used for the download */
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	TWeakPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequestPtr;
#else
	TWeakPtr<IHttpRequest> HttpRequestPtr;
#endif

	/** A flag indicating whether the download has been canceled */
	bool bCanceled;

	/**
	 * Get the content length of the file to be downloaded
	 *
	 * @param URL The URL of the file to be downloaded
	 * @param Timeout The timeout value in seconds
	 * @return A future that resolves to the content length of the file to be downloaded in bytes
	 */
	TFuture<int64> GetContentSize(const FString& URL, float Timeout);
};
