// Georgy Treshchev 2024.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRuntimeFilesDownloaderEditor, Log, All);

class FRuntimeFilesDownloaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
