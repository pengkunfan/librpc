/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIBRPC_OBJECT_H
#define LIBRPC_OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/types.h>

/**
 * @file object.h
 *
 * Object model (boxed types) API.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_object;

/**
 * Enumerates the possible types of an rpc_object_t.
 */
typedef enum {
	RPC_TYPE_NULL,			/**< null type */
	RPC_TYPE_BOOL,			/**< boolean type */
	RPC_TYPE_UINT64,		/**< unsigned 64-bit integer type */
	RPC_TYPE_INT64,			/**< signed 64-bit integer type */
	RPC_TYPE_DOUBLE,		/**< double precision floating-point type */
	RPC_TYPE_DATE,			/**< date type (represented as 64-bit timestamp */
	RPC_TYPE_STRING,		/**< string type */
	RPC_TYPE_BINARY,		/**< binary data type */
	RPC_TYPE_FD,			/**< file descriptor type */
	RPC_TYPE_DICTIONARY,		/**< dictionary type */
	RPC_TYPE_ARRAY,			/**< array type */
	RPC_TYPE_ERROR,			/**< error type */
#if defined(__linux__)
    	RPC_TYPE_SHMEM,			/**< shared memory type */
#endif
} rpc_type_t;

/**
 * Definition of data object pointer.
 */
typedef struct rpc_object *rpc_object_t;

/**
 * Definition of array applier block type.
 *
 * Body of block is being executed for each of the array's elements, until
 * generator reaches end of the array, or until applier block returns false.
 *
 * @param index Currently processed index.
 * @param value Object stored at currently processed index.
 * @return Continue iteration signal.
 */
typedef bool (^rpc_array_applier_t)(size_t index, rpc_object_t value);

/**
 * Definition of dictionary applier block type.
 *
 * Body of block is being executed for each of the dictionary's elements, until
 * generator reaches end of the dictionary, or until applier block returns
 * false.
 *
 * @param key Currently processed key.
 * @param value Object stored at currently processed key.
 * @return Continue iteration signal.
 */
typedef bool (^rpc_dictionary_applier_t)(const char *key, rpc_object_t value);

/**
 * Definition of array compare block type.
 *
 * Body of block is being executed for pairs of array's members
 * during sort operation.
 *
 * The block should return a negative integer
 * if the first value comes before the second, 0 if they are equal,
 * or a positive integer if the first value comes after the second.
 *
 * @param o1 A value.
 * @param o2 A value to compare with.
 * @return Negative value if a < b ; zero if a = b ; positive value if a > b.
 */
typedef int (^rpc_array_cmp_t)(rpc_object_t o1, rpc_object_t o2);

/**
 * Converts function pointer to an rpc_array_applier_t block type.
 */
#define	RPC_ARRAY_APPLIER(_fn, _arg)					\
	^(size_t _index, rpc_object_t _value) {				\
                return ((bool)_fn(_arg, _index, _value));		\
        }

/**
 * Converts function pointer to an rpc_dictionary_applier_t block type.
 */
#define	RPC_DICTIONARY_APPLIER(_fn, _arg)				\
	^(const char *_key, rpc_object_t _value) {			\
                return ((bool)_fn(_arg, _key, _value));			\
        }

/**
 * Converts function pointer to an rpc_array_cmp_t block type.
 */
#define	RPC_ARRAY_CMP(_fn, _arg)					\
	^(rpc_object_t _o1, rpc_object_t _o2) {				\
                return ((int)_fn(_arg, _o1, _o2));			\
        }

/**
 * Increments reference count of an object.
 *
 * For convenience, the function returns reference to an object passed
 * as the first argument.
 *
 * @param object Object to increment reference count of.
 * @return Same object
 */
rpc_object_t rpc_retain(rpc_object_t object);

/**
 * Decrements reference count of an object.
 *
 * Function returns reference count of an object after operation.
 *
 * @param object Object to decrement reference count of.
 * @return Reference count after operation.
 */
int rpc_release_impl(rpc_object_t object);

/**
 * Gets line number of object location in source file (if any).
 *
 * If the object was read from a file using RPC serializer API,
 * this function might return a line number where the object was located
 * in the source file. Otherwise, it returns 0.
 *
 * @param object Object to get line number from
 * @return A line number in source file or 0
 */
