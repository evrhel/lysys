extern "C"
{
#include "ls_media_priv.h"
#include <lysys/ls_core.h>
}

#include <windows.media.control.h>
#include <windows.foundation.h>
#include <wrl.h>

#include <cassert>

using namespace ABI::Windows::Media::Control;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

template <typename TResultInterface>
class BaseAsyncHandler
{
public:
	static void *STDMETHODCALLTYPE operator new(size_t cb)
	{
		return CoTaskMemAlloc(cb);
	}

	static void STDMETHODCALLTYPE operator delete(void *block)
	{
		CoTaskMemFree(block);
	}

	AsyncStatus STDMETHODCALLTYPE GetStatus()
	{
		AsyncStatus status;
		lock_lock(&_lock);
		status = _status;
		lock_unlock(&_lock);
		return status;
	}

	HRESULT STDMETHODCALLTYPE Wait()
	{
		lock_lock(&_lock);
		while (_status == Started)
			cond_wait(&_cond, &_lock, LS_INFINITE);
		lock_unlock(&_lock);

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetResults(TResultInterface *pResults)
	{
		HRESULT hr;

		if (!pResults)
			return E_POINTER;

		lock_lock(&_lock);

		if (_result && _status == Completed)
		{
			hr = _result->QueryInterface(pResults);
			if (FAILED(hr))
			{
				lock_unlock(&_lock);
				return hr;
			}
		}
		else
			*pResults = NULL;

		lock_unlock(&_lock);
		return S_OK;
	}

	BaseAsyncHandler() : _status(Started), _result(nullptr)
	{
		lock_init(&_lock);
		cond_init(&_cond);
	}

	~BaseAsyncHandler()
	{
		assert(_status != Started);

		if (_result)
			_result->Release(), _result = NULL;

		cond_destroy(&_cond);
		lock_destroy(&_lock);
	}
protected:
	AsyncStatus _status;
	TResultInterface _result;
	ls_lock_t _lock;
	ls_cond_t _cond;
};

template <typename TResult, typename TProgress, typename TResultInterface>
class GenericAsyncWithProgressHandler : public BaseAsyncHandler<TResultInterface>, public IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>
{
public:
	static void *STDMETHODCALLTYPE operator new(size_t cb)
	{
		return CoTaskMemAlloc(cb);
	}

	static void STDMETHODCALLTYPE operator delete(void *block)
	{
		CoTaskMemFree(block);
	}

	virtual HRESULT STDMETHODCALLTYPE Invoke(IAsyncOperationWithProgress<TResult, TProgress> *asyncInfo, AsyncStatus status) override
	{
		HRESULT hr;

		lock_lock(&_lock);

		assert(_result == NULL);

		_status = status;

		if (status == Canceled || status == Error)
		{
			cond_signal(&_cond);
			lock_unlock(&_lock);

			return S_OK;
		}

		hr = asyncInfo->GetResults(&_result);
		if (FAILED(hr))
		{
			assert(_result == NULL);
			cond_signal(&_cond);
			lock_unlock(&_lock);

			return hr;
		}

		cond_signal(&_cond);
		lock_unlock(&_lock);

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
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>)))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	GenericAsyncWithProgressHandler() : _refCount(1) {}
private:
	LONG _refCount;
};

template <typename TResult, typename TResultInterface>
class GenericAsyncHandler : public BaseAsyncHandler<TResultInterface>, public IAsyncOperationCompletedHandler<TResult>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Invoke(IAsyncOperation<TResult> *asyncInfo, AsyncStatus status) override
	{
		HRESULT hr;

		lock_lock(&_lock);

		assert(_result == NULL);

		_status = status;

		if (status == Canceled || status == Error)
		{
			cond_signal(&_cond);
			lock_unlock(&_lock);

			return S_OK;
		}

		hr = asyncInfo->GetResults(&_result);
		if (FAILED(hr))
		{
			assert(_result == NULL);
			cond_signal(&_cond);
			lock_unlock(&_lock);

			return hr;
		}

		cond_signal(&_cond);
		lock_unlock(&_lock);

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
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IAsyncOperationCompletedHandler<TResult>)))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	GenericAsyncHandler() : _refCount(1) {}
private:
	LONG _refCount;
};

template <typename TResult, typename TResultInterface>
static HRESULT STDMETHODCALLTYPE Await(IAsyncOperation<TResult> *operation, TResultInterface *pResults)
{
	HRESULT hr;
	GenericAsyncHandler<TResult, TResultInterface> *handler;

	if (!pResults)
		return E_POINTER;

	*pResults = NULL;
	if (!operation)
		return E_POINTER;

	handler = new GenericAsyncHandler<TResult, TResultInterface>();

	hr = operation->put_Completed(handler);
	if (FAILED(hr))
	{
		handler->Release();
		return hr;
	}

	hr = handler->Wait();
	if (FAILED(hr))
	{
		handler->Release();
		return hr;
	}

	hr = handler->GetResults(pResults);
	handler->Release();

	return hr;
}

