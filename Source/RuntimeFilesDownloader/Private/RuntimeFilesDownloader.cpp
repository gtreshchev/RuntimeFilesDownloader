// Georgy Treshchev 2022.

#include "RuntimeFilesDownloader.h"
#include "RuntimeFilesDownloaderDefines.h"

#define LOCTEXT_NAMESPACE "FRuntimeFilesDownloaderModule"

void FRuntimeFilesDownloaderModule::StartupModule()
{
}

void FRuntimeFilesDownloaderModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRuntimeFilesDownloaderModule, RuntimeFilesDownloader)

DEFINE_LOG_CATEGORY(LogRuntimeFilesDownloader);