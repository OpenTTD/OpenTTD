/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file safeguards.h A number of safeguards to prevent using unsafe methods.
 *
 * Unsafe methods are, for example, strndup and strncpy because they may leave the
 * string without a null termination, but also strdup and strndup because they can
 * return nullptr and then all strdups would need to be guarded against.
 */

#ifndef SAFEGUARDS_H
#define SAFEGUARDS_H

/* Use std::vector/std::unique_ptr/new instead. */
#define malloc    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define calloc    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define realloc   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use std::string/std::string_view instead. */
#define strdup    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strndup   SAFEGUARD_DO_NOT_USE_THIS_METHOD

#define strcpy    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strncpy   SAFEGUARD_DO_NOT_USE_THIS_METHOD

#define strcat    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strncat   SAFEGUARD_DO_NOT_USE_THIS_METHOD

#define sprintf   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define snprintf  SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define vsprintf  SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define vsnprintf SAFEGUARD_DO_NOT_USE_THIS_METHOD

#define strcmp SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strncmp SAFEGUARD_DO_NOT_USE_THIS_METHOD
#ifdef strcasecmp
#undef strcasecmp
#endif
#define strcasecmp SAFEGUARD_DO_NOT_USE_THIS_METHOD
#ifdef stricmp
#undef stricmp
#endif
#define stricmp SAFEGUARD_DO_NOT_USE_THIS_METHOD

#define memcmp SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define memcpy SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define memmove SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define memset SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fgets instead. */
#define gets      SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use StringConsumer instead. */
#define strtok    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define sscanf    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define from_string SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use ParseInteger or StringConsumer instead. */
#define atoi      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define atol      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define atoll     SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strtol    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strtoll   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strtoul   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strtoull  SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoi      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stol      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoll     SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoul     SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoull    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoimax   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define stoumax   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fmt::print instead. */
#define printf    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define fprintf   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define puts      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define fputs     SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define putchar   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fmt::format instead */
#define to_string SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use our own templated implementation instead of a macro or function with only one type. */
#ifdef min
#undef min
#endif

/* Use our own templated implementation instead of a macro or function with only one type. */
#ifdef max
#undef max
#endif

/* Use our own templated implementation instead of a macro or function with only one type. */
#ifdef abs
#undef abs
#endif

#if defined(NETWORK_CORE_OS_ABSTRACTION_H) && defined(_WIN32)
/* Use NetworkError::GetLast() instead of errno, or do not (indirectly) include network/core/os_abstraction.h.
 * Winsock does not set errno, but one should rather call WSAGetLastError. NetworkError::GetLast abstracts that away. */
#ifdef errno
#undef errno
#endif
#define errno    SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use NetworkError::AsString() instead of strerror, or do not (indirectly) include network/core/os_abstraction.h.
 * Winsock errors are not handled by strerror, but one should rather call FormatMessage. NetworkError::AsString abstracts that away. */
#define strerror SAFEGUARD_DO_NOT_USE_THIS_METHOD
#endif /* defined(NETWORK_CORE_OS_ABSTRACTION_H) && defined(_WIN32) */

#endif /* SAFEGUARDS_H */