template <typename TResult, typename TProgress, typename TResultInterface>
static HRESULT STDMETHODCALLTYPE Await(IAsyncOperationWithProgress<TResult, TProgress> *operation, TResultInterface *pResults)
{
	HRESULT hr;
	GenericAsyncWithProgressHandler<TResult, TProgress, TResultInterface> *handler;

	if (!pResults)
		return E_POINTER;

	*pResults = NULL;
	if (!operation)
		return E_POINTER;

	handler = new GenericAsyncWithProgressHandler<TResult, TProgress, TResultInterface>();

	hr = operation->put_Completed(handler);
	if (FAILED(hr))
	{
		handler->Release();
		return hr;
	}

	hr = handler->Wait();
	if (FAILED(hr))
	{
		handler->Release();
		return hr;
	}

	hr = handler->GetResults(pResults);
	handler->Release();

	return hr;
}

static HRESULT STDMETHODCALLTYPE copy_hstring(HSTRING hString, char *buf, UINT32 count)
{
	PCWSTR pcwstr;
	UINT32 length;
	int cch;

	if (!hString || !buf)
		return E_POINTER;

	pcwstr = WindowsGetStringRawBuffer(hString, &length);
	cch = WideCharToMultiByte(
		CP_UTF8,
		0,
		pcwstr,
		length,
		buf,
		count - 1,
		NULL,
		NULL);

	if (!cch)
		return HRESULT_FROM_WIN32(GetLastError());
	buf[cch] = 0;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE populate(IGlobalSystemMediaTransportControlsSessionMediaProperties *props, struct mediaplayer *mp)
{
	HRESULT hr;
	HSTRING hs = NULL;

	if (!props || !mp)
		return E_POINTER;

	ZeroMemory(mp->title, sizeof(mp->title));
	ZeroMemory(mp->artist, sizeof(mp->artist));
	ZeroMemory(mp->album, sizeof(mp->album));

	hr = props->get_Title(&hs);
	if (SUCCEEDED(hr) && hs)
	{
		hr = copy_hstring(hs, mp->title, sizeof(mp->title));
		WindowsDeleteString(hs);

		if (FAILED(hr))
			return hr;
	}

	hr = props->get_Artist(&hs);
	if (SUCCEEDED(hr) && hs)
	{
		hr = copy_hstring(hs, mp->artist, sizeof(mp->artist));
		WindowsDeleteString(hs);

		if (FAILED(hr))
			return hr;
	}

	hr = props->get_AlbumTitle(&hs);
	if (SUCCEEDED(hr) && hs)
	{
		hr = copy_hstring(hs, mp->album, sizeof(mp->album));
		WindowsDeleteString(hs);

		if (FAILED(hr))
			return hr;
	}

	return S_OK;
}

static HRESULT WINAPI is_different(struct mediaplayer *mp, IGlobalSystemMediaTransportControlsSessionMediaProperties *props, bool *is_different)
{
	HRESULT hr;
	HSTRING hs;
	char tmp[256];

	if (!props || !mp || !is_different)
		return E_POINTER;

	hr = props->get_Title(&hs);
	if (FAILED(hr) || !hs)
	{
		*is_different = !!mp->title[0];
		return S_OK;
	}

	ZeroMemory(tmp, sizeof(tmp));
	hr = copy_hstring(hs, tmp, sizeof(tmp));
	WindowsDeleteString(hs);

	if (FAILED(hr))
	{
		*is_different = false;
		return hr;
	}

	*is_different = strncmp(tmp, mp->title, sizeof(tmp));

	return S_OK;
}

static HRESULT WINAPI get_properties(IGlobalSystemMediaTransportControlsSessionManager *manager, IGlobalSystemMediaTransportControlsSessionMediaProperties **pProps)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSession *session;
	IAsyncOperation<GlobalSystemMediaTransportControlsSessionMediaProperties *> *operation;

	if (!manager || !pProps)
		return E_POINTER;

	*pProps = NULL;

	hr = manager->GetCurrentSession(&session);
	if (FAILED(hr))
		return hr;

	if (!session)
		return S_OK;

	hr = session->TryGetMediaPropertiesAsync(&operation);
	session->Release(), session = NULL;

	if (FAILED(hr))
		return hr;

	hr = Await(operation, pProps);
	operation->Release(), operation = NULL;

	return hr;
}

