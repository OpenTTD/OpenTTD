/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file walltime_func.h Functionality related to the time of the clock on your wall. */

#ifndef WALLTIME_FUNC_H
#define WALLTIME_FUNC_H

#include <ctime>

/** Helper for safely converting a std::time_t to a local time std::tm using localtime_s. */
struct LocalTimeToStruct {
	static inline std::tm ToTimeStruct(std::time_t time_since_epoch)
	{
		std::tm time = {};
#ifdef _WIN32
		/* Windows has swapped the parameters around for localtime_s. */
		localtime_s(&time, &time_since_epoch);
#else
		localtime_r(&time_since_epoch, &time);
#endif
		return time;
	}
};

/** Helper for safely converting a std::time_t to a UTC time std::tm using gmtime_s. */
struct UTCTimeToStruct {
	static inline std::tm ToTimeStruct(std::time_t time_since_epoch)
	{
		std::tm time = {};
#ifdef _WIN32
		/* Windows has swapped the parameters around for gmtime_s. */
		gmtime_s(&time, &time_since_epoch);
#else
		gmtime_r(&time_since_epoch, &time);
#endif
		return time;
	}
};

/**
 * Container for wall clock time related functionality not directly provided by C++.
 * @tparam T The type of the time-to-struct implementation class.
 */
template <typename T>
struct Time {
	/**
	 * Format the current time with the given strftime format specifiers.
	 * @param buffer The buffer to write the time string to.
	 * @param last   The last element in the buffer.
	 * @param format The format according to strftime format specifiers.
	 * @return The number of characters that were written to the buffer.
	 */
	static inline size_t Format(char *buffer, const char *last, const char *format) NOACCESS(2) WARN_TIME_FORMAT(3)
	{
		std::tm time_struct = T::ToTimeStruct(time(nullptr));
#ifndef _MSC_VER
		/* GCC bug #39438; unlike for printf where the appropriate attribute prevent the
		 * "format non literal" warning, that does not happen for strftime. Even though
		 * format warnings will be created for invalid strftime formats. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif /* _MSC_VER */
		return strftime(buffer, last - buffer, format, &time_struct);
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif /* _MSC_VER */
	}
};

/** Wall clock time functionality using the local time zone. */
using LocalTime = Time<LocalTimeToStruct>;
/** Wall clock time functionality using the UTC time zone. */
using UTCTime = Time<UTCTimeToStruct>;

#endif
