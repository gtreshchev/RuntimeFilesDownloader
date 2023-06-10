// Georgy Treshchev 2023.

#include "RuntimeChunkDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

FRuntimeChunkDownloader::FRuntimeChunkDownloader()
	: bCanceled(false)
{}

FRuntimeChunkDownloader::~FRuntimeChunkDownloader()
{
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("FRuntimeChunkDownloader destroyed"));
}

TFuture<TArray64<uint8>> FRuntimeChunkDownloader::DownloadFile(const FString& URL, float Timeout, const FString& ContentType, const TFunction<void(int64, int64)>& OnProgress)
{
	TSharedPtr<TPromise<TArray64<uint8>>> PromisePtr = MakeShared<TPromise<TArray64<uint8>>>();
	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();
	GetContentSize(URL, Timeout).Next([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, OnProgress](int64 ContentLength)
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file: downloader has been destroyed"));
			PromisePtr->SetValue(TArray64<uint8>());
			return;
		}

		if (ContentLength <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file: content length is <= 0"));
			PromisePtr->SetValue(TArray64<uint8>());
			return;
		}

		SharedThis->DownloadFileByChunk(URL, Timeout, ContentType, ContentLength, TNumericLimits<TArray<uint8>::SizeType>::Max(), OnProgress).Next([PromisePtr](TArray64<uint8>&& ResultData)
		{
			PromisePtr->SetValue(MoveTemp(ResultData));
		});
	});
	return PromisePtr->GetFuture();
}