static HRESULT WINAPI read_thumbnail_async(IGlobalSystemMediaTransportControlsSessionMediaProperties *props, IAsyncOperationWithProgress<IBuffer *, UINT32> **out_op)
{
	HRESULT hr;
	IRandomAccessStreamReference *stream_ref;
	IAsyncOperation<IRandomAccessStreamWithContentType *> *stream_operation;
	IUnknown *stream;
	IRandomAccessStream *ras;
	IInputStream *is;
	UINT64 size;
	IBufferFactory *factory;
	IBuffer *buffer;

	if (!props || !out_op)
		return E_POINTER;

	*out_op = NULL;

	hr = props->get_Thumbnail(&stream_ref);
	if (FAILED(hr))
		return hr;

	if (!stream_ref)
		return S_OK;

	hr = stream_ref->OpenReadAsync(&stream_operation);
	stream_ref->Release();

	if (FAILED(hr))
		return hr;

	hr = Await(stream_operation, (IRandomAccessStreamWithContentType **)&stream);
	stream_operation->Release();

	if (FAILED(hr))
		return hr;

	hr = stream->QueryInterface(&ras);
	if (FAILED(hr))
	{
		stream->Release();
		return hr;
	}

	hr = stream->QueryInterface(&is);
	stream->Release();

	if (FAILED(hr))
	{
		ras->Release();
		return hr;
	}

	hr = ras->get_Size(&size);
	ras->Release();

	if (FAILED(hr))
	{
		is->Release();
		return hr;
	}

	hr = GetActivationFactory(
		HStringReference(RuntimeClass_Windows_Storage_Streams_Buffer).Get(),
		&factory);
	if (FAILED(hr))
	{
		is->Release();
		return hr;
	}

	hr = factory->Create(size, &buffer);
	factory->Release();

	if (FAILED(hr))
	{
		is->Release();
		return hr;
	}

	hr = is->ReadAsync(buffer, size, InputStreamOptions::InputStreamOptions_None, out_op);
	buffer->Release();
	is->Release();

	if (FAILED(hr))
		return hr;

	return S_OK;
}

static HRESULT WINAPI complete_thumbnail(struct mediaplayer *mp, IAsyncOperationWithProgress<IBuffer *, UINT32> *thumbnail_op)
{
	HRESULT hr;
	IBuffer *buffer;
	IDataReaderStatics *statics;
	IDataReaderFactory *factory;
	IDataReader *reader;
	UINT32 length;
	void *tmp;

	hr = Await(thumbnail_op, &buffer);
	if (FAILED(hr))
		return hr;

	hr = buffer->get_Length(&length);
	if (FAILED(hr))
	{
		buffer->Release();
		return hr;
	}

	hr = GetActivationFactory(
		HStringReference(RuntimeClass_Windows_Storage_Streams_DataReader).Get(),
		&factory);
	if (FAILED(hr))
	{
		buffer->Release();
		return hr;
	}

	hr = factory->QueryInterface(&statics);
	factory->Release();

	if (FAILED(hr))
	{
		buffer->Release();
		return hr;
	}

	hr = statics->FromBuffer(buffer, &reader);
	statics->Release();
	buffer->Release();

	if (FAILED(hr))
		return hr;

	if (length > mp->art_data_capacity)
	{
		tmp = ls_realloc(mp->art_data, length);
		if (!tmp)
		{
			reader->Release();
			return E_OUTOFMEMORY;
		}

		mp->art_data = tmp;
		mp->art_data_capacity = length;
	}

	mp->art_data_length = length;

	hr = reader->ReadBytes(length, (BYTE *)mp->art_data);
	reader->Release();

	if (FAILED(hr))
		mp->art_data_length = 0;

	return hr;
}

static HRESULT WINAPI media_fetch(struct mediaplayer *mp)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSessionMediaProperties *props;
	IAsyncOperationWithProgress<IBuffer *, UINT32> *thumbnail_op;
	bool different;

	hr = get_properties((IGlobalSystemMediaTransportControlsSessionManager *)mp->session_manager, &props);
	if (FAILED(hr))
		return hr;

	if (!props)
	{
		if (!mp->title[0])
			return S_OK;

		ZeroMemory(mp->title, sizeof(mp->title));
		ZeroMemory(mp->artist, sizeof(mp->artist));
		ZeroMemory(mp->album, sizeof(mp->album));
		mp->art_data_length = 0;
		mp->revision++;

		return S_OK;
	}

	hr = read_thumbnail_async(props, &thumbnail_op);
	if (FAILED(hr))
	{
		props->Release();
		return hr;
	}

	hr = is_different(mp, props, &different);
	if (FAILED(hr))
	{
		if (thumbnail_op) thumbnail_op->Release();
		props->Release();
		return hr;
	}

	if (!different)
	{
		if (thumbnail_op) thumbnail_op->Release();
		props->Release();
		return S_OK;
	}

	hr = populate(props, mp);
	props->Release();

	if (FAILED(hr))
	{
		if (thumbnail_op) thumbnail_op->Release();
		return hr;
	}

	if (thumbnail_op)
	{
		hr = complete_thumbnail(mp, thumbnail_op);
		thumbnail_op->Release();

		if (FAILED(hr))
			return hr;
	}

	mp->revision++;

	return S_OK;
}