size_t rpc_get_line_number(rpc_object_t object);

/**
 * Gets column number of object location in source file (if any).
 *
 * If the object was read from a file using RPC serializer API,
 * this function might return a column number where the object was located
 * in the source file. Otherwise, it returns 0.
 *
 * @param object Object to get column number from
 * @return A column number in source file or 0
 */
size_t rpc_get_column_number(rpc_object_t object);

/**
 * Creates and returns independent copy of an object.
 *
 * @param object Object to be copied.
 * @return Copy of an provided as the function argument.
 */
rpc_object_t rpc_copy(rpc_object_t object);

/**
 * Compares objects provided as the function arguments
 * and returns the comparison result.
 *
 * The function returns negative value if object passed in first argument
 * is smaller than the object passed in the second argument.
 *
 * The function returns zero if object passed in first argument
 * is equal to the object passed in the second argument.
 *
 * The function returns positive value if object passed in first argument
 * is greater than the object passed in the second argument.
 *
 * @param o1 Object to be compared.
 * @param o2 Object to be compared.
 * @return Negative value - o1 < o2, zero - o1 = o2, positive value - o1 > o2
 */
int rpc_cmp(rpc_object_t o1, rpc_object_t o2);

/**
 * Compares objects provided as the function arguments and returns true
 * if they are equivalent, otherwise returns false.
 *
 * @param o1 Object to be compared.
 * @param o2 Object to be compared.
 * @return Comparison result.
 */
bool rpc_equal(rpc_object_t o1, rpc_object_t o2);

/**
 * Returns numerical hash calculated from the value of an object.
 *
 * @param object Object to be hashed.
 * @return Numerical hash.
 */
size_t rpc_hash(rpc_object_t object);

/**
 * Creates and returns null byte terminated human readable string representation
 * of an object.
 *
 * Caller has to take care of releasing resources held by returned value
 * when it's not needed anymore.
 *
 * @param object Object to be represented as a human readable string.
 * @return String description.
 */
char *rpc_copy_description(rpc_object_t object);

/**
 * Returns the type of an object.
 *
 * @param object Input object.
 * @return Object's type.
 */
rpc_type_t rpc_get_type(rpc_object_t object);

/**
 * Decrements reference count of an object.
 *
 * Also sets it to NULL if as a result of the operation
 * reference count dropped to 0 and the object was freed.
 */
#define	rpc_release(_object)						\
	do {								\
		if (rpc_release_impl(_object) == 0)			\
			_object = NULL;					\
	} while(0)

/**
 * Returns last runtime error reported by the library.
 *
 * @return Error object.
 */
rpc_object_t rpc_get_last_error(void);

/**
 * Deserialize a JSON string to an object compatible with library's data model.
 *
 * @param frame JSON string pointer.
 * @param size JSON string size.
 * @return Deserialized object.
 */
rpc_object_t rpc_object_from_json(const void *frame, size_t size);

/**
 * Serialize an object to a JSON string.
 *
 * @param object Object to be serialized.
 * @param frame Pointer to a serialized JSON string.
 * @param size Size of a serialized JSON string.
 * @return Serialization status. Errors are reported as non-zero values.
 */
int rpc_object_to_json(rpc_object_t object, void **frame, size_t *size);

/**
 * Packs provided values accordingly to a specified format string an into object
 * compatible with library's data model.
 *
 * Each character of the format string represents a single creation step of
 * an output object and may require from 0 to n arguments.
 * Example: [sss] represents an array containing three strings and require
 *     three char * arguments to be passed to the function
 *     after the format string.
 *
 * Format string syntax:
 * - v - Librpc object - args: rpc_object_t object
 * - n - Null object
 * - b - Boolean object - args: bool value
 * - B - Binary object - args: void *buffer, size_t size, bool copy
 * - f - File descriptor object - args: int fd
 * - i - Integer object - args: int value
 * - u - Unsigned integer object - args: unsigned int value
 * - d - Double object - args: double value
 * - s - String object - args: char *string
 * - { - Open dictionary - values inside of dictionary require additional
 *   char *key argument at the beginning of their usual argument list
 * - } - Close dictionary
 * - [ - Open array
 * - ] - Close array
 *
 * @param fmt Format string.
 * @param ... Variable length list of values to be packed.
 * @return Packed object.
 */
