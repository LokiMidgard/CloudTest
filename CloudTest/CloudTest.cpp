// CloudTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"

#include "CloudTest.h"
#include <iostream>

#include "Utilities.h"
#include "CloudProviderRegistrar.h"
#include <filesystem>
#include <conio.h>


#include <propkey.h>      // needed for ApplyTransferStateToFile
#include <propvarutil.h>  // needed for ApplyTransferStateToFile


#define TEXT_STATUS(status)(status == SYNC_TRANSFER_STATUS::STS_HASERROR \
		? "SYNC_TRANSFER_STATUS::STS_HASERROR" \
		: status == SYNC_TRANSFER_STATUS::STS_HASWARNING \
		? "SYNC_TRANSFER_STATUS::STS_HASWARNING" \
		: "normal") 


CF_CONNECTION_KEY s_transferCallbackConnectionKey;
HRESULT Init(std::wstring localRoot);
HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, bool isFolder, _Out_ USN& usn);
void DisconnectSyncRootTransferCallbacks();
void ConnectSyncRootTransferCallbacks(std::wstring localRoot);
HRESULT GetUSN(LPCWSTR path, _Out_ USN& usn);
void SetTransferStatus(_In_ PCWSTR fullPath, _In_ SYNC_TRANSFER_STATUS status);
SYNC_TRANSFER_STATUS  GetTransferStatus(_In_ PCWSTR fullPath);

