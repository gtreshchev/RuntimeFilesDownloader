// Georgy Treshchev 2024.

#include "RuntimeChunkDownloader.h"

#include "FileToMemoryDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#if PLATFORM_ANDROID
#include "Async/Future.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#include "Android/AndroidPlatformMisc.h"
#endif

FRuntimeChunkDownloader::FRuntimeChunkDownloader()
	: bCanceled(false)
{}

FRuntimeChunkDownloader::~FRuntimeChunkDownloader()
{
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("FRuntimeChunkDownloader destroyed"));
}

TFuture<FRuntimeChunkDownloaderResult> FRuntimeChunkDownloader::DownloadFile(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, const FOnProgress& OnProgress)
{
	if (bCanceled)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TSharedPtr<TPromise<FRuntimeChunkDownloaderResult>> PromisePtr = MakeShared<TPromise<FRuntimeChunkDownloaderResult>>();
	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize, OnProgress](int64 ContentSize) mutable
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->bCanceled)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		auto DownloadByPayload = [SharedThis, WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnProgress]()
		{
			SharedThis->DownloadFileByPayload(URL, Timeout, ContentType, OnProgress).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnProgress](FRuntimeChunkDownloaderResult Result) mutable
			{
				TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
				if (!SharedThis.IsValid())
					{
					UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
					PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
					return;
				}

				if (SharedThis->bCanceled)
				{
					UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
					PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
					return;
				}

				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{Result.Result, MoveTemp(Result.Data)});
			});
		};
		
		if (ContentSize <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL);
			DownloadByPayload();
			return;
		}

		if (MaxChunkSize <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: MaxChunkSize is <= 0. Trying to download the file by payload"), *URL);
			DownloadByPayload();
			return;
		}

		TSharedPtr<TArray64<uint8>> OverallDownloadedDataPtr = MakeShared<TArray64<uint8>>();
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Pre-allocating %lld bytes for file download from %s"), ContentSize, *URL);
			OverallDownloadedDataPtr->SetNumUninitialized(ContentSize);
		}

		FInt64Vector2 ChunkRange;
		{
			ChunkRange.X = 0;
			ChunkRange.Y = FMath::Min(MaxChunkSize, ContentSize) - 1;
		}

		TSharedPtr<int64> ChunkOffsetPtr = MakeShared<int64>(ChunkRange.X);
		TSharedPtr<bool> bChunkDownloadedFilledPtr = MakeShared<bool>(false);

		auto OnChunkDownloadedFilled = [bChunkDownloadedFilledPtr]()
		{
			if (bChunkDownloadedFilledPtr.IsValid())
			{
				*bChunkDownloadedFilledPtr = true;
			}
		};

		auto OnChunkDownloaded = [WeakThisPtr, PromisePtr, URL, ContentSize, Timeout, ContentType, OnProgress, DownloadByPayload, OverallDownloadedDataPtr, bChunkDownloadedFilledPtr, ChunkOffsetPtr, OnChunkDownloadedFilled](TArray64<uint8>&& ResultData) mutable
		{
			TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
			if (!SharedThis.IsValid())
			{
				UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			if (ResultData.Num() <= 0)
			{
				UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: result data is empty"), *URL);
				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			if (SharedThis->bCanceled)
			{
				UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
				OnChunkDownloadedFilled();
				return;
			}

			// Calculate the currently size of the downloaded content in the result buffer
			const int64 CurrentlyDownloadedSize = *ChunkOffsetPtr + ResultData.Num();

			// Check if some values are out of range
			{
				if (*ChunkOffsetPtr < 0 || *ChunkOffsetPtr >= OverallDownloadedDataPtr->Num())
				{
					UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: data offset is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, *ChunkOffsetPtr, OverallDownloadedDataPtr->Num());
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}

				if (CurrentlyDownloadedSize > OverallDownloadedDataPtr->Num())
				{
					UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: overall downloaded size is out of range (%lld, expected [0, %lld]). Trying to download the file by payload"), *URL, CurrentlyDownloadedSize, OverallDownloadedDataPtr->Num());
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}
			}

			// Append the downloaded chunk to the result data
			FMemory::Memcpy(OverallDownloadedDataPtr->GetData() + *ChunkOffsetPtr, ResultData.GetData(), ResultData.Num());

			// If the download is complete, return the result data
			if (*ChunkOffsetPtr + ResultData.Num() >= ContentSize)
			{
				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Success, MoveTemp(*OverallDownloadedDataPtr.Get())});
				OnChunkDownloadedFilled();
				return;
			}

			// Increase the offset by the size of the downloaded chunk
			*ChunkOffsetPtr += ResultData.Num();
		};

		SharedThis->DownloadFilePerChunk(URL, Timeout, ContentType, MaxChunkSize, ChunkRange, OnProgress, OnChunkDownloaded).Next([PromisePtr, bChunkDownloadedFilledPtr, URL, OverallDownloadedDataPtr, OnChunkDownloadedFilled, DownloadByPayload](EDownloadToMemoryResult Result) mutable
		{
			// Only return data if no chunk was downloaded
			if (bChunkDownloadedFilledPtr.IsValid() && (*bChunkDownloadedFilledPtr.Get() == false))
			{
				if (Result != EDownloadToMemoryResult::Success && Result != EDownloadToMemoryResult::SucceededByPayload)
				{
					UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: download failed. Trying to download the file by payload"), *URL);
					DownloadByPayload();
					OnChunkDownloadedFilled();
					return;
				}
				OverallDownloadedDataPtr->Shrink();
				PromisePtr->SetValue(FRuntimeChunkDownloaderResult{Result, MoveTemp(*OverallDownloadedDataPtr.Get())});
			}
		});
	});
	return PromisePtr->GetFuture();
}

