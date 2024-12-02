extern "C"
{
#include "ls_media_priv.h"
#include <lysys/ls_core.h>
}

#include <windows.media.control.h>
#include <windows.foundation.h>
#include <wrl.h>

using namespace ABI::Windows::Media::Control;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

static void copy_hstring(HSTRING hString, char *buf, UINT32 count)
{
	PCWSTR pcwstr;
	UINT32 length;

	pcwstr = WindowsGetStringRawBuffer(hString, &length);
	if (!pcwstr)
		return;

	(void)WideCharToMultiByte(
		CP_UTF8,
		0,
		pcwstr,
		length,
		buf,
		count,
		NULL,
		NULL);
}

static void populate(IGlobalSystemMediaTransportControlsSessionMediaProperties *props, struct mediaplayer *mp)
{
	HRESULT hr;
	HSTRING hs;

	ZeroMemory(mp->title, sizeof(mp->title));
	ZeroMemory(mp->artist, sizeof(mp->artist));
	ZeroMemory(mp->album, sizeof(mp->album));

	hr = props->get_Title(&hs);
	if (SUCCEEDED(hr) && hs)
		copy_hstring(hs, mp->title, sizeof(mp->title));

	hr = props->get_Artist(&hs);
	if (SUCCEEDED(hr) && hs)
		copy_hstring(hs, mp->artist, sizeof(mp->artist));

	hr = props->get_AlbumTitle(&hs);
	if (SUCCEEDED(hr) && hs)
		copy_hstring(hs, mp->album, sizeof(mp->album));

	mp->revision++;
}

class GlobalSystemMediaTransportControlsSessionMediaPropertiesAsyncHandler : public IAsyncOperationCompletedHandler<GlobalSystemMediaTransportControlsSessionMediaProperties *>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Invoke(IAsyncOperation<GlobalSystemMediaTransportControlsSessionMediaProperties *> *asyncInfo, AsyncStatus status) override
	{
		HRESULT hr;
		IGlobalSystemMediaTransportControlsSessionMediaProperties *props;

		if (status == Canceled || status == Error)
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return S_OK;
		}

		if (status != Completed)
			return S_OK;

		hr = asyncInfo->GetResults(&props);
		if (FAILED(hr))
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return hr;
		}

		populate(props, _mp);

		if (_sema)
			ls_semaphore_signal(_sema);
		return S_OK;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		LONG ref;

		ref = InterlockedDecrement(&_refCount);
		if (ref == 0)
			delete this;

		return ref;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
	{
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IAsyncOperationCompletedHandler<GlobalSystemMediaTransportControlsSessionMediaProperties *>)))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	GlobalSystemMediaTransportControlsSessionMediaPropertiesAsyncHandler(struct mediaplayer *mp, ls_handle sema) :
		_mp(mp), _sema(sema), _refCount(1) {}

	virtual ~GlobalSystemMediaTransportControlsSessionMediaPropertiesAsyncHandler() = default;
private:
	struct mediaplayer *_mp;
	ls_handle _sema;
	LONG _refCount;
};

class GlobalSystemMediaTransportControlsSessionManagerAsyncHandler : public IAsyncOperationCompletedHandler<GlobalSystemMediaTransportControlsSessionManager *>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Invoke(IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager *> *asyncInfo, AsyncStatus status) override
	{
		HRESULT hr;
		IGlobalSystemMediaTransportControlsSessionManager *manager;
		IGlobalSystemMediaTransportControlsSession *session;
		IAsyncOperation<GlobalSystemMediaTransportControlsSessionMediaProperties *> *operation;
		GlobalSystemMediaTransportControlsSessionMediaPropertiesAsyncHandler *handler;

		if (status == Canceled || status == Error)
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return S_OK;
		}

		if (status != Completed)
			return S_OK;

		hr = asyncInfo->GetResults(&manager);
		if (FAILED(hr))
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return hr;
		}

		hr = manager->GetCurrentSession(&session);
		if (FAILED(hr))
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return hr;
		}

		if (!session)
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return S_OK;
		}

		hr = session->TryGetMediaPropertiesAsync(&operation);
		session->Release();

		if (FAILED(hr))
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return hr;
		}

		handler = new GlobalSystemMediaTransportControlsSessionMediaPropertiesAsyncHandler(_mp, _sema);
		hr = operation->put_Completed(handler);

		handler->Release();
		operation->Release();

		if (FAILED(hr))
		{
			if (_sema)
				ls_semaphore_signal(_sema);
			return hr;
		}

		return S_OK;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		LONG ref;

		ref = InterlockedDecrement(&_refCount);
		if (ref == 0)
			delete this;

		return ref;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
	{
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IAsyncOperationCompletedHandler<GlobalSystemMediaTransportControlsSessionManager *>)))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	GlobalSystemMediaTransportControlsSessionManagerAsyncHandler(struct mediaplayer *mp, ls_handle sema) :
		_mp(mp), _sema(sema), _refCount(1) {}

	virtual ~GlobalSystemMediaTransportControlsSessionManagerAsyncHandler() = default;
private:
	struct mediaplayer *_mp;
	ls_handle _sema;
	LONG _refCount;
};

EXTERN_C
int ls_media_player_poll_WIN32(struct mediaplayer *mp, ls_handle sema)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSessionManagerStatics *statics;
	IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager *> *async_op;
	GlobalSystemMediaTransportControlsSessionManagerAsyncHandler *handler;
	IActivationFactory *factory;

	hr = RoGetActivationFactory(
		HStringReference(RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(),
		IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = factory->QueryInterface(IID_PPV_ARGS(&statics));
	factory->Release();

	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = statics->RequestAsync(&async_op);
	if (FAILED(hr))
	{
		statics->Release();
		return ls_set_errno_hresult(hr);
	}

	statics->Release();

	handler = new GlobalSystemMediaTransportControlsSessionManagerAsyncHandler(mp, sema);
	hr = async_op->put_Completed(handler);

	handler->Release();
	async_op->Release();

	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	return 0;
}

EXTERN_C
int ls_media_player_send_command_WIN32(struct mediaplayer *mp, int cname)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

EXTERN_C
DWORD ls_media_player_getpid_WIN32(struct mediaplayer *mp)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

EXTERN_C
int ls_media_player_cache_artwork_WIN32(struct mediaplayer *mp)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

EXTERN_C
int ls_media_player_publish_WIN32(struct mediaplayer *mp, ls_handle sema)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

EXTERN_C
int ls_media_player_setvolume_WIN32(struct mediaplayer *mp, double volume)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

EXTERN_C
double ls_media_player_getvolume_WIN32(struct mediaplayer *mp)
{
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return 0.0;
}
