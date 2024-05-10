#ifndef _LS_UTIL_H_
#define _LS_UTIL_H_

#include <stdlib.h>
#include <stdint.h>

#define ls_zero_memory(ptr, cb) ((void)memset((ptr), 0, (cb)))

typedef struct map map_t;
typedef struct entry entry_t;

//! \brief Any type
typedef union any
{
    void *ptr;
    const void *cptr;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    float f;
    double d;
} any_t;

#define ANY_PTR(x) ((any_t){ .ptr = (x) })
#define ANY_CPTR(x) ((any_t){ .cptr = (x) })
#define ANY_I32(x) ((any_t){ .i32 = (x) })
#define ANY_U32(x) ((any_t){ .u32 = (x) })
#define ANY_I64(x) ((any_t){ .i64 = (x) })
#define ANY_U64(x) ((any_t){ .u64 = (x) })
#define ANY_F(x) ((any_t){ .f = (x) })
#define ANY_D(x) ((any_t){ .d = (x) })

//! \brief compare function
//!
//! \param key1 Key 1
//! \param key2 Key 2
//!
//! \return 0 if the keys are equal, non-zero otherwise
typedef int (*map_cmp_t)(any_t key1, any_t key2);

typedef void (*map_free_t)(any_t val);
typedef any_t (*map_dup_t)(any_t val);

//! \brief Entry in a map
struct entry
{
    any_t key;          //!< Key, do not modify
    any_t value;        //!< Value
    struct entry *next; //!< Next entry
};

struct map
{
    entry_t *entries;   //!< List of entries
    size_t size;        //!< Number of entries

    map_cmp_t cmp;
    map_free_t key_free;
    map_dup_t key_dup;
    map_free_t value_free;
    map_dup_t value_dup;
};

//! \brief Create a map
//!
//! The map is empty after creation. Iterate through by traversing
//! the entries list.
//!
//! \param cmp Compare function, NULL for pointer comparison
//! \param key_free Key free function, NULL for no free
//! \param key_dup Key duplicate function, NULL for direct assignment
//! \param value_free Value free function, NULL for no free
//! \param value_dup Value duplicate function, NULL for direct
//! assignment
//!
//! \return Map or NULL on failure
map_t *ls_map_create(map_cmp_t cmp, map_free_t key_free,
    map_dup_t key_dup, map_free_t value_free, map_dup_t value_dup);

//! \brief Destroy a map
//!
//! \param map Map
void ls_map_destroy(map_t *map);

//! \brief Clear a map
//!
//! \param map Map
//!
//! \return 0 on success, -1 on failure, and ls_errno is set
int ls_map_clear(map_t *map);

//! \brief Find an entry in the map
//!
//! \param map Map
//! \param key Key
//!
//! \return Entry or NULL if not found. If not found, ls_errno is
//! set to NOT_FOUND.
entry_t *ls_map_find(map_t *map, any_t key);

//! \brief Insert a key-value pair into the map
//!
//! On failure, the map is unchanged.
//!
//! \param map Map
//! \param key Key
//! \param value Value
//!
//! \return A pointer to the new or existing entry, or NULL on failure.
entry_t *ls_map_insert(map_t *map, any_t key, any_t value);

//! \brief Print formatted output to a buffer
//!
//! \param str Destination buffer
//! \param cb Size of the destination buffer, in bytes
//! \param format Format string
//!
//! \return Length of the destination buffer, excluding the null
//! terminator, in bytes, or -1 on error. If str and cb are 0, the
//! required buffer size is returned.
size_t ls_scbprintf(char *str, size_t cb, const char *format, ...);

//! \brief Print formatted output to a buffer
//!
//! \param str Destination buffer
//! \param cb Size of the destination buffer, in bytes
//! \param format Format string
//!
//! \return Length of the destination buffer, excluding the null
//! terminator, in bytes, or -1 on error. If str and cb are 0, the
//! required buffer size is returned.
size_t ls_scbwprintf(wchar_t *str, size_t cb, const wchar_t *format, ...);

//! \brief Copy a string to a buffer
//!     
//! \param dest Destination buffer
//! \param src Source string
//! \param cb Size of the destination buffer
//! 
//! \return Length of the destination buffer, excluding the null
//! terminator, or -1 on error
size_t ls_strcbcpy(char *dest, const char *src, size_t cb);

//! \brief Concatenate a string to a buffer
//! 
//! \param dest Destination buffer
//! \param src Source string
//! \param cb Size of the destination buffer
//! 
//! \return Length of the destination buffer, excluding the null
//! terminator, or -1 on error
size_t ls_strcbcat(char *dest, const char *src, size_t cb);

char ls_tolower(char c);

wchar_t ls_wtolower(wchar_t c);

char ls_toupper(char c);

wchar_t ls_wtoupper(wchar_t c);

void ls_strlower(char *str, size_t len);

void ls_wstrlower(wchar_t *str, size_t len);

void ls_strupper(char *str, size_t len);

void ls_wstrupper(wchar_t *str, size_t len);

#endif // _LS_UTIL_H_