TFuture<EDownloadToMemoryResult> FRuntimeChunkDownloader::DownloadFilePerChunk(const FString& URL, float Timeout, const FString& ContentType, int64 MaxChunkSize, FInt64Vector2 ChunkRange, const FOnProgress& OnProgress, const FOnChunkDownloaded& OnChunkDownloaded)
{
	if (bCanceled)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
		return MakeFulfilledPromise<EDownloadToMemoryResult>(EDownloadToMemoryResult::Cancelled).GetFuture();
	}

	TSharedPtr<TPromise<EDownloadToMemoryResult>> PromisePtr = MakeShared<TPromise<EDownloadToMemoryResult>>();
	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize, OnProgress, OnChunkDownloaded, ChunkRange](int64 ContentSize) mutable
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}
		
		if (SharedThis->bCanceled)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
			return;
		}

		if (ContentSize <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Unable to get content size for %s. Trying to download the file by payload"), *URL);
			SharedThis->DownloadFileByPayload(URL, Timeout, ContentType, OnProgress).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnChunkDownloaded, OnProgress](FRuntimeChunkDownloaderResult Result) mutable
			{
				TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
				if (!SharedThis.IsValid())
				{
					UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
					return;
				}

				if (SharedThis->bCanceled)
				{
					UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
					return;
				}

				if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
				{
					UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result));
					PromisePtr->SetValue(Result.Result);
					return;
				}

				if (Result.Data.Num() <= 0)
				{
					UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: downloaded content is empty"), *URL);
					PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
					return;
				}

				PromisePtr->SetValue(Result.Result);
				OnChunkDownloaded(MoveTemp(Result.Data));
			});
			return;
		}

		if (MaxChunkSize <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: max chunk size is <= 0"), *URL);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}

		// If the chunk range is not specified, determine the range based on the max chunk size and the content size
		if (ChunkRange.X == 0 && ChunkRange.Y == 0)
		{
			ChunkRange.Y = FMath::Min(MaxChunkSize, ContentSize) - 1;
		}

		if (ChunkRange.Y > ContentSize)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: chunk range is out of range (%lld, expected [0, %lld])"), *URL, ChunkRange.Y, ContentSize);
			PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
			return;
		}

		auto OnProgressInternal = [WeakThisPtr, PromisePtr, URL, Timeout, ContentType, MaxChunkSize, OnChunkDownloaded, OnProgress, ChunkRange](int64 BytesReceived, int64 ContentSize) mutable
		{
			TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
			if (SharedThis.IsValid())
			{
				const float Progress = ContentSize <= 0 ? 0.0f : static_cast<float>(BytesReceived + ChunkRange.X) / ContentSize;
				UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Downloaded %lld bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *URL, ChunkRange.X, ChunkRange.Y, ContentSize, Progress);
				OnProgress(BytesReceived + ChunkRange.X, ContentSize);
			}
		};

		SharedThis->DownloadFileByChunk(URL, Timeout, ContentType, ContentSize, ChunkRange, OnProgressInternal).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, ContentSize, MaxChunkSize, OnChunkDownloaded, OnProgress, ChunkRange](FRuntimeChunkDownloaderResult&& Result)
		{
			TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
			if (!SharedThis.IsValid())
			{
				UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
				PromisePtr->SetValue(EDownloadToMemoryResult::DownloadFailed);
				return;
			}

			if (SharedThis->bCanceled)
			{
				UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
				PromisePtr->SetValue(EDownloadToMemoryResult::Cancelled);
				return;
			}

			if (Result.Result != EDownloadToMemoryResult::Success && Result.Result != EDownloadToMemoryResult::SucceededByPayload)
			{
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: %s"), *URL, *UEnum::GetValueAsString(Result.Result));
				PromisePtr->SetValue(Result.Result);
				return;
			}

			OnChunkDownloaded(MoveTemp(Result.Data));

			// Check if the download is complete
			if (ContentSize > ChunkRange.Y + 1)
			{
				const int64 ChunkStart = ChunkRange.Y + 1;
				const int64 ChunkEnd = FMath::Min(ChunkStart + MaxChunkSize, ContentSize) - 1;

				SharedThis->DownloadFilePerChunk(URL, Timeout, ContentType, MaxChunkSize, FInt64Vector2(ChunkStart, ChunkEnd), OnProgress, OnChunkDownloaded).Next([WeakThisPtr, PromisePtr](EDownloadToMemoryResult Result)
				{
					PromisePtr->SetValue(Result);
				});
			}
			else
			{
				PromisePtr->SetValue(EDownloadToMemoryResult::Success);
			}
		});
	});

	return PromisePtr->GetFuture();
}

