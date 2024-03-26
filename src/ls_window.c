#include <lysys/ls_window.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"

int ls_dialog_message(void *parent, const char *title, const char *message, int flags)
{
#if LS_WINDOWS
    UINT type;
    int r;
    LPWSTR lpTitle, lpMessage;

    switch (flags & LS_DIALOG_TYPE_MASK)
    {
    case LS_DIALOG_OK:
        type = MB_OK;
        break;
    case LS_DIALOG_OKCANCEL:
        type = MB_OKCANCEL;
        break;
    case LS_DIALOG_ABORTRETRYIGNORE:
        type = MB_ABORTRETRYIGNORE;
        break;
    case LS_DIALOG_YESNOCANCEL:
        type = MB_YESNOCANCEL;
        break;
    case LS_DIALOG_YESNO:
        type = MB_YESNO;
        break;
    case LS_DIALOG_RETRYCANCEL:
        type = MB_RETRYCANCEL;
        break;
    case LS_DIALOG_CANCELTRYCONTINUE:
        type = MB_CANCELTRYCONTINUE;
        break;
    default:
        return -1;
    }

    switch (flags & LS_DIALOG_ICON_MASK)
    {
    case 0:
        break;
    case LS_DIALOG_ERROR:
        type |= MB_ICONHAND;
        break;
    case LS_DIALOG_QUESTION:
        type |= MB_ICONQUESTION;
        break;
    case LS_DIALOG_WARNING:
        type |= MB_ICONEXCLAMATION;
        break;
    case LS_DIALOG_INFORMATION:
        type |= MB_ICONASTERISK;
        break;
    default:
        return -1;
    }

    if (title)
    {
        lpTitle = ls_utf8_to_wchar(title);
        if (!lpTitle)
            return -1;
    }
    else
        lpTitle = NULL;

    if (message)
    {
        lpMessage = ls_utf8_to_wchar(message);
        if (!lpMessage)
        {
            if (lpTitle)
                ls_free(lpTitle);
            return -1;
        }
    }
    else
        lpMessage = NULL;

    r = MessageBoxW(parent, lpMessage, lpTitle, type);

    if (lpMessage)
        ls_free(lpMessage);

    if (lpTitle)
        ls_free(lpTitle);

    if (r == 0)
        return -1;

    switch (r)
    {
    case IDOK: return LS_CMD_OK;
    case IDCANCEL: return LS_CMD_CANCEL;
    case IDABORT: return LS_CMD_ABORT;
    case IDRETRY: return LS_CMD_RETRY;
    case IDIGNORE: return LS_CMD_IGNORE;
    case IDYES: return LS_CMD_YES;
    case IDNO: return LS_CMD_NO;
    case IDCLOSE: return LS_CMD_CLOSE;
    case IDHELP: return LS_CMD_HELP;
    case IDTRYAGAIN: return LS_CMD_TRYAGAIN;
    case IDCONTINUE: return LS_CMD_CONTINUE;
    }

    return 0;
#endif // LS_WINDOWS
}

int ls_dialog_input(void *parent, const char *title, const char *message, char *buffer, size_t size, int flags)
{
#if LS_WINDOWS
    return -1;
#endif // LS_WINDOWS
}

struct open_dialog_results
{
    char *head;
    char *current;
    size_t size;
};

static void LS_CLASS_FN open_dialog_results_dtor(struct open_dialog_results *results)
{
    if (results->head)
        ls_free(results->head);
}

static struct ls_class OpenDialogResultsClass = {
	.type = LS_FILE_OPEN_DIALOG_RESULTS,
	.cb = sizeof(struct open_dialog_results),
	.dtor = &open_dialog_results_dtor,
	.wait = NULL
};

static int add_result(struct open_dialog_results *results, const char *path)
{
    size_t size;
    char *new_head;

    size = strlen(path) + 1;
    new_head = ls_realloc(results->head, results->size + size);
    if (!new_head)
		return -1;

    results->head = new_head;
	memcpy(results->head + results->size - 1, path, size);
	results->size += size;
    results->head[results->size - 1] = 0; // double null-terminated

	return 0;
}

static size_t filter_array_len( const file_filter_t *filters )
{
    size_t len = 0;

    if (filters == NULL)
        return 0;

    while (filters[len].name)
        len++;
    return len;
}

#if LS_WINDOWS

// Adapted from https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winui/shell/appplatform/commonfiledialog/CommonFileDialogApp.cpp

