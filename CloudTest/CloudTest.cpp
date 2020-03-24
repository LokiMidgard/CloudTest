// CloudTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"

#include "CloudTest.h"
#include <iostream>

#include "Utilities.h"
#include "CloudProviderRegistrar.h"
#include <filesystem>
#include <conio.h>

CF_CONNECTION_KEY s_transferCallbackConnectionKey;
HRESULT Init(std::wstring localRoot);
HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, _Out_ USN& usn);
void DisconnectSyncRootTransferCallbacks();
void ConnectSyncRootTransferCallbacks(std::wstring localRoot);


int main()
{
	CoInitialize(nullptr);
	std::cout << "Cloud sample test!\n";

	Init(L"C:\\Test\\Cloud");



	auto fullPath = L"C:\\Test\\Cloud\\test1.txt";
	auto fullPath2 = L"C:\\Test\\Cloud\\test2.txt";
	// cleanup previous run
	if (std::filesystem::exists(fullPath)) {
		int result = _wremove(fullPath);
		if (result != 0) {
			std::cout << "Failed to delete old file\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return result;
		}
	}
	if (std::filesystem::exists(fullPath2)) {
		int result = _wremove(fullPath2);
		if (result != 0) {
			std::cout << "Failed to delete old file2\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return result;
		}
	}

	// SET IN SYNC
	std::cout << "Try Set In Sync\n";
	std::cout << "\n";

	USN usn;
	auto hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"", L"test1.txt", false, usn);
	// We got our USN
	std::cout << "Created placeholder test1.txt with USN " << usn << ".\n";

	if (hr != S_OK) {
		std::cout << "Failed to create placeholder\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}


	HANDLE fileHandle;
	hr = CfOpenFileWithOplock(fullPath, CF_OPEN_FILE_FLAGS::CF_OPEN_FILE_FLAG_WRITE_ACCESS, &fileHandle);
	if (S_OK != hr)
	{
		std::cout << "Failed to open file\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}
	USN tmpUsn;
	tmpUsn = usn;
	// At this point USN is still valid.
	std::cout << "Try to set Sync state IN_SYNC\n";
	hr = CfSetInSyncState(fileHandle, CF_IN_SYNC_STATE::CF_IN_SYNC_STATE_IN_SYNC, CF_SET_IN_SYNC_FLAGS::CF_SET_IN_SYNC_FLAG_NONE, &tmpUsn);
	// Now the USN changed and the method failed.
	if (hr == ERROR_CLOUD_FILE_NOT_IN_SYNC) {
		std::cout << "Unable to set in sync with usn. Returned USN was " << tmpUsn << "\n";
	}
	else if (hr != S_OK)
	{
		std::cout << "Faild to set InSyncState\n";
	}
	else
	{
		std::cout << "Everything OK (will not be called).\n";
	}

	if (tmpUsn == usn) {
		std::cout << "USN was NOT changed.\n";
	}
	else {
		std::cout << "USN was changed now " << tmpUsn << ".\n";
	}


	CfCloseHandle(fileHandle);


	std::cout << "\n";
	std::cout << "\n";

	// SET NOT IN SYNC
	std::cout << "Try Set NOT In Sync\n";
	std::cout << "\n";

	hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"", L"test2.txt", true, usn);
	std::cout << "Created placeholder test2.txt with USN " << usn << ".\n";

	usn = -1;
	std::cout << "setting USN variable to " << usn << " But will still work for NOT_IN_SYNC.\n";

	// We got our USN

	if (hr != S_OK) {
		std::cout << "Failed to create placeholder\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}


	hr = CfOpenFileWithOplock(fullPath2, CF_OPEN_FILE_FLAGS::CF_OPEN_FILE_FLAG_WRITE_ACCESS, &fileHandle);
	if (S_OK != hr)
	{
		std::cout << "Failed to open file\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}

	tmpUsn = usn;
	// At this point USN is still valid.
	hr = CfSetInSyncState(fileHandle, CF_IN_SYNC_STATE::CF_IN_SYNC_STATE_NOT_IN_SYNC, CF_SET_IN_SYNC_FLAGS::CF_SET_IN_SYNC_FLAG_NONE, &tmpUsn);
	// Now the USN changed and the method failed.
	if (hr == ERROR_CLOUD_FILE_NOT_IN_SYNC) {
		std::cout << "unable to set in sync with usn.\n";
	}
	else if (hr != S_OK)
	{
		std::cout << "Faild to set InSyncState\n";
	}
	else
	{
		std::cout << "Seting Sync state to NOT IN SYNC.\n";
	}

	if (tmpUsn == usn) {
		std::cout << "USN was NOT changed.\n";
	}
	else {
		std::cout << "USN was changed now " << tmpUsn << ".\n";
	}


	CfCloseHandle(fileHandle);




	DisconnectSyncRootTransferCallbacks();
	CoUninitialize();

	_getch();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file


HRESULT Mount(std::wstring localRoot) {

	// Stage 1: Setup
	//--------------------------------------------------------------------------------------------
	// The client folder (syncroot) must be indexed in order for states to properly display
	Utilities::AddFolderToSearchIndexer(localRoot.c_str());


	concurrency::create_task([localRoot] {
		// Register the provider with the shell so that the Sync Root shows up in File Explorer
		CloudProviderRegistrar::RegisterWithShell(localRoot, L"FOO");
	}).wait();

	return S_OK;
}

HRESULT IsSyncRoot(LPCWSTR path, _Out_ bool& isSyncRoot) {
	CF_SYNC_ROOT_BASIC_INFO info = { 0 };
	DWORD returnedLength;
	auto hr = CfGetSyncRootInfoByPath(path, CF_SYNC_ROOT_INFO_CLASS::CF_SYNC_ROOT_INFO_BASIC, &info, sizeof(info), &returnedLength);
	isSyncRoot = hr == S_OK && info.SyncRootFileId.QuadPart != 0;
	return S_OK;

	if (hr == 0x80070186) { // 0x80070186: This operation is only supported in a SyncRoot.
		return S_OK;
	}
	return hr;
}


HRESULT Init(std::wstring localRoot) {
	bool isAlreadySyncRoot;
	auto hr = IsSyncRoot(localRoot.c_str(), isAlreadySyncRoot);
	if (hr != S_OK) {
		return hr;
	}

	if (!isAlreadySyncRoot) {
		hr = Mount(localRoot);
		if (hr != S_OK) {
			return hr;
		}
	}

	// Hook up callback methods (in this class) for transferring files between client and server
	ConnectSyncRootTransferCallbacks(localRoot);


	return S_OK;
}

// This is a list of callbacks our fake provider support. This
// class has the callback methods, which are then delegated to
// helper classes
CF_CALLBACK_REGISTRATION s_MirrorCallbackTable[] =
{
	//{ CF_CALLBACK_TYPE_FETCH_DATA, CloudFolder::OnFetchData_C },
	//{ CF_CALLBACK_TYPE_CANCEL_FETCH_DATA, CloudFolder::OnCancelFetchData_C },

	//{ CF_CALLBACK_TYPE_NOTIFY_DELETE_COMPLETION, CloudFolder::OnDeleteCompletion_C },

	//{ CF_CALLBACK_TYPE_NOTIFY_RENAME_COMPLETION, CloudFolder::OnRenameCompletion_C },
	CF_CALLBACK_REGISTRATION_END
};

// Registers the callbacks in the table at the top of this file so that the methods above
// are called for our fake provider
void ConnectSyncRootTransferCallbacks(std::wstring localRoot)
{
	// Connect to the sync root using Cloud File API
	auto hr = CfConnectSyncRoot(
		localRoot.c_str(),
		s_MirrorCallbackTable,
		nullptr,
		CF_CONNECT_FLAGS::CF_CONNECT_FLAG_NONE,
		&s_transferCallbackConnectionKey);
	if (hr != S_OK)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		//LOG(LOG_LEVEL::Error, L"Could not connect to sync root, hr %08x", hr);
	}
}


// Unregisters the callbacks in the table at the top of this file so that 
// the client doesn't Hindenburg
void DisconnectSyncRootTransferCallbacks()
{
	//LOG(LOG_LEVEL::Info, L"Shutting down");
	auto hr = CfDisconnectSyncRoot(s_transferCallbackConnectionKey);

	if (hr != S_OK)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		//LOG(LOG_LEVEL::Error, L"Could not disconnect the sync root, hr %08x", hr);
	}
}






















HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, _Out_ USN& usn)
{
	std::wstring relativePath(parentPath);
	if (relativePath.size() > 0)
		if (relativePath.at(relativePath.size() - 1) != L'\\')
		{
			relativePath.append(L"\\");
		}
	relativePath.append(fileName);

	FileMetaData metadata = {};

	metadata.FileSize = 0;
	metadata.IsDirectory = false;

	fileName.copy(metadata.Name, fileName.length());

	CF_PLACEHOLDER_CREATE_INFO cloudEntry;
	auto fileIdentety = L"F";
	cloudEntry.FileIdentity = &fileIdentety;
	cloudEntry.FileIdentityLength = sizeof(fileIdentety);

	cloudEntry.RelativeFileName = relativePath.data();
	cloudEntry.Flags = inSync
		? CF_PLACEHOLDER_CREATE_FLAGS::CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC
		: CF_PLACEHOLDER_CREATE_FLAGS::CF_PLACEHOLDER_CREATE_FLAG_NONE;
	cloudEntry.FsMetadata.FileSize.QuadPart = metadata.FileSize;
	cloudEntry.FsMetadata.BasicInfo.FileAttributes = metadata.FileAttributes;
	cloudEntry.FsMetadata.BasicInfo.CreationTime = metadata.CreationTime;
	cloudEntry.FsMetadata.BasicInfo.LastWriteTime = metadata.LastWriteTime;
	cloudEntry.FsMetadata.BasicInfo.LastAccessTime = metadata.LastAccessTime;
	cloudEntry.FsMetadata.BasicInfo.ChangeTime = metadata.ChangeTime;

	if (metadata.IsDirectory)
	{
		cloudEntry.FsMetadata.BasicInfo.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
		cloudEntry.Flags |= CF_PLACEHOLDER_CREATE_FLAG_DISABLE_ON_DEMAND_POPULATION;
		cloudEntry.FsMetadata.FileSize.QuadPart = 0;
	}

	auto hr = CfCreatePlaceholders(localRoot.c_str(), &cloudEntry, 1, CF_CREATE_FLAGS::CF_CREATE_FLAG_NONE, NULL);
	if (hr != S_OK) {
		return hr;
	}



	//std::wstring absolutePath(>localRoot);
	//if (absolutePath.size() > 0 && absolutePath.at(absolutePath.size() - 1) != L'\\')
	//	absolutePath.append(L"\\");

	//absolutePath.append(relativePath);
	//DWORD attrib = GetFileAttributes(absolutePath.c_str());
	usn = cloudEntry.CreateUsn;
	return cloudEntry.Result;
}
