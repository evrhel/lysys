#include <lysys/ls_random.h>

#include "ls_native.h"

int ls_rand_bytes(void *buf, size_t len)
{
#if LS_WINDOWS
	NTSTATUS nt;

	if (!len)
		return 0;

	nt = BCryptGenRandom(
		NULL,
		buf,
		len,
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (nt != 0)
		return ls_set_errno(LS_NOT_SUPPORTED);

	return 0;
#elif LS_DARWIN
	int status;

	status = SecRandomBytesCopy(kSecRandomDefault, len, buf);
	if (status != errSecSuccess)
		return ls_set_errno(LS_NOT_SUPPORTED);

	return 0;
#else
	size_t count, offset = 0;
	int rc;

	if (!len)
		return 0;

	while (len > 0)
	{
		// max entropy is 256 bytes
		count = len > 256 ? 256 : len;
		rc = getentropy((uint8_t *)buf + offset, count);
		if (rc != 0)
			return ls_set_errno(LS_NOT_SUPPORTED);

		// advance the buffer
		offset += count;
		len -= count;
	}

	return 0;
#endif // LS_WINDOWS
}

uint64_t ls_rand_uint64(void)
{
	int rc;
	uint64_t value;

	_ls_errno = 0;

	rc = ls_rand_bytes(&value, sizeof(value));
	if (rc != 0)
		return -1;

	return value;
}

int ls_rand_int(int min, int max)
{
	int rc;
	int value;
	int range;

	_ls_errno = 0;

	// clamp the range
	if (min > max)
		min = max;

	rc = ls_rand_bytes(&value, sizeof(value));
	if (rc != 0)
		return -1;

	range = max - min + 1;
	return (value % range) + min;
}

double ls_rand_double(void)
{
	int rc;
	uint64_t value;

	_ls_errno = 0;

	rc = ls_rand_bytes(&value, sizeof(value));
	if (rc != 0)
		return -1;


	return (double)value / (double)UINT64_MAX;
}

float ls_rand_float(void)
{
	int rc;
	uint32_t value;

	_ls_errno = 0;

	rc = ls_rand_bytes(&value, sizeof(value));
	if (rc != 0)
		return -1;

	return (float)value / (float)UINT32_MAX;
}
