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


void (*logmsg_func)(int code, const char *message) = NULL;


const char *error_string(int code)
{
	switch (code) {
	case NO_ERROR:
		return "";
	case ERROR_INVAL:
		return "Input Error";
	case ERROR_NOMEM:
		return "Memory Error";
	case ERROR_OS:
		return "OS Error";
	case ERROR_OVERFLOW:
		return "Overflow Error";
	case ERROR_INTERNAL:
		return "Internal Error";
	default:
		return "Error";
	}
}


void logmsg(int code, const char *format, ...)
{
	char msg[LOGMSG_MAX];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, LOGMSG_MAX, format, ap);
	va_end(ap);

	if (logmsg_func) {
		logmsg_func(code, msg);
	} else if (code) {
		fprintf(stderr, "[%s] %s\n", error_string(code), msg);
	} else {
		fprintf(stderr, "%s\n", msg);
	}
}
