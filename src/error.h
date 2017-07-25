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

#ifndef CORPUS_ERROR_H
#define CORPUS_ERROR_H

/**
 * \file error.h
 *
 * Error codes and error logging.
 */

/**
 * Maximum log message size, in bytes, including the trailing zero.
 * Longer messages get truncated.
 */
#define CORPUS_LOG_MAX 1024

/**
 * Integer codes for errors and messages.
 */
enum corpus_error_code {
	CORPUS_ERROR_NONE = 0,	/**< successful result */
	CORPUS_ERROR_INVAL,	/**< invalid input */
	CORPUS_ERROR_NOMEM,	/**< memory allocation failure */
	CORPUS_ERROR_OS,	/**< operating system error */
	CORPUS_ERROR_OVERFLOW,	/**< value is too big for data type */
	CORPUS_ERROR_DOMAIN,	/**< input is outside function's domain */
	CORPUS_ERROR_RANGE,	/**< output is outside data type's range */
	CORPUS_ERROR_INTERNAL	/**< internal library error */
};

/**
 * If non-`NULL`, this function gets called to log a message. Otherwise,
 * messages get written to `stderr`. Initialized to `NULL`.
 */
extern void (*corpus_log_func)(int code, const char *message);

/**
 * Get a human-readable string representation of an error code.
 *
 * \param code a #corpus_error_code value
 *
 * \returns a string describing the error code
 */
const char *corpus_error_string(int code);

/**
 * Log a message.
 *
 * \param code the status code, a #corpus_error_code value
 * \param format a printf-style format string
 */
void corpus_log(int code, const char *format, ...)
#if (defined(_WIN32) || defined(_WIN64))
	;
#else
	__attribute__ ((format (printf, 2, 3)));
#endif

#endif /* CORPUS_ERROR_H */