typedef struct CDialogEventHandler
{
    struct CDialogEventHandlerVtbl *lpVtbl;
    long cRef;
} CDialogEventHandler;

typedef struct CDialogEventHandlerVtbl
{
    BEGIN_INTERFACE

    DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(__RPC__in CDialogEventHandler *This, __RPC__in REFIID riid, _COM_Outptr_ void **ppvObject);

    DECLSPEC_XFGVIRT(IUnknown, AddRef)
    ULONG(STDMETHODCALLTYPE *AddRef)(__RPC__in CDialogEventHandler *This);

    DECLSPEC_XFGVIRT(IUnknown, Release)
    ULONG(STDMETHODCALLTYPE *Release)(__RPC__in CDialogEventHandler *This);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnFileOk)
    HRESULT(STDMETHODCALLTYPE *OnFileOk)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnFolderChanging)
    HRESULT(STDMETHODCALLTYPE *OnFolderChanging)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd, __RPC__in_opt IShellItem *psiFolder);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnFolderChange)
    HRESULT(STDMETHODCALLTYPE *OnFolderChange)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnSelectionChange)
    HRESULT(STDMETHODCALLTYPE *OnSelectionChange)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnShareViolation)
    HRESULT(STDMETHODCALLTYPE *OnShareViolation)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd, __RPC__in_opt IShellItem *psi, __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnTypeChange)
    HRESULT(STDMETHODCALLTYPE *OnTypeChange)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd);

    DECLSPEC_XFGVIRT(IFileDialogEvents, OnOverwrite)
    HRESULT(STDMETHODCALLTYPE *OnOverwrite)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialog *pfd, __RPC__in_opt IShellItem *psi, __RPC__out FDE_OVERWRITE_RESPONSE *pResponse);

    DECLSPEC_XFGVIRT(IFileDialogControlEvents, OnItemSelected)
    HRESULT(STDMETHODCALLTYPE *OnItemSelected)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem);

    DECLSPEC_XFGVIRT(IFileDialogControlEvents, OnButtonClicked)
    HRESULT(STDMETHODCALLTYPE *OnButtonClicked)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialogCustomize *pfdc, DWORD dwIDCtl);

    DECLSPEC_XFGVIRT(IFileDialogControlEvents, OnCheckButtonToggled)
    HRESULT(STDMETHODCALLTYPE *OnCheckButtonToggled)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialogCustomize *pfdc, DWORD dwIDCtl, BOOL bChecked);

    DECLSPEC_XFGVIRT(IFileDialogControlEvents, OnControlActivating)
    HRESULT(STDMETHODCALLTYPE *OnControlActivating)(__RPC__in CDialogEventHandler *This, __RPC__in_opt IFileDialogCustomize *pfdc, DWORD dwIDCtl);

    END_INTERFACE
} CDialogEventHandlerVtbl;

static IFACEMETHODIMP CDialogEventHandler_dtor(CDialogEventHandler *This)
{
    CoTaskMemFree(This->lpVtbl);
    return S_OK;
}

static IFACEMETHODIMP CDialogEventHandler_QueryInterface(CDialogEventHandler *This, REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(CDialogEventHandler, IFileDialogEvents),
        QITABENT(CDialogEventHandler, IFileDialogControlEvents),
        { 0 }
#pragma warning(suppress:4838)
    };
    return QISearch(This, qit, riid, ppv);
}

static IFACEMETHODIMP_(ULONG) CDialogEventHandler_AddRef(CDialogEventHandler *This)
{
    return InterlockedIncrement(&This->cRef);
}

static IFACEMETHODIMP_(ULONG) CDialogEventHandler_Release(CDialogEventHandler *This)
{
    ULONG cRef = InterlockedDecrement(&This->cRef);
    if (cRef == 0)
    {
        (void)CDialogEventHandler_dtor(This);
        CoTaskMemFree(This);
    }
    return cRef;
}

// IFileDialogEvents
static IFACEMETHODIMP CDialogEventHandler_OnFileOk(CDialogEventHandler *This, IFileDialog *pfd) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnFolderChanging(CDialogEventHandler *This, IFileDialog *pfd, IShellItem *psiFolder) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnFolderChange(CDialogEventHandler *This, IFileDialog *pfd) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnSelectionChange(CDialogEventHandler *This, IFileDialog *pfd) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnShareViolation(CDialogEventHandler *This, IFileDialog *pfd, IShellItem *psi, FDE_SHAREVIOLATION_RESPONSE *pResponse) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnTypeChange(CDialogEventHandler *This, IFileDialog *pfd) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnOverwrite(CDialogEventHandler *This, IFileDialog *pfd, IShellItem *psi, FDE_OVERWRITE_RESPONSE *pResponse) { return S_OK; }

