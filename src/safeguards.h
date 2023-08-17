/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file safeguards.h A number of safeguards to prevent using unsafe methods.
 *
 * Unsafe methods are, for example, strndup and strncpy because they may leave the
 * string without a null termination, but also strdup and strndup because they can
 * return nullptr and then all strdups would need to be guarded against that instead
 * of using the current MallocT/ReallocT/CallocT technique of just giving the user
 * an error that too much memory was used instead of spreading that code though
 * the whole code base.
 */

#ifndef SAFEGUARDS_H
#define SAFEGUARDS_H

/* Use MallocT instead. */
#define malloc    SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use MallocT instead. */
#define calloc    SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use ReallocT instead. */
#define realloc   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use std::string instead. */
#define strdup    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strndup   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use strecpy instead. */
#define strcpy    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strncpy   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use std::string concatenation/fmt::format instead. */
#define strcat    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define strncat   SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fmt::format instead. */
#define sprintf   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define snprintf  SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fmt::format instead. */
#define vsprintf  SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define vsnprintf SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fgets instead. */
#define gets      SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* No clear replacement. */
#define strtok    SAFEGUARD_DO_NOT_USE_THIS_METHOD

/* Use fmt::print instead. */
#define printf    SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define fprintf   SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define puts      SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define fputs     SAFEGUARD_DO_NOT_USE_THIS_METHOD
#define putchar   SAFEGUARD_DO_NOT_USE_THIS_METHOD

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
