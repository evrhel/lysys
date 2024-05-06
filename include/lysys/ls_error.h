#ifndef _LS_ERROR_H_
#define _LS_ERROR_H_

// Success
#define LS_SUCCESS 0

// Unknown error
#define LS_UNKNOWN_ERROR 1

// A handle is invalid
#define LS_INVALID_HANDLE 2

// There is insufficient memory to complete the operation
#define LS_OUT_OF_MEMORY 3

// One or more arguments are invalid
#define LS_INVALID_ARGUMENT 4

// An invalid state was detected
#define LS_INVALID_STATE 5

// The object is not waitable
#define LS_NOT_WAITABLE 6

// Access denied
#define LS_ACCESS_DENIED 7

// File not found
#define LS_FILE_NOT_FOUND 8

// Buffer too small, retry with a larger buffer
#define LS_BUFFER_TOO_SMALL 9

// Invalid character encoding
#define LS_INVALID_ENCODING 10

// File sharing violation
#define LS_SHARING_VIOLATION 11

// A value is out of range
#define LS_OUT_OF_RANGE 12

// The operation is not supported
#define LS_NOT_SUPPORTED 13

// The path was not found
#define LS_PATH_NOT_FOUND 14

// The end of the file has been reached
#define LS_END_OF_FILE 15

// The object already exists
#define LS_ALREADY_EXISTS 16

// The object was not found
#define LS_NOT_FOUND 17

// The pipe is broken
#define LS_BAD_PIPE 18

// No more files
#define LS_NO_MORE_FILES 19

// No data is available
#define LS_NO_DATA 20

// The device is not ready to perform the operation
#define LS_NOT_READY 21

// A deadlock was detected
#define LS_DEADLOCK 22

// The operation was interrupted
#define LS_INTERRUPTED 23

// An I/O error occurred
#define LS_IO_ERROR 24

// The disk is full
#define LS_DISK_FULL 25

// The device is busy
#define LS_BUSY 26

// The operation timed out
#define LS_TIMEDOUT 27

// The path is invalid
#define LS_INVALID_PATH 28

// An invalid image was detected
#define LS_INVALID_IMAGE 29

// An operation was canceled
#define LS_CANCELED 30

// An internal error occurred
#define LS_INTERNAL_ERROR 31

// The function is not implemented
#define LS_NOT_IMPLEMENTED 100

#endif // _LS_ERROR_H_