// IFileDialogControlEvents
static IFACEMETHODIMP CDialogEventHandler_OnItemSelected(CDialogEventHandler *This, IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnButtonClicked(CDialogEventHandler *This, IFileDialogCustomize *pfdc, DWORD dwIDCtl) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnCheckButtonToggled(CDialogEventHandler *This, IFileDialogCustomize *pfdc, DWORD dwIDCtl, BOOL bChecked) { return S_OK; }
static IFACEMETHODIMP CDialogEventHandler_OnControlActivating(CDialogEventHandler *This, IFileDialogCustomize *pfdc, DWORD dwIDCtl) { return S_OK; }

static IFACEMETHODIMP CDialogEventHandler_ctor(CDialogEventHandler *This)
{
    This->lpVtbl = (CDialogEventHandlerVtbl *)CoTaskMemAlloc(sizeof(CDialogEventHandlerVtbl));
    if (!This->lpVtbl)
        return E_OUTOFMEMORY;

    This->lpVtbl->QueryInterface = CDialogEventHandler_QueryInterface;
    This->lpVtbl->AddRef = CDialogEventHandler_AddRef;
    This->lpVtbl->Release = CDialogEventHandler_Release;

    This->lpVtbl->OnFileOk = CDialogEventHandler_OnFileOk;
    This->lpVtbl->OnFolderChanging = CDialogEventHandler_OnFolderChanging;
    This->lpVtbl->OnFolderChange = CDialogEventHandler_OnFolderChange;
    This->lpVtbl->OnSelectionChange = CDialogEventHandler_OnSelectionChange;
    This->lpVtbl->OnShareViolation = CDialogEventHandler_OnShareViolation;
    This->lpVtbl->OnTypeChange = CDialogEventHandler_OnTypeChange;
    This->lpVtbl->OnOverwrite = CDialogEventHandler_OnOverwrite;

    This->lpVtbl->OnItemSelected = CDialogEventHandler_OnItemSelected;
    This->lpVtbl->OnButtonClicked = CDialogEventHandler_OnButtonClicked;
    This->lpVtbl->OnCheckButtonToggled = CDialogEventHandler_OnCheckButtonToggled;
    This->lpVtbl->OnControlActivating = CDialogEventHandler_OnControlActivating;
    
    This->cRef = 1;

    return S_OK;
}

static HRESULT CDialogEventHandler_new(CDialogEventHandler **ppde)
{
    HRESULT hr;
    CDialogEventHandler *pde;

    *ppde = NULL;
    
    pde = CoTaskMemAlloc(sizeof(CDialogEventHandler));
    if (!pde)
        return E_OUTOFMEMORY;

    hr = CDialogEventHandler_ctor(pde);
    if (!SUCCEEDED(hr))
    {
        CoTaskMemFree(pde);
        return hr;
    }

    *ppde = pde;

    return S_OK;
}

static HRESULT CDialogEventHandler_CreateInstance(REFIID rrid, void **ppv)
{
    HRESULT hr;
    CDialogEventHandler *pde;

    *ppv = NULL;

    hr = CDialogEventHandler_new(&pde);
    if (SUCCEEDED(hr))
    {
        hr = pde->lpVtbl->QueryInterface(pde, rrid, ppv);
        pde->lpVtbl->Release(pde);
    }

    return hr;
}

static HRESULT to_filter_spec(const file_filter_t *filters, COMDLG_FILTERSPEC *lpFilterSpecs, size_t count)
{
    size_t i;

    ZeroMemory(lpFilterSpecs, count * sizeof(COMDLG_FILTERSPEC));
    for (i = 0; i < count; i++)
    {
        lpFilterSpecs[i].pszName = ls_utf8_to_wchar(filters[i].name);
        if (!lpFilterSpecs[i].pszName)
            goto out_of_memory;

        lpFilterSpecs[i].pszSpec = ls_utf8_to_wchar(filters[i].pattern);
        if (!lpFilterSpecs[i].pszSpec)
            goto out_of_memory;
    }

    return S_OK;

out_of_memory:
    for (; i != -1; i--)
    {
        ls_free((void *)lpFilterSpecs[i].pszName);
        ls_free((void *)lpFilterSpecs[i].pszSpec);
    }

    return E_OUTOFMEMORY;
}

struct win32_file_open_info
{
    // input

    HWND hwnd;
    COMDLG_FILTERSPEC *lpFileTypes; // allocated with CoTaskMemAlloc
    UINT cFileTypes;
    int flags;

    DWORD dwOptions;
    
    // output

    struct open_dialog_results *results;

    // internal

    IFileOpenDialog *pfd;
    IFileDialogEvents *pfde;
    IFileDialogControlEvents *pfce;
    DWORD dwCookie;
};

static HRESULT ls_init_file_open_info(void *parent, const file_filter_t *filters, int flags, struct win32_file_open_info *lpFoi)
{
    HRESULT hr;

    ZeroMemory(lpFoi, sizeof(*lpFoi));

    lpFoi->hwnd = parent;

    // convert filters to COMDLG_FILTERSPEC array
    lpFoi->cFileTypes = (UINT)filter_array_len(filters);
    lpFoi->lpFileTypes = CoTaskMemAlloc(lpFoi->cFileTypes * sizeof(COMDLG_FILTERSPEC));
    if (!lpFoi->lpFileTypes)
        return E_OUTOFMEMORY;

    hr = to_filter_spec(filters, lpFoi->lpFileTypes, lpFoi->cFileTypes);
    if (!SUCCEEDED(hr))
    {
        CoTaskMemFree(lpFoi->lpFileTypes);
        return hr;
    }

    lpFoi->dwOptions = FOS_FORCEFILESYSTEM;

    if (flags & LS_FILE_DIALOG_DIR)
		lpFoi->dwOptions |= FOS_PICKFOLDERS;

    if (flags & LS_FILE_DIALOG_MUST_EXIST)
        lpFoi->dwOptions |= FOS_FILEMUSTEXIST;

    if (flags & LS_FILE_DIALOG_MULTI)
        lpFoi->dwOptions |= FOS_ALLOWMULTISELECT;

    // create the file dialog
    hr = CoCreateInstance(
        &CLSID_FileOpenDialog,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IFileOpenDialog,
        (void **)&lpFoi->pfd);
        
    if (!SUCCEEDED(hr))
    {
        CoTaskMemFree(lpFoi->lpFileTypes);
        return hr;
    }

    // create the event handler
    hr = CDialogEventHandler_CreateInstance(&IID_IFileDialogEvents, (void **)&lpFoi->pfde);
    if (!SUCCEEDED(hr))
    {
        lpFoi->pfd->lpVtbl->Release(lpFoi->pfd);
        CoTaskMemFree(lpFoi->lpFileTypes);
        return hr;
    }

    // assign the event handler to the file dialog
    hr = lpFoi->pfd->lpVtbl->Advise(lpFoi->pfd, lpFoi->pfde, &lpFoi->dwCookie);
    if (!SUCCEEDED(hr))
    {
        lpFoi->pfde->lpVtbl->Release(lpFoi->pfde);
        lpFoi->pfd->lpVtbl->Release(lpFoi->pfd);
        CoTaskMemFree(lpFoi->lpFileTypes);
        return hr;
    }

    return S_OK;
}

static HRESULT ls_release_file_open_info(struct win32_file_open_info *lpFoi)
{
    if (lpFoi->dwCookie)
        lpFoi->pfd->lpVtbl->Unadvise(lpFoi->pfd, lpFoi->dwCookie);

    if (lpFoi->pfde)
        lpFoi->pfde->lpVtbl->Release(lpFoi->pfde);

    if (lpFoi->pfd)
        lpFoi->pfd->lpVtbl->Release(lpFoi->pfd);

    if (lpFoi->lpFileTypes)
        CoTaskMemFree(lpFoi->lpFileTypes);

    return S_OK;
}

static HRESULT ls_dialog_open_win32(struct win32_file_open_info *foi)
{
    HRESULT hr;
    DWORD dwFlags;
    LPWSTR lpszFilePath;
    CHAR szFilePath[MAX_PATH];
    DWORD dwNumItems;
    DWORD i;
    int rc;
    IShellItem *psiResult;
    IShellItemArray *psiaResults;

    hr = foi->pfd->lpVtbl->GetOptions(foi->pfd, &dwFlags);
    if (!SUCCEEDED(hr))
        return hr;

    dwFlags |= foi->dwOptions;
    hr = foi->pfd->lpVtbl->SetOptions(foi->pfd, dwFlags);
    if (!SUCCEEDED(hr))
        return hr;

    // set the allowed file types
    if (foi->cFileTypes > 0)
    {
        hr = foi->pfd->lpVtbl->SetFileTypes(foi->pfd, foi->cFileTypes, foi->lpFileTypes);
        if (!SUCCEEDED(hr))
            return hr;
    }

    // show the dialog and wait for it to close
    hr = foi->pfd->lpVtbl->Show(foi->pfd, foi->hwnd);
    if (!SUCCEEDED(hr))
        return hr;

    foi->results = ls_handle_create(&OpenDialogResultsClass);
    if (!foi->results)
		return E_OUTOFMEMORY;

    foi->results->head = ls_malloc(1);
    if (!foi->results->head)
    {
        ls_close(foi->results), foi->results = NULL;
        return E_OUTOFMEMORY;
    }

    foi->results->head[0] = 0;
    foi->results->size = 1;

    // get the result
    if (dwFlags & FOS_ALLOWMULTISELECT)
    {
        hr = foi->pfd->lpVtbl->GetResults(foi->pfd, &psiaResults);
        if (!SUCCEEDED(hr))
        {
            ls_close(foi->results), foi->results = NULL;
			return hr;
        }

        hr = psiaResults->lpVtbl->GetCount(psiaResults, &dwNumItems);
        if (!SUCCEEDED(hr))
        {
			psiaResults->lpVtbl->Release(psiaResults);
            ls_close(foi->results), foi->results = NULL;
			return hr;
        }

       
        for (i = 0; i < dwNumItems; i++)
        {
			hr = psiaResults->lpVtbl->GetItemAt(psiaResults, i, &psiResult);
			if (!SUCCEEDED(hr))
			{
                psiaResults->lpVtbl->Release(psiaResults);
                ls_close(foi->results), foi->results = NULL;
                return hr;
            }

            hr = psiResult->lpVtbl->GetDisplayName(psiResult, SIGDN_FILESYSPATH, &lpszFilePath);

            psiResult->lpVtbl->Release(psiResult);

            if (!SUCCEEDED(hr))
			{
				psiaResults->lpVtbl->Release(psiaResults);
                ls_close(foi->results), foi->results = NULL;
				return hr;
			}

            ls_wchar_to_utf8_buf(lpszFilePath, szFilePath, sizeof(szFilePath));

            rc = add_result(foi->results, szFilePath);

            CoTaskMemFree(lpszFilePath);

            if (rc == -1)
            {
                psiaResults->lpVtbl->Release(psiaResults);
				ls_close(foi->results), foi->results = NULL;
				return E_OUTOFMEMORY;
            }
        }

        psiaResults->lpVtbl->Release(psiaResults);
    }
    else
    {        
        hr = foi->pfd->lpVtbl->GetResult(foi->pfd, &psiResult);
        if (!SUCCEEDED(hr))
        {
            ls_close(foi->results), foi->results = NULL;
            return hr;
        }

        hr = psiResult->lpVtbl->GetDisplayName(psiResult, SIGDN_FILESYSPATH, &lpszFilePath);
            
        psiResult->lpVtbl->Release(psiResult);        

        ls_wchar_to_utf8_buf(lpszFilePath, szFilePath, sizeof(szFilePath));

        CoTaskMemFree(lpszFilePath);

        rc = add_result(foi->results, szFilePath);
        if (rc == -1)
		{
            ls_close(foi->results), foi->results = NULL;
			return E_OUTOFMEMORY;
		}
    }

    foi->results->current = foi->results->head;

    return hr;
}

#endif

int ls_dialog_open(void *parent, const file_filter_t *filters, int flags, ls_handle *results)
{
#if LS_WINDOWS
    HRESULT hr;
    struct win32_file_open_info foi;

    *results = NULL;

    hr = ls_init_file_open_info(parent, filters, flags, &foi);
    if (!SUCCEEDED(hr))
        return -1;

    hr = ls_dialog_open_win32(&foi);

    if (SUCCEEDED(hr))
        *results = foi.results;

    ls_release_file_open_info(&foi);
    
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		return 1;

    return SUCCEEDED(hr) ? 0 : -1;
#endif // LS_WINDOWS
}

int ls_dialog_save(void *parent, const file_filter_t *filters, int flags, char *filename, size_t size)
{
#if LS_WINDOWS
    return -1;
#endif // LS_WINDOWS
}

const char *ls_dialog_next_file(ls_handle results)
{
    struct open_dialog_results *r = results;
    const char *rs;
    size_t len;

    rs = r->current;
    if (!rs[0])
		return NULL; // end of list

    len = strlen(rs);
    r->current += len + 1;

    return rs;
}
