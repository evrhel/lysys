#include <lysys/ls_random.h>

#include <math.h>

#include "ls_native.h"

int ls_rand_bytes(void *buf, size_t len)
{
#if LS_WINDOWS
	NTSTATUS nt;
	ULONG cb;

	_ls_errno = 0;

	if (!len)
		return 0;

	// BCryptGenRandom has a limit of 2^32 bytes
#if SIZE_MAX > UINT32_MAX
	cb = (ULONG)(len & UINT32_MAX);
#else
	cb = (ULONG)len;
#endif // SIZE_MAX > UINT32_MAX

	nt = BCryptGenRandom(
		NULL,
		buf,
		cb,
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (!BCRYPT_SUCCESS(nt))
		return ls_set_errno(LS_NOT_SUPPORTED);

#if SIZE_MAX > UINT32_MAX
	// handle the remaining bytes
	len >>= 32;
	if (len)
	{
		nt = BCryptGenRandom(
			NULL,
			(uint8_t *)buf + cb,
			(ULONG)len,
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (!BCRYPT_SUCCESS(nt))
			return ls_set_errno(LS_NOT_SUPPORTED);
	}
#endif // SIZE_MAX > UINT32_MAX

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

// Mantissa bits for a double, including the implicit bit
#define DOUBLE_BITS 53

double ls_rand_double(void)
{
	int rc;
	uint64_t bits;

	rc = ls_rand_bytes(&bits, sizeof(bits));
	if (rc == -1)
		return -1;

	bits = bits & ((1ULL << DOUBLE_BITS) - 1);
	return ldexp((double)bits, -DOUBLE_BITS);
}

// Mantissa bits for a float, including the implicit bit
#define FLOAT_BITS 24

float ls_rand_float(void)
{
	int rc;
	uint32_t bits;

	rc = ls_rand_bytes(&bits, sizeof(bits));
	if (rc == -1)
		return -1;

	bits = bits & ((1U << FLOAT_BITS) - 1);
	return ldexpf((float)bits, -FLOAT_BITS);
}