TFuture<FRuntimeChunkDownloaderResult> FRuntimeChunkDownloader::DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, FInt64Vector2 ChunkRange, const FOnProgress& OnProgress)
{
	if (bCanceled)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.X < 0 || ChunkRange.Y <= 0 || ChunkRange.X > ChunkRange.Y)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is invalid"), *URL, ChunkRange.X, ChunkRange.Y);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	if (ChunkRange.Y - ChunkRange.X + 1 > ContentSize)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: chunk range (%lld; %lld) is out of range (%lld)"), *URL, ChunkRange.X, ChunkRange.Y, ContentSize);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	if (!ContentType.IsEmpty())
	{
		HttpRequestRef->SetHeader(TEXT("Content-Type"), ContentType);
	}

	const FString RangeHeaderValue = FString::Format(TEXT("bytes={0}-{1}"), {ChunkRange.X, ChunkRange.Y});
	HttpRequestRef->SetHeader(TEXT("Range"), RangeHeaderValue);

	HttpRequestRef->
#if UE_VERSION_OLDER_THAN(5, 4, 0)
		OnRequestProgress().BindLambda([WeakThisPtr, ContentSize, ChunkRange, OnProgress](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
#else
		OnRequestProgress64().BindLambda([WeakThisPtr, ContentSize, ChunkRange, OnProgress](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
#endif
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const float Progress = ContentSize <= 0 ? 0.0f : static_cast<float>(BytesReceived) / ContentSize;
			UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Downloaded %lld bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), static_cast<int64>(BytesReceived), *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentSize, Progress);
			OnProgress(BytesReceived, ContentSize);
		}
	});

	TSharedPtr<TPromise<FRuntimeChunkDownloaderResult>> PromisePtr = MakeShared<TPromise<FRuntimeChunkDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL, ChunkRange](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk from %s: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->bCanceled)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file chunk download from %s"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: request failed"), *Request->GetURL());
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: content length is 0"), *Request->GetURL());
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));

		if (ContentLength != ChunkRange.Y - ChunkRange.X + 1)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: content length (%lld) does not match the expected length (%lld)"), *Request->GetURL(), ContentLength, ChunkRange.Y - ChunkRange.X + 1);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Successfully downloaded file chunk from %s. Range: {%lld; %lld}, Overall: %lld"), *Request->GetURL(), ChunkRange.X, ChunkRange.Y, ContentLength);
		PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Success, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: request failed"), *URL);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<FRuntimeChunkDownloaderResult> FRuntimeChunkDownloader::DownloadFileByPayload(const FString& URL, float Timeout, const FString& ContentType, const FOnProgress& OnProgress)
{
	if (bCanceled)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()}).GetFuture();
	}

	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	HttpRequestRef->