rpc_object_t rpc_object_pack(const char *fmt, ...);

/**
 * Packs provided values accordingly to a specified format string an into object
 * compatible with library's data model.
 *
 * The functions acts exactly the same as the rpc_object_pack function,
 * but takes assembled variable arguments list structure as its argument.
 *
 * @param fmt Format string.
 * @param ap Variable arguments list structure.
 * @return Packed object.
 */
rpc_object_t rpc_object_vpack(const char *fmt, va_list ap);

/**
 * Unpacks provided values accordingly to a specified format string from an
 * object.
 *
 * Each character of the format string represents a single unpack step
 * and may require from 0 to n arguments.
 * Example: [sss] represents an array containing three strings and require
 *     three char ** arguments to be passed to the function
 *     after the format string.
 *
 * The function returns the number of successfully processed format characters
 * (excluding '{', '[', ']', '}'), or an error as a negative value.
 *
 * Format string syntax:
 * - * - Array's no-op - skip index
 * - v - Object - args: rpc_object_t *object
 * - b - Boolean object - args: bool *value
 * - f - File descriptor object - args: int *fd
 * - i - Integer object - args: int *value
 * - u - Unsigned integer object - args: unsigned int *value
 * - d - Double object - args: double *value
 * - s - String object - args: char **string
 * - R - Rest of array - returns the rest of an array into separate object -
 *   args: rpc_object_t *object
 * - { - Open dictionary - values inside of dictionary require additional
 *   char *key argument at the beginning of their usual argument list
 * - } - Close dictionary
 * - [ - Open array
 * - ] - Close array
 *
 * @param fmt Format string.
 * @param ... Variable length list of values to be unpacked.
 * @return Unpacking status. Errors are reported as negative values.
 */
int rpc_object_unpack(rpc_object_t, const char *fmt, ...);

/**
 * Unpacks provided values accordingly to a specified format string from an
 * object.
 *
 * The functions acts exactly the same as the rpc_object_unpack function,
 * but takes assembled variable arguments list structure as its argument.
 *
 * @param fmt Format string.
 * @param ap Variable arguments list structure.
 * @return Unpacking status. Errors are reported as negative values.
 */
int rpc_object_vunpack(rpc_object_t, const char *fmt, va_list ap);

/**
 * Creates an object holding null value.
 *
 * @return newly created object
 */
rpc_object_t rpc_null_create(void);

/**
 * Creates an rpc_object_t holding boolean value.
 *
 * @param value Value of the object (true or false).
 * @return Newly created object.
 */
rpc_object_t rpc_bool_create(bool value);

/**
 * Returns a boolean value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_BOOL
 * type, the function returns false.
 *
 * @param xbool Object to read the value from.
 * @return Boolean value of the object (true or false).
 */
bool rpc_bool_get_value(rpc_object_t xbool);

/**
 * Creates an object holding a signed 64-bit integer value.
 *
 * @param value Value of the object (signed 64-bit integer).
 * @return Newly created object.
 */
rpc_object_t rpc_int64_create(int64_t value);

/**
 * Returns an integer value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_INT64
 * type, the function returns -1.
 *
 * @param xint Object to read the value from.
 * @return Integer value of the object.
 */
int64_t rpc_int64_get_value(rpc_object_t xint);

/**
 * Creates an RPC object holding an unsigned 64-bit integer value.
 *
 * @param value Value of the object (unsigned 64-bit integer).
 * @return Newly created object.
 */
rpc_object_t rpc_uint64_create(uint64_t value);

/**
 * Returns an integer value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_UINT64
 * type, the function returns 0.
 *
 * @param xuint Object to read the value from.
 * @return Integer value of the object.
 */
uint64_t rpc_uint64_get_value(rpc_object_t xuint);

/**
 * Creates an RPC object holding a double value.
 *
 * @param value Value of the object (double).
 * @return Newly created object.
 */
rpc_object_t rpc_double_create(double value);

/**
 * Returns a double value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_DOUBLE
 * type, the function returns 0.
 *
 * @param xdouble Object to read the value from.
 * @return Double value of the object.
 */
double rpc_double_get_value(rpc_object_t xdouble);

