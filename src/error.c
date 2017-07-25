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

#include <stdarg.h>
#include <stdio.h>
#include "error.h"


void (*corpus_log_func)(int code, const char *message) = NULL;


const char *corpus_error_string(int code)
{
	switch (code) {
	case CORPUS_ERROR_NONE:
		return "";
	case CORPUS_ERROR_INVAL:
		return "Input Error";
	case CORPUS_ERROR_NOMEM:
		return "Memory Error";
	case CORPUS_ERROR_OS:
		return "OS Error";
	case CORPUS_ERROR_OVERFLOW:
		return "Overflow Error";
	case CORPUS_ERROR_DOMAIN:
		return "Domain Error";
	case CORPUS_ERROR_RANGE:
		return "Range Error";
	case CORPUS_ERROR_INTERNAL:
		return "Internal Error";
	default:
		return "Error";
	}
}


void corpus_log(int code, const char *format, ...)
{
	char msg[CORPUS_LOG_MAX];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, CORPUS_LOG_MAX, format, ap);
	va_end(ap);

	if (corpus_log_func) {
		corpus_log_func(code, msg);
	} else if (code) {
		fprintf(stderr, "[%s] %s\n", corpus_error_string(code), msg);
	} else {
		fprintf(stderr, "%s\n", msg);
	}
}