#if UE_VERSION_OLDER_THAN(5, 4, 0)
		OnRequestProgress().BindLambda([WeakThisPtr, OnProgress](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
#else
		OnRequestProgress64().BindLambda([WeakThisPtr, OnProgress](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
#endif
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const int64 ContentLength = Request->GetContentLength();
			const float Progress = ContentLength <= 0 ? 0.0f : static_cast<float>(BytesReceived) / ContentLength;
			UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Downloaded %lld bytes of file chunk from %s by payload. Overall: %lld, Progress: %f"), static_cast<int64>(BytesReceived), *Request->GetURL(), static_cast<int64>(Request->GetContentLength()), Progress);
			OnProgress(BytesReceived, ContentLength);
		}
	});

	TSharedPtr<TPromise<FRuntimeChunkDownloaderResult>> PromisePtr = MakeShared<TPromise<FRuntimeChunkDownloaderResult>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file from %s by payload: downloader has been destroyed"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (SharedThis->bCanceled)
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s by payload"), *URL);
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::Cancelled, TArray64<uint8>()});
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file from %s by payload: request failed"), *Request->GetURL());
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file from %s by payload: content length is 0"), *Request->GetURL());
			PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()});
			return;
		}

		UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Successfully downloaded file from %s by payload. Overall: %lld"), *Request->GetURL(), static_cast<int64>(Response->GetContentLength()));
		return PromisePtr->SetValue(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::SucceededByPayload, TArray64<uint8>(Response->GetContent())});
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file from %s by payload: request failed"), *URL);
		return MakeFulfilledPromise<FRuntimeChunkDownloaderResult>(FRuntimeChunkDownloaderResult{EDownloadToMemoryResult::DownloadFailed, TArray64<uint8>()}).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

