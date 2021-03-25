// Georgy Treshchev 2021.

#pragma once

#include "Modules/ModuleManager.h"

class FRuntimeFilesDownloaderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