/**
 * Creates an RPC object holding a date.
 *
 * @param interval Value of the object (signed 64-bit integer).
 * @return Newly created object.
 */
rpc_object_t rpc_date_create(int64_t interval);

/**
 * Creates an RPC object holding a date from current UTC time.
 *
 * @return Newly created object.
 */
rpc_object_t rpc_date_create_from_current(void);

/**
 * Returns an integer value of an object (UNIX timestamp).
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_DATE
 * type, the function returns 0.
 *
 * @param xdate Object to read the value from.
 * @return Integer UNIX timestamp value of the object.
 */
int64_t rpc_date_get_value(rpc_object_t xdate);

/**
 * Creates an RPC object holding a binary data.
 *
 * @param bytes Pointer to data buffer (void pointer).
 * @param length Length of the data buffer.
 * @param copy Copy vs. reference an input data buffer.
 * @return Newly created object.
 */
rpc_object_t rpc_data_create(const void *bytes, size_t length, bool copy);

/**
 * Returns the length of internal binary data buffer of a provided object.
 *
 * @param xdata Input object
 * @return Size of an object's data buffer.
 */
size_t rpc_data_get_length(rpc_object_t xdata);

/**
 * Returns the pointer to internal binary data buffer of a provided object.
 *
 * @param xdata Input object
 * @return Pointer to an object's data buffer (void pointer).
 */
const void *rpc_data_get_bytes_ptr(rpc_object_t xdata);

/**
 * Creates a copy of a slice of a internal data buffer of a provided object.
 *
 * @param xdata Input object.
 * @param buffer Output buffer.
 * @param off Start offset for the copy operation.
 * @param length Requested size of a copy.
 * @return Size of a copied data.
 */
size_t rpc_data_get_bytes(rpc_object_t xdata, void *buffer, size_t off,
    size_t length);

/**
 * Creates an RPC object holding a string.
 *
 * @param string Value of the object (constant character pointer).
 * @return Newly created object.
 */
rpc_object_t rpc_string_create(const char *string);

/**
 * Creates an RPC object holding a string of a specified length.
 *
 * @param string Value of the object (constant character pointer).
 * @param length Length of an input string.
 * @return Newly created object.
 */
rpc_object_t rpc_string_create_len(const char *string, size_t length);

/**
 * Creates an RPC object holding a string formatted using the regular printf
 * function format string and arguments.
 *
 * @param fmt Format string.
 * @param ... Variable length list of input data arguments for formatting.
 * @return Newly created object.
 */
rpc_object_t rpc_string_create_with_format(const char *fmt, ...);

/**
 * Creates an RPC object holding a string formatted using the regular printf
 * function format string and arguments (as variable arguments list type).
 *
 * @param fmt Format string.
 * @param ap Variable arguments list.
 * @return Newly created object.
 */
rpc_object_t rpc_string_create_with_format_and_arguments(const char *fmt,
    va_list ap);

/**
 * Returns the length of internal string buffer of a provided object.
 *
 * @param xstring Input object
 * @return Size of an object's data buffer.
 */
size_t rpc_string_get_length(rpc_object_t xstring);

/**
 * Returns the pointer to internal string data buffer of a provided object.
 *
 * @param xstring Input object
 * @return Pointer to an object's data buffer (constant character pointer).
 */
const char *rpc_string_get_string_ptr(rpc_object_t xstring);

/**
 * Creates an RPC object holding a file descriptor.
 *
 * @param fd Value of the object (signed integer).
 * @return Newly created object.
 */
rpc_object_t rpc_fd_create(int fd);

/**
 * Creates a duplicate of a file descriptor held by a provided object.
 *
 * @param xfd Input object.
 * @return Newly created file descriptor.
 */
int rpc_fd_dup(rpc_object_t xfd);

/**
 * Returns a value of a file descriptor helf by a provided object.
 *
 * @param xfd Input object.
 * @return File descriptor's value.
 */
int rpc_fd_get_value(rpc_object_t xfd);

/**
 * Creates a new, empty array of objects.
 *
 * @return Empty array.
 */
rpc_object_t rpc_array_create(void);

/**
 * Creates a new array of objects, optionally populating it with data.
 *
 * @param objects Array of objects to insert.
 * @param count Number of items in @ref objects.
 * @param steal Reference vs. reference and increment refcount of elements
 *        in @ref objects.
 * @return Newly created object.
 */