TFuture<int64> FRuntimeChunkDownloader::GetContentSize(const FString& URL, float Timeout)
{
	TSharedPtr<TPromise<int64>> PromisePtr = MakeShared<TPromise<int64>>();

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("HEAD");
	HttpRequestRef->SetURL(URL);

#if UE_VERSION_NEWER_THAN(4, 26, 0)
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	HttpRequestRef->OnProcessRequestComplete().BindLambda([PromisePtr, URL](const FHttpRequestPtr& Request, const FHttpResponsePtr& Response, const bool bSucceeded)
	{
		if (!bSucceeded || !Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get size of file from %s: request failed"), *URL);
			PromisePtr->SetValue(0);
			return;
		}

		const int64 ContentLength = FCString::Atoi64(*Response->GetHeader("Content-Length"));
		if (ContentLength <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get size of file from %s: content length is %lld, expected > 0"), *URL, ContentLength);
			PromisePtr->SetValue(0);
			return;
		}

		UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Got size of file from %s: %lld"), *URL, ContentLength);
		PromisePtr->SetValue(ContentLength);
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get size of file from %s: request failed"), *URL);
		return MakeFulfilledPromise<int64>(0).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

void FRuntimeChunkDownloader::CancelDownload()
{
	bCanceled = true;
	if (HttpRequestPtr.IsValid())
	{
#if UE_VERSION_NEWER_THAN(4, 26, 0)
		const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpRequestPtr.Pin();
#else
		const TSharedPtr<IHttpRequest> HttpRequest = HttpRequestPtr.Pin();
#endif

		HttpRequest->CancelRequest();
	}
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Download canceled"));
}

TFuture<bool> FRuntimeChunkDownloader::CheckAndRequestPermissions()
{
#if PLATFORM_ANDROID
	TArray<FString> AllRequiredPermissions = []()
	{
		TArray<FString> InternalPermissions = {TEXT("android.permission.READ_EXTERNAL_STORAGE"), TEXT("android.permission.WRITE_EXTERNAL_STORAGE")};
		if (FAndroidMisc::GetAndroidBuildVersion() >= 33)
		{
			InternalPermissions.Append(TArray<FString>{TEXT("android.permission.READ_MEDIA_AUDIO"), TEXT("android.permission.READ_MEDIA_VIDEO"), TEXT("android.permission.READ_MEDIA_IMAGES")});
		}
		return InternalPermissions;
	}();

	TArray<FString> UngrantedPermissions;
	for (const FString& Permission : AllRequiredPermissions)
	{
		if (!UAndroidPermissionFunctionLibrary::CheckPermission(Permission))
		{
			UngrantedPermissions.Add(Permission);
			UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Permission '%s' is not granted on Android. Requesting permission..."), *Permission);
		}
		else
		{
			UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Permission '%s' is granted on Android. No need to request permission"), *Permission);
		}
	}

	if (UngrantedPermissions.Num() == 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("All required permissions are granted on Android. No need to request permissions"));
		return MakeFulfilledPromise<bool>(true).GetFuture();
	}

	TSharedRef<TPromise<bool>> bPermissionGrantedPromise = MakeShared<TPromise<bool>>();
	TFuture<bool> bPermissionGrantedFuture = bPermissionGrantedPromise->GetFuture();
	UAndroidPermissionCallbackProxy* PermissionsGrantedCallbackProxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(UngrantedPermissions);
	if (!PermissionsGrantedCallbackProxy)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to request permissions for reading/writing audio data on Android (PermissionsGrantedCallbackProxy is null)"));
		return MakeFulfilledPromise<bool>(false).GetFuture();
	}

	FAndroidPermissionDelegate OnPermissionsGrantedDelegate;
#if UE_VERSION_NEWER_THAN(5, 0, 0)
	TSharedRef<FDelegateHandle> OnPermissionsGrantedDelegateHandle = MakeShared<FDelegateHandle>();
	*OnPermissionsGrantedDelegateHandle = OnPermissionsGrantedDelegate.AddLambda
#else
	OnPermissionsGrantedDelegate.BindLambda
#endif
	([
#if UE_VERSION_NEWER_THAN(5, 0, 0)
	OnPermissionsGrantedDelegateHandle,
#endif
	OnPermissionsGrantedDelegate, bPermissionGrantedPromise, Permissions = MoveTemp(UngrantedPermissions)](const TArray<FString>& GrantPermissions, const TArray<bool>& GrantResults) mutable
	{
#if UE_VERSION_NEWER_THAN(5, 0, 0)
		OnPermissionsGrantedDelegate.Remove(OnPermissionsGrantedDelegateHandle.Get());
#else
		OnPermissionsGrantedDelegate.Unbind();
#endif
		for (const FString& Permission : Permissions)
		{
			if (!GrantPermissions.Contains(Permission) || !GrantResults.IsValidIndex(GrantPermissions.Find(Permission)) || !GrantResults[GrantPermissions.Find(Permission)])
			{
				TArray<FString> GrantResultsString;
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Unable to request permission '%s' for reading/writing audio data on Android"), *Permission);
				bPermissionGrantedPromise->SetValue(false);
				return;
			}
		}
		UE_LOG(LogRuntimeFilesDownloader, Log, TEXT("Successfully granted permission for reading/writing audio data on Android"));
		bPermissionGrantedPromise->SetValue(true);
	});
	PermissionsGrantedCallbackProxy->OnPermissionsGrantedDelegate = OnPermissionsGrantedDelegate;
	return bPermissionGrantedFuture;
#else
	return MakeFulfilledPromise<bool>(true).GetFuture();
#endif
}
