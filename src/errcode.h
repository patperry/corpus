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

#ifndef ERRCODE_H
#define ERRCODE_H

/**
 * \file errcode.h
 *
 * Error codes for function return values.
 */

#define NO_ERROR	0	/**< successful result */
#define ERROR_INVAL	1	/**< invalid input */
#define ERROR_NOMEM	2	/**< memory allocation failure */
#define ERROR_OS	3	/**< operating system error */
#define ERROR_OVERFLOW	4	/**< value is too big for data type */
#define ERROR_INTERNAL  5	/**< internal library error */

#endif /* ERRCODE_H */
