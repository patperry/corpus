/*
 * Copyright 2017 Patrick O. Perry.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ERROR_H
#define ERROR_H

/**
 * \file errcode.h
 *
 * Error codes and error logging.
 */

/**
 * Maximum log message size, in bytes, including the trailing zero.
 * Longer messages get truncated.
 */
#define LOGMSG_MAX 1024

/**
 * Integer codes for errors and messages.
 */
enum error_code {
	NO_ERROR = 0,	/**< successful result */
	ERROR_INVAL,	/**< invalid input */
	ERROR_NOMEM,	/**< memory allocation failure */
	ERROR_OS,	/**< operating system error */
	ERROR_OVERFLOW,	/**< value is too big for data type */
	ERROR_INTERNAL	/**< internal library error */
};

/**
 * If non-`NULL`, this function gets called to log a message. Otherwise,
 * messages get written to `stderr`. Initialized to `NULL`.
 */
extern void (*logmsg_func)(int code, const char *message);

/**
 * Get a human-readable string representation of an error code.
 *
 * \param code an #error_code value
 *
 * \returns a string describing the error code
 */
const char *error_string(int code);

/**
 * Log a message.
 *
 * \param code the status code, an #error code value
 * \param format a printf-style format string
 */
void logmsg(int code, const char *format, ...)
#if (defined(_WIN32) || defined(_WIN64))
	;
#else
	__attribute__ ((format (printf, 2, 3)));
#endif

#endif /* ERROR_H */