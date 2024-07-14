// Georgy Treshchev 2024.

#include "RuntimeFilesDownloaderEditor.h"

#include "Misc/EngineVersionComparison.h"

#include "AndroidRuntimeSettings.h"

DEFINE_LOG_CATEGORY(LogRuntimeFilesDownloaderEditor);
#define LOCTEXT_NAMESPACE "FRuntimeFilesDownloaderEditorModule"

void FRuntimeFilesDownloaderEditorModule::StartupModule()
{
	// This is required to access the external storage and record audio on Android
	{
		UAndroidRuntimeSettings* AndroidRuntimeSettings = GetMutableDefault<UAndroidRuntimeSettings>();
		if (!AndroidRuntimeSettings)
		{
			UE_LOG(LogRuntimeFilesDownloaderEditor, Error, TEXT("Failed to get AndroidRuntimeSettings to add permissions"));
			return;
		}

		TSet<FString> NewExtraPermissions = TSet<FString>(AndroidRuntimeSettings->ExtraPermissions);
		TArray<FString> RequiredExtraPermissions = {TEXT("android.permission.READ_EXTERNAL_STORAGE"), TEXT("android.permission.WRITE_EXTERNAL_STORAGE"), TEXT("android.permission.READ_MEDIA_AUDIO"), TEXT("android.permission.READ_MEDIA_VIDEO"), TEXT("android.permission.READ_MEDIA_IMAGES")};
		NewExtraPermissions.Append(RequiredExtraPermissions);
		if (NewExtraPermissions.Num() > AndroidRuntimeSettings->ExtraPermissions.Num())
		{
			AndroidRuntimeSettings->ExtraPermissions = NewExtraPermissions.Array();
#if UE_VERSION_OLDER_THAN(5, 0, 0)
			AndroidRuntimeSettings->UpdateDefaultConfigFile();
#else
			AndroidRuntimeSettings->TryUpdateDefaultConfigFile();
#endif
		}
	}
}

void FRuntimeFilesDownloaderEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRuntimeFilesDownloaderEditorModule, RuntimeFilesDownloaderEditor)