static void WINAPI media_fetch_apc(ULONG_PTR Parameter)
{
	HRESULT hr;
	struct mediaplayer *mp = (struct mediaplayer *)Parameter;

	hr = media_fetch(mp);

	ls_lock(&mp->lock);

	mp->status = hresult_to_error(hr);

	if (mp->sema)
	{
		ls_semaphore_signal(mp->sema);
		mp->sema = NULL;
	}

	ls_unlock(&mp->lock);
}


EXTERN_C
int ls_media_player_init_WIN32(struct mediaplayer *mp)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSessionManagerStatics *statics;
	IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager *> *async_op;
	IActivationFactory *factory;

	hr = RoGetActivationFactory(
		HStringReference(RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager).Get(),
		IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		ls_set_errno_hresult(hr);

	hr = factory->QueryInterface(IID_PPV_ARGS(&statics));
	factory->Release();

	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = statics->RequestAsync(&async_op);
	statics->Release();
	
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = Await(async_op, (IGlobalSystemMediaTransportControlsSessionManager **)&mp->session_manager);
	async_op->Release();

	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	return 0;
}

EXTERN_C
void ls_media_player_deinit_WIN32(struct mediaplayer *mp)
{
	IGlobalSystemMediaTransportControlsSessionManager *session_manager = (IGlobalSystemMediaTransportControlsSessionManager *)mp->session_manager;

	session_manager->Release();
}

EXTERN_C
int ls_media_player_poll_WIN32(struct mediaplayer *mp, ls_handle sema)
{
	DWORD dwError;

	lock_lock(&mp->lock);

	if (mp->status == LS_BUSY)
	{
		lock_unlock(&mp->lock);
		return ls_set_errno(LS_BUSY);
	}

	mp->sema = sema;

	if (!QueueUserAPC(&media_fetch_apc, GetCurrentThread(), (ULONG_PTR)mp))
	{
		dwError = GetLastError();
		ls_unlock(&mp->lock);
		return ls_set_errno_win32(dwError);
	}

	mp->status = LS_BUSY;

	lock_unlock(&mp->lock);
	return 0;
}

class MediaSubscription : public ITypedEventHandler<GlobalSystemMediaTransportControlsSession *, MediaPropertiesChangedEventArgs *>
{
public:
	static void *STDMETHODCALLTYPE operator new(size_t cb)
	{
		return CoTaskMemAlloc(cb);
	}

	static void STDMETHODCALLTYPE operator delete(void *block)
	{
		CoTaskMemFree(block);
	}

	virtual HRESULT STDMETHODCALLTYPE Invoke(IGlobalSystemMediaTransportControlsSession *sender, IMediaPropertiesChangedEventArgs *args)
	{
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
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(ITypedEventHandler<GlobalSystemMediaTransportControlsSession *, MediaPropertiesChangedEventArgs *>)))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	MediaSubscription(struct mediaplayer *mp, ls_handle sema) :
		_refCount(1), _mp(mp), _sema(sema) {}

	~MediaSubscription()
	{
		if (_sema)
			ls_semaphore_signal(_sema);
	}
private:
	LONG _refCount;
	struct mediaplayer *_mp;
	ls_handle _sema;
};

EXTERN_C
ls_atom ls_media_player_subscribe_WIN32(struct mediaplayer *mp, ls_handle sema)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSessionManager *session_manager;
	IGlobalSystemMediaTransportControlsSession *session;
	MediaSubscription *handler;
	EventRegistrationToken token;

	session_manager = (IGlobalSystemMediaTransportControlsSessionManager *)mp->session_manager;

	hr = session_manager->GetCurrentSession(&session);
	if (FAILED(hr))
	{
		ls_set_errno_hresult(hr);
		return 0;
	}

	handler = new MediaSubscription(mp, sema);

	hr = session->add_MediaPropertiesChanged(handler, &token);
	handler->Release();
	session->Release();

	ls_set_errno_hresult(hr);
	return token.value;
}

EXTERN_C
int ls_media_player_unsubscribe_WIN32(struct mediaplayer *mp, ls_atom atom)
{
	HRESULT hr;
	IGlobalSystemMediaTransportControlsSessionManager *session_manager;
	IGlobalSystemMediaTransportControlsSession *session;
	EventRegistrationToken token;

	session_manager = (IGlobalSystemMediaTransportControlsSessionManager *)mp->session_manager;

	hr = session_manager->GetCurrentSession(&session);
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	token.value = atom;
	hr = session->remove_MediaPropertiesChanged(token);
	session->Release();

	return ls_set_errno_hresult(hr);
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