rpc_object_t rpc_array_create_ex(const rpc_object_t *objects, size_t count,
    bool steal);

/**
 * Inserts an object to an input array at a given index and increments object's
 * refcount.
 *
 * If an index is bigger than an array itself, gap will be filled
 * with null objects.
 *
 * If an index is already occupied, then a new object takes place of
 * an old object and the refcount of an old object is decremented.
 *
 * If an object is NULL, then this function removes an old object
 * from a given index.
 *
 * @param array Input array.
 * @param value Value to be inserted.
 * @param index Target index for a newly inserted value.
 */
void rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value);

/**
 * Inserts an object to an input array at a given index.
 *
 * If an index is bigger than an array itself, gap will be filled
 * with null objects.
 *
 * If an index is already occupied, then a new object takes place of
 * an old object and the refcount of an old object is decremented.
 *
 * If an object is NULL, then this function removes an old object
 * from a given index.
 *
 * @param array Input array.
 * @param value Value to be inserted.
 * @param index Target index for a newly inserted value.
 */
void rpc_array_steal_value(rpc_object_t array, size_t index, rpc_object_t value);

/**
 * Removes an object from a given index of a provided array.
 *
 * @param array Input array.
 * @param index Index to be removed.
 */
void rpc_array_remove_index(rpc_object_t array, size_t index);

/**
 * Appends an object at the end of a provided array and increments object's
 * refcount.
 *
 * @param array Input array.
 * @param value Object to be appended.
 */
void rpc_array_append_value(rpc_object_t array, rpc_object_t value);

/**
 * Appends an object at the end of a provided array.
 *
 * @param array Input array.
 * @param value Object to be appended.
 */
void rpc_array_append_stolen_value(rpc_object_t array, rpc_object_t value);

/**
 * Returns an object held by a provided array at a given index.
 *
 * If a given index does not exist, then the function returns NULL.
 *
 * @param array Input array.
 * @param index Index of an object to be returned.
 * @return Object at a given index.
 */
rpc_object_t rpc_array_get_value(rpc_object_t array, size_t index);

/**
 * Returns a number of elements in a provided array.
 *
 * @param array Input array.
 * @return Number of elements held by an input array.
 */
size_t rpc_array_get_count(rpc_object_t array);

/**
 * Iterates over a given array. For each of elements executes an applier block,
 * providing a current index and a current value as
 * an applier block's arguments.
 *
 * If an applier returns false, iteration is terminated and the function returns
 * true (terminated). Otherwise the function iterates to the end of
 * an input array and returns false (finished).
 *
 * @param array Input array.
 * @param applier Block of code to be executed for each of an array's elements.
 * @return Iteration terminated (true)/finished (false) boolean flag.
 */
bool rpc_array_apply(rpc_object_t array, rpc_array_applier_t applier);

/**
 * Checks if an entry with the same value as provided
 * in the second argument of the function exists in a given array.
 *
 * The function returns boolean result of that check.
 *
 * @param array Array to be searched.
 * @param value RPC object representing value of a searched object.
 * @return Boolean result of the search operation.
 */
bool rpc_array_contains(rpc_object_t array, rpc_object_t value);

/**
 * Iterates over a given array in reversed order (starting from its end).
 * Besides of that the function is acting exactly the same as rpc_array_apply.
 *
 * @param array Input array.
 * @param applier Block of code to be executed for each of an array's elements.
 * @return Iteration terminated (true)/finished (false) boolean flag.
 */
bool rpc_array_reverse_apply(rpc_object_t array, rpc_array_applier_t applier);

/**
 * Sorts contents of a given array using provided comparator code block.
 *
 * The desired behavior of the comparator block is described in
 * rpc_array_cmp_t type's documentation.
 *
 * @param array Array to be sorted.
 * @param comparator Comparator to be used during sort operation.
 */
void rpc_array_sort(rpc_object_t array, rpc_array_cmp_t comparator);