int main()
{
	CoInitialize(nullptr);
	std::cout << "Cloud sample test!\n";

	Init(L"C:\\Test\\Cloud");



	auto fullPath = L"C:\\Test\\Cloud\\T\\test1.txt";
	auto fullPathFolder = L"C:\\Test\\Cloud\\T";
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

	if (std::filesystem::exists(fullPathFolder)) {
		if (!std::filesystem::remove(fullPathFolder)) {
			auto error = GetLastError();
			std::cout << "Failed to delete old folder\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return error;
		}
	}

	// SET IN SYNC
	std::cout << "Try Set In Sync\n";
	std::cout << "\n";

	USN usn;
	auto hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"", L"T", true, true, usn);
	std::cout << "Created folder T.\n";

	if (hr != S_OK) {
		std::cout << "Failed to create placeholder\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}
	hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"T", L"test1.txt", true, false, usn);
	// We got our USN
	std::cout << "Created placeholder T/test1.txt with USN.\n";

	if (hr != S_OK) {
		std::cout << "Failed to create placeholder\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}

	std::cout << "\n";
	std::cout << "\n";

	HANDLE hfile = CreateFile(
		fullPath2,
		GENERIC_READ,
		0,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (!CloseHandle(hfile)) {
		std::cout << "Failed to create file\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}



	auto old = GetTransferStatus(fullPath);
	std::cout << "You should see a cloud symbol now if this is the first time you execute this.\n";
	std::cout << "If you run this a seccond time you may now need to hit F5 in the explorer you will see the " <<
		TEXT_STATUS(old) << " again.\n";
	std::cout << "The previous icon is displayed even the file was deleted and a new file was created.\n";
	std::cout << "The error is set on the file but only the folder showes the error, the file still keep the cloud symbol.\n";
	std::cout << "\nPress key to continue\n";

	_getch();


	auto newValue = old != SYNC_TRANSFER_STATUS::STS_HASERROR ? SYNC_TRANSFER_STATUS::STS_HASERROR : SYNC_TRANSFER_STATUS::STS_HASWARNING;
	SetTransferStatus(fullPath, newValue);

	std::cout << "The icon should now be" <<
		TEXT_STATUS(newValue) << ".\n";


	SetTransferStatus(fullPath2, SYNC_TRANSFER_STATUS::STS_EXCLUDED);
	std::cout << "Error was set, but the file T/test1.txt still has cloud symbol.\n";
	std::cout << "\nPress key to exit\n";


	_getch();


	DisconnectSyncRootTransferCallbacks();
	CoUninitialize();
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


void SetTransferStatus(_In_ PCWSTR fullPath, _In_ SYNC_TRANSFER_STATUS status)
{
	// Tell the Shell so File Explorer can display the progress bar in its view
	try
	{
		// First, get the Volatile property store for the file. That's where the properties are maintained.
		winrt::com_ptr<IShellItem2> shellItem;
		winrt::check_hresult(SHCreateItemFromParsingName(fullPath, nullptr, __uuidof(shellItem), shellItem.put_void()));

		winrt::com_ptr<IPropertyStore> propStoreVolatile;
		winrt::check_hresult(
			shellItem->GetPropertyStore(
				GETPROPERTYSTOREFLAGS::GPS_READWRITE | GETPROPERTYSTOREFLAGS::GPS_VOLATILEPROPERTIESONLY,
				__uuidof(propStoreVolatile),
				propStoreVolatile.put_void()));

		// Set the sync transfer status accordingly
		PROPVARIANT transferStatus;
		winrt::check_hresult(
			InitPropVariantFromUInt32(
				status,
				&transferStatus));
		winrt::check_hresult(propStoreVolatile->SetValue(PKEY_SyncTransferStatus, transferStatus));

		// Without this, all your hard work is wasted.
		winrt::check_hresult(propStoreVolatile->Commit());

		// Broadcast a notification that something about the file has changed, so that apps
		// who subscribe (such as File Explorer) can update their UI to reflect the new progress
		SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, static_cast<LPCVOID>(fullPath), nullptr);

		//wprintf(L"Succesfully Set Transfer Progress on \"%s\" to %llu/%llu\n", fullPath, completed, total);
	}
	catch (...)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		wprintf(L"Failed to Set Transfer Progress on \"%s\" with %08x\n", fullPath, static_cast<HRESULT>(winrt::to_hresult()));
	}
}

SYNC_TRANSFER_STATUS  GetTransferStatus(_In_ PCWSTR fullPath)
{
	// Tell the Shell so File Explorer can display the progress bar in its view
	try
	{
		// First, get the Volatile property store for the file. That's where the properties are maintained.
		winrt::com_ptr<IShellItem2> shellItem;
		winrt::check_hresult(SHCreateItemFromParsingName(fullPath, nullptr, __uuidof(shellItem), shellItem.put_void()));

		winrt::com_ptr<IPropertyStore> propStoreVolatile;
		winrt::check_hresult(
			shellItem->GetPropertyStore(
				GETPROPERTYSTOREFLAGS::GPS_READWRITE | GETPROPERTYSTOREFLAGS::GPS_VOLATILEPROPERTIESONLY,
				__uuidof(propStoreVolatile),
				propStoreVolatile.put_void()));

		// Set the sync transfer status accordingly
		PROPVARIANT transferStatus;
		winrt::check_hresult(propStoreVolatile->GetValue(PKEY_SyncTransferStatus, &transferStatus));

		SYNC_TRANSFER_STATUS result = (SYNC_TRANSFER_STATUS)(transferStatus.uintVal);
		return result;
	}
	catch (...)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		wprintf(L"Failed to Set Transfer Progress on \"%s\" with %08x\n", fullPath, static_cast<HRESULT>(winrt::to_hresult()));
	}
}


HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, bool isFolder, _Out_ USN& usn)
{
	std::wstring relativePath(localRoot);
	if (relativePath.size() > 0)
		if (relativePath.at(relativePath.size() - 1) != L'\\')
		{
			relativePath.append(L"\\");
		}
	relativePath.append(parentPath);

	FileMetaData metadata = {};

	metadata.FileSize = 0;
	metadata.IsDirectory = isFolder;

	fileName.copy(metadata.Name, fileName.length());

	CF_PLACEHOLDER_CREATE_INFO cloudEntry;
	auto fileIdentety = L"F";
	cloudEntry.FileIdentity = &fileIdentety;
	cloudEntry.FileIdentityLength = sizeof(fileIdentety);

	cloudEntry.RelativeFileName = fileName.c_str();
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

	auto hr = CfCreatePlaceholders(relativePath.c_str(), &cloudEntry, 1, CF_CREATE_FLAGS::CF_CREATE_FLAG_NONE, NULL);
	if (hr != S_OK) {
		return hr;
	}

	usn = cloudEntry.CreateUsn;
	return cloudEntry.Result;
}