TFuture<TArray64<uint8>> FRuntimeChunkDownloader::DownloadFileByChunk(const FString& URL, float Timeout, const FString& ContentType, int64 ContentSize, int64 MaxChunkSize, const TFunction<void(int64, int64)>& OnProgress, FInt64Vector2 InternalContentRange, TArray64<uint8>&& InternalResultData)
{
	// Check if download has been canceled before starting the download
	if (bCanceled)
	{
		UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Canceled file download from %s"), *URL);
		return MakeFulfilledPromise<TArray64<uint8>>(TArray64<uint8>()).GetFuture();
	}

	if (MaxChunkSize <= 0)
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file from %s: MaxChunkSize is <= 0"), *URL);
		return MakeFulfilledPromise<TArray64<uint8>>(TArray64<uint8>()).GetFuture();
	}

	// If the InternalResultData was not provided, initialize it to the size of the file
	if (InternalResultData.Num() <= 0)
	{
		InternalResultData.SetNumUninitialized(ContentSize);
	}

	// If the InternalContentRange was not provided, set it to the first chunk of size MaxChunkSize
	if (InternalContentRange.X == 0 && InternalContentRange.Y == 0)
	{
		InternalContentRange.Y = FMath::Min(ContentSize, MaxChunkSize) - 1;
	}

	TWeakPtr<FRuntimeChunkDownloader> WeakThisPtr = AsShared();

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("GET");
	HttpRequestRef->SetURL(URL);

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	HttpRequestRef->SetTimeout(Timeout);
#else
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("The Timeout feature is only supported in engine version 4.26 or later. Please update your engine to use this feature"));
#endif

	if (!ContentType.IsEmpty())
	{
		HttpRequestRef->SetHeader(TEXT("Content-Type"), ContentType);
	}

	if (MaxChunkSize < ContentSize)
	{
		const FString RangeHeaderValue = FString::Format(TEXT("bytes={0}-{1}"), {InternalContentRange.X, InternalContentRange.Y});
		HttpRequestRef->SetHeader(TEXT("Range"), RangeHeaderValue);
	}

	HttpRequestRef->OnRequestProgress().BindLambda([WeakThisPtr, ContentSize, InternalContentRange, OnProgress](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (SharedThis.IsValid())
		{
			const float Progress = static_cast<float>(InternalContentRange.X + BytesReceived) / ContentSize;
			UE_LOG(LogRuntimeFilesDownloader, Verbose, TEXT("Downloaded %d bytes of file chunk from %s. Range: {%lld; %lld}, Overall: %lld, Progress: %f"), BytesReceived, *Request->GetURL(), InternalContentRange.X, InternalContentRange.Y, ContentSize, Progress);
			OnProgress(InternalContentRange.X + BytesReceived, ContentSize);
		}
	});

	TSharedPtr<TPromise<TArray64<uint8>>> PromisePtr = MakeShared<TPromise<TArray64<uint8>>>();
	HttpRequestRef->OnProcessRequestComplete().BindLambda([WeakThisPtr, PromisePtr, URL, Timeout, ContentType, ContentSize, MaxChunkSize, InternalContentRange, InternalResultData = MoveTemp(InternalResultData), OnProgress](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) mutable
	{
		TSharedPtr<FRuntimeChunkDownloader> SharedThis = WeakThisPtr.Pin();
		if (!SharedThis.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Failed to download file chunk: downloader has been destroyed"));
			PromisePtr->SetValue(TArray64<uint8>());
			return;
		}

		if (!bSuccess || !Response.IsValid())
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: request failed"), *Request->GetURL());
			PromisePtr->SetValue(TArray64<uint8>());
			return;
		}

		if (Response->GetContentLength() <= 0)
		{
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: content length is 0"), *Request->GetURL());
			PromisePtr->SetValue(TArray64<uint8>());
			return;
		}

		const int64 DataOffset = InternalContentRange.X;
		const TArray<uint8>& ResponseContent = Response->GetContent();

		// Calculate the overall size of the downloaded content in the result buffer
		const int64 OverallDownloadedSize = DataOffset + ResponseContent.Num();

		// Check if some values are out of range
		{
			if (DataOffset < 0 || DataOffset >= InternalResultData.Num())
			{
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: data offset is out of range"), *Request->GetURL());
				PromisePtr->SetValue(TArray64<uint8>());
				return;
			}

			if (OverallDownloadedSize > InternalResultData.Num())
			{
				UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: overall downloaded size is out of range"), *Request->GetURL());
				PromisePtr->SetValue(TArray64<uint8>());
				return;
			}
		}

		FMemory::Memcpy(InternalResultData.GetData() + DataOffset, ResponseContent.GetData(), ResponseContent.Num());

		// Check if there's still more content to download
		if (OverallDownloadedSize < ContentSize)
		{
			// Calculate how much more data needs to be downloaded in the next chunk
			const int64 BytesRemaining = ContentSize - OverallDownloadedSize;
			const int64 BytesToDownload = FMath::Min(BytesRemaining, MaxChunkSize);

			// Calculate the range of data to download in the next chunk
			const FInt64Vector2 NewContentRange = FInt64Vector2(OverallDownloadedSize, OverallDownloadedSize + BytesToDownload - 1);

			// Initiate the next download chunk
			SharedThis->DownloadFileByChunk(URL, Timeout, ContentType, ContentSize, MaxChunkSize, OnProgress, NewContentRange, MoveTemp(InternalResultData)).Next([PromisePtr](TArray64<uint8>&& ResultData)
			{
				PromisePtr->SetValue(MoveTemp(ResultData));
			});
		}
		else
		{
			// If there is no more content to download, then the download is complete
			PromisePtr->SetValue(MoveTemp(InternalResultData));
		}
	});

	if (!HttpRequestRef->ProcessRequest())
	{
		UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to download file chunk from %s: request failed"), *URL);
		return MakeFulfilledPromise<TArray64<uint8>>(TArray64<uint8>()).GetFuture();
	}

	HttpRequestPtr = HttpRequestRef;
	return PromisePtr->GetFuture();
}

void FRuntimeChunkDownloader::CancelDownload()
{
	bCanceled = true;
	if (HttpRequestPtr.IsValid())
	{
#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
		const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpRequestPtr.Pin();
#else
		const TSharedPtr<IHttpRequest> HttpRequest = HttpRequestPtr.Pin();
#endif

		HttpRequest->CancelRequest();
	}
	UE_LOG(LogRuntimeFilesDownloader, Warning, TEXT("Download canceled"));
}

TFuture<int64> FRuntimeChunkDownloader::GetContentSize(const FString& URL, float Timeout)
{
	TSharedPtr<TPromise<int64>> PromisePtr = MakeShared<TPromise<int64>>();

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequestRef = FHttpModule::Get().CreateRequest();
#else
	const TSharedRef<IHttpRequest> HttpRequestRef = FHttpModule::Get().CreateRequest();
#endif

	HttpRequestRef->SetVerb("HEAD");
	HttpRequestRef->SetURL(URL);

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
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
			UE_LOG(LogRuntimeFilesDownloader, Error, TEXT("Failed to get size of file from %s: content length is 0"), *URL);
			PromisePtr->SetValue(0);
			return;
		}

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