/**
 * Takes an input array and copies a selected number of its elements,
 * starting from a given index, creating an output array.
 *
 * If a copy size is set to -1, then an output array is being created from
 * input array's elements from a start index to the end of an input array.
 *
 * If during a copy operation, the end of an input array is reached before
 * size requirement is satisfied, then a copy operation is terminated and
 * the function returns an array smaller than a requested size.
 *
 * @param array Input array.
 * @param start Copy start index.
 * @param len Requested copy size.
 * @return Newly created array containing elements copied from an input array.
 */
rpc_object_t rpc_array_slice(rpc_object_t array, size_t start, ssize_t len);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given boolean value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input value (boolean).
 */
void rpc_array_set_bool(rpc_object_t array, size_t index, bool value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given 64-bit signed integer value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input value (64-bit signed integer).
 */
void rpc_array_set_int64(rpc_object_t array, size_t index, int64_t value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given 64-bit unsigned integer value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input value (64-bit unsigned integer).
 */
void rpc_array_set_uint64(rpc_object_t array, size_t index, uint64_t value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given double value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input value (double).
 */
void rpc_array_set_double(rpc_object_t array, size_t index, double value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given UNIX timestamp value represented as an integer.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input value (UNIX timestamp represented as an integer).
 */
void rpc_array_set_date(rpc_object_t array, size_t index, int64_t value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given binary data of a specified length.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param bytes Input binary data buffer.
 * @param length Size of an input data buffer.
 */
void rpc_array_set_data(rpc_object_t array, size_t index, const void *bytes,
    size_t length);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given string value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input string (null byte terminated constant character pointer).
 */
void rpc_array_set_string(rpc_object_t array, size_t index, const char *value);

/**
 * Sets a selected index of an input array to a newly created RPC object holding
 * a given file descriptor value.
 *
 * If a selected index is larger than an input array size, then a gap between
 * a last existing index and a requested index is filled with NULL objects.
 *
 * @param array Input array.
 * @param index Index to store an input value.
 * @param value Input file descriptor (integer).
 */
void rpc_array_set_fd(rpc_object_t array, size_t index, int value);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_BOOL),
 * then the function returns false.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return Boolean value.
 */
bool rpc_array_get_bool(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_INT64),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return 64-bit signed integer value.
 */
int64_t rpc_array_get_int64(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_UINT64),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return 64-bit unsigned integer value.
 */
uint64_t rpc_array_get_uint64(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_DOUBLE),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return Double value.
 */
double rpc_array_get_double(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_DATE),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return Integer representing a UNIX timestamp.
 */
int64_t rpc_array_get_date(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_BINARY),
 * then the function returns NULL.
 *
 * @param array Input array.
 * @param index Output value's index.
 * @param length Size of an output buffer.
 * @return Binary output buffer pointer.
 */
const void *rpc_array_get_data(rpc_object_t array, size_t index,
    size_t *length);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_STRING),
 * then the function returns NULL.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return String value.
 */
const char *rpc_array_get_string(rpc_object_t array, size_t index);

/**
 * Returns a value from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_FD),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index Index storing an output value.
 * @return File descriptor value (integer).
 */
int rpc_array_get_fd(rpc_object_t array, size_t index);

/**
 * Duplicates a file descriptor from a given index of an input array.
 *
 * If a selected index does not exist, or object being held at a given index
 * has a type different from expected (RPC_TYPE_FD),
 * then the function returns 0.
 *
 * @param array Input array.
 * @param index File descriptor's index.
 * @return Duplicated file descriptor (integer).
 */
int rpc_array_dup_fd(rpc_object_t array, size_t index);
#if defined(__linux__)

/**
 * Allocates a chunk of a shared memory of a given size.
 *
 * @param size Size (in bytes) of a shared memory to be allocated.
 * @return Newly created object representing a shared memory.
 */
rpc_object_t rpc_shmem_create(size_t size);

/**
 * Maps a given shared memory object to an actual address.
 *
 * @param shmem Input shared memory object.
 * @return Address a shared memory has been mapped to.
 */
void *rpc_shmem_map(rpc_object_t shmem);

/**
 * Unmaps a given shared memory object from a given address.
 *
 * @param shmem Input shared memory object.
 * @param addr Address to be unmapped.
 */
void rpc_shmem_unmap(rpc_object_t shmem, void *addr);

/**
 * Returns a size of a provided shared memory chunk.
 *
 * @param shmem Object representing a shared memory.
 * @return Size of a shared memory.
 */
size_t rpc_shmem_get_size(rpc_object_t shmem);
#endif

/**
 * Creates an RPC object representing an error condition with automatically
 * extracted stack trace.
 *
 * It can hold data about numerical error code, string describing
 * the actual error in human readable format and extra auxiliary data.
 *
 * Extra is an optional argument and can be safely set to NULL when not needed.
 *
 * @param code Numerical error code.
 * @param msg String representing an actual error description.
 * @param extra Extra data (optional).
 * @return Newly created object.
 */
rpc_object_t rpc_error_create(int code, const char *msg, rpc_object_t extra);

/**
 * Creates an RPC object representing an error condition with externally
 * provided extracted stack trace.
 *
 * It can hold data about numerical error code, string describing
 * the actual error in human readable format and extra auxiliary data.
 *
 * Extra is an optional argument and can be safely set to NULL when not needed.
 *
 * @param code Numerical error code.
 * @param msg String representing an actual error description.
 * @param extra Extra data (optional).
 * @param stack Externally provided stack trace of an error.
 * @return Newly created object.
 */
rpc_object_t rpc_error_create_with_stack(int code, const char *msg,
    rpc_object_t extra, rpc_object_t stack);

/**
 * Returns numerical error code of a provided error object.
 *
 * @param error Input error object.
 * @return Numerical error code.
 */
int rpc_error_get_code(rpc_object_t error);

/**
 * Returns string description of a provided error object.
 *
 * @param error Input error object.
 * @return String description of an error condition.
 */
const char *rpc_error_get_message(rpc_object_t error);

/**
 * Returns an extra (auxiliary data) set during creation of a provided provided
 * error object.
 *
 * @param error Input error object.
 * @return Extra data attached to an error.
 */
rpc_object_t rpc_error_get_extra(rpc_object_t error);

/**
 * Sets an extra data attached to an error.
 *
 * @param error Input error object.
 * @param extra Extra data to be attached to an error.
 */
void rpc_error_set_extra(rpc_object_t error, rpc_object_t extra);

/**
 * Returns a stack trace associated with an error condition of a provided
 * error object.
 *
 * @param error Input error object.
 * @return Stack trace of an error.
 */
rpc_object_t rpc_error_get_stack(rpc_object_t error);

/**
 * Creates a new, empty dictionary of objects.
 *
 * @return Empty dictionary.
 */
rpc_object_t rpc_dictionary_create(void);

/**
 * Creates a new dictionary of objects, optionally populating it with data.
 *
 * @param keys Array of keys associated with values to insert.
 * @param values Array of objects to insert.
 * @param count Number of items in @ref values.
 * @param steal Reference vs. reference and increment refcount of elements
 *        in @ref values.
 * @return Newly created object.
 */
rpc_object_t rpc_dictionary_create_ex(const char *const *keys,
    const rpc_object_t *values, size_t count, bool steal);

/**
 * Inserts an object to an input dictionary at a given key and increments
 * object's refcount.
 *
 * If a key is already occupied, then a new object takes place of
 * an old object and the refcount of an old object is decremented.
 *
 * If an object is NULL, then this function removes an old object
 * stored at a given key.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store a value.
 * @param value Value to be inserted.
 */
void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value);

/**
 * Inserts an object to an input dictionary at a given key.
 *
 * If a key is already occupied, then a new object takes place of
 * an old object and the refcount of an old object is decremented.
 *
 * If an object is NULL, then this function removes an old object
 * stored at a given key.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store a value.
 * @param value Value to be inserted.
 */
void rpc_dictionary_steal_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value);

/**
 * Removes an object from a given key of a provided dictionary.
 *
 * @param dictionary Input dictionary.
 * @param key Key to be removed.
 */
void rpc_dictionary_remove_key(rpc_object_t dictionary, const char *key);

/**
 * Returns an object held by a provided dictionary at a given key.
 *
 * If a given key does not exist, then the function returns NULL.
 *
 * @param dictionary Input dictionary.
 * @param key Key of an object to be returned.
 * @return Object at a given key.
 */
rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
    const char *key);

