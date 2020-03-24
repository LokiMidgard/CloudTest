// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "stdafx.h"

#include <SearchAPI.h>    // needed for AddFolderToSearchIndexer
#include <propkey.h>      // needed for ApplyTransferStateToFile
#include <propvarutil.h>  // needed for ApplyTransferStateToFile

#define MSSEARCH_INDEX L"SystemIndex"
DEFINE_PROPERTYKEY(PKEY_StorageProviderTransferProgress, 0xE77E90DF, 0x6271, 0x4F5B, 0x83, 0x4F, 0x2D, 0xD1, 0xF2, 0x45, 0xDD, 0xA4, 4);

// All methods and fields for this class are static

// If the local (client) folder where the cloud file placeholders are created
// is not under the User folder (i.e. Documents, Photos, etc), then it is required
// to add the folder to the Search Indexer. This is because the properties for
// the cloud file state/progress are cached in the indexer, and if the folder isn't
// indexed, attempts to get the properties on items will not return the expected values.
HRESULT Utilities::AddFolderToSearchIndexer(_In_ PCWSTR folder)
{
	std::wstring url(L"file:///");
	url.append(folder);

	try
	{
		winrt::com_ptr<ISearchManager> searchManager;
		auto hr = CoCreateInstance(__uuidof(CSearchManager), NULL, CLSCTX_SERVER, __uuidof(&searchManager), searchManager.put_void());
		if (hr != S_OK) {
			return hr;
		}

		winrt::com_ptr<ISearchCatalogManager> searchCatalogManager;
		hr = searchManager->GetCatalog(MSSEARCH_INDEX, searchCatalogManager.put());
		if (hr != S_OK) {
			return hr;
		}

		winrt::com_ptr<ISearchCrawlScopeManager> searchCrawlScopeManager;
		hr = searchCatalogManager->GetCrawlScopeManager(searchCrawlScopeManager.put());
		if (hr != S_OK) {
			return hr;
		}

		hr = searchCrawlScopeManager->AddDefaultScopeRule(url.data(), TRUE, FOLLOW_FLAGS::FF_INDEXCOMPLEXURLS);
		if (hr != S_OK) {
			return hr;
		}

		hr = searchCrawlScopeManager->SaveAll();
		if (hr != S_OK) {
			return hr;
		}

		wprintf(L"Succesfully called AddFolderToSearchIndexer on \"%s\"\n", url.data());
	}
	catch (...)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		wprintf(L"Failed on call to AddFolderToSearchIndexer for \"%s\" with %08x\n", url.data(), static_cast<HRESULT>(winrt::to_hresult()));
		return static_cast<HRESULT>(winrt::to_hresult());
	}
	return S_OK;
}


