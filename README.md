![GitHub](https://img.shields.io/github/license/Respirant/RuntimeFilesDownloader)
# RuntimeFilesDownloader
Runtime Files Downloader plugin for Unreal Engine. Allows you to download any files via the HTTP protocol to the device memory. It's easy to use in both C++ and Blueprints.

![Runtime Files Downloader Unreal Engine Plugin Logo](image/runtimefilesdownloader.png "RuntimeFilesDownloader Unreal Engine Plugin Logo")

## Features
- Low library size (≈ 30 kb)
- No any third party libraries and external dependencies
- Support for all available devices (tested on Windows and Android, but there are no restrictions to work on other devices)

![Runtime Files Downloader Unreal Engine Plugin Nodes 1](image/nodesexample1.png "RuntimeFilesDownloader Unreal Engine Plugin Nodes 1")
![Runtime Files Downloader Unreal Engine Plugin Nodes 2](image/nodesexample2.png "RuntimeFilesDownloader Unreal Engine Plugin Nodes 2")

## How to install
There're two ways to install the plugin:
1) [Through the marketplace (not ready yet)](https://www.unrealengine.com/marketplace/product/runtime-files-downloader).
2) Manual installation. Just extract the "RuntimeFilesDownloader" folder into your plugins project folder to get the following path: "[ProjectName]/Plugins/RuntimeFilesDownloader".

## How to use
 There are two ways to use this plugin: using Blueprints and C++:
 1. Blueprints. Just reopen your Unreal project and use "Create Downloader" and "Download File" nodes.
 2. C++. Open your "[ProjectName].Build.cs" and find a block of ` PublicDependencyModuleNames ` or ` PrivateDependencyModuleNames ` (it depends on your needs). Add `"RuntimeFilesDownloader"` String inside this block. After just include "RuntimeFilesDownloaderBPLibrary.h" using ` #include "RuntimeFilesDownloaderBPLibrary.h" ` where needed.

## Legal info

Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.

Unreal® Engine, Copyright 1998 – 2020, Epic Games, Inc. All rights reserved.