/**
 * Returns a number of elements in a provided dictionary.
 *
 * @param dictionary Input dictionary.
 * @return Number of elements held by an input dictionary.
 */
size_t rpc_dictionary_get_count(rpc_object_t dictionary);

/**
 * Iterates over a given dictionary. For each of elements executes
 * an applier block, providing a current key and a current value as
 * an applier block's arguments.
 *
 * If an applier returns false, iteration is terminated and the function returns
 * true (terminated). Otherwise the function iterates to the end of
 * an input array and returns false (finished).
 *
 * @param dictionary Input dictionary.
 * @param applier Block to be executed for each of an dictionary's elements.
 * @return Iteration terminated (true)/finished (false) boolean flag.
 */
bool rpc_dictionary_apply(rpc_object_t dictionary,
    rpc_dictionary_applier_t applier);

/**
 * Checks if an input dictionary does have a given key set.
 *
 * @param dictionary Input dictionary.
 * @param key Key to be tested.
 * @return Boolean check result.
 */
bool rpc_dictionary_has_key(rpc_object_t dictionary, const char *key);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given boolean value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input value (boolean).
 */
void rpc_dictionary_set_bool(rpc_object_t dictionary, const char *key,
    bool value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given 64-bit signed integer value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input value (64-bit signed integer).
 */
void rpc_dictionary_set_int64(rpc_object_t dictionary, const char *key,
    int64_t value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given 64-bit unsigned integer value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input value (64-bit unsigned integer).
 */
void rpc_dictionary_set_uint64(rpc_object_t dictionary, const char *key,
    uint64_t value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given double value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input value (double).
 */
void rpc_dictionary_set_double(rpc_object_t dictionary, const char *key,
    double value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given UNIX timestamp value represented as an integer.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input value (UNIX timestamp represented as an integer).
 */
void rpc_dictionary_set_date(rpc_object_t dictionary, const char *key,
    int64_t value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given binary data of a specified length.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input binary data buffer.
 * @param length Size of an input data buffer.
 */
void rpc_dictionary_set_data(rpc_object_t dictionary, const char *key,
    const void *value, size_t length);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given string value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input string (null byte terminated constant character pointer).
 */
void rpc_dictionary_set_string(rpc_object_t dictionary, const char *key,
    const char *value);

/**
 * Sets a selected key of an input dictionary to a newly created RPC object
 * holding a given file descriptor value.
 *
 * @param dictionary Input dictionary.
 * @param key Key to store an input value.
 * @param value Input file descriptor (integer).
 */
void rpc_dictionary_set_fd(rpc_object_t dictionary, const char *key,
    int value);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_BOOL),
 * then the function returns false.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return Boolean value.
 */
bool rpc_dictionary_get_bool(rpc_object_t dictionary, const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_INT64),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return 64-bit signed integer value.
 */
int64_t rpc_dictionary_get_int64(rpc_object_t dictionary, const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_UINT64),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return 64-bit unsigned integer value.
 */
uint64_t rpc_dictionary_get_uint64(rpc_object_t dictionary, const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_DOUBLE),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return Double value.
 */
double rpc_dictionary_get_double(rpc_object_t dictionary, const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_DATE),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return Integer representing a UNIX timestamp.
 */
int64_t rpc_dictionary_get_date(rpc_object_t dictionary, const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_BINARY),
 * then the function returns NULL.
 *
 * @param dictionary Input dictionary.
 * @param key Output value's key.
 * @param length Size of an output buffer.
 * @return Binary output buffer pointer.
 */
const void *rpc_dictionary_get_data(rpc_object_t dictionary, const char *key,
    size_t *length);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_STRING),
 * then the function returns NULL.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return String value.
 */
const char *rpc_dictionary_get_string(rpc_object_t dictionary,
    const char *key);

/**
 * Returns a value from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_FD),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key Key storing an output value.
 * @return File descriptor value (integer).
 */
int rpc_dictionary_get_fd(rpc_object_t dictionary, const char *key);

/**
 * Duplicates a file descriptor from a given key of an input dictionary.
 *
 * If a selected key does not exist, or object being held at a given key
 * has a type different from expected (RPC_TYPE_FD),
 * then the function returns 0.
 *
 * @param dictionary Input dictionary.
 * @param key File descriptor's key.
 * @return Duplicated file descriptor (integer).
 */
int rpc_dictionary_dup_fd(rpc_object_t dictionary, const char *key);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_OBJECT_H
