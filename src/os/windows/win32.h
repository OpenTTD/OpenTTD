/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32.h declarations of functions for MS windows systems */

#ifndef WIN32_H
#define WIN32_H

#include <windows.h>
bool MyShowCursor(bool show, bool toggle = false);

class DllLoader {
public:
	explicit DllLoader(LPCTSTR filename)
	{
		this->hmodule = ::LoadLibrary(filename);
		if (this->hmodule == nullptr) this->success = false;
	}


	~DllLoader()
	{
		::FreeLibrary(this->hmodule);
	}

	bool Success() { return this->success; }

	class ProcAddress {
	public:
		explicit ProcAddress(void *p) : p(p) {}

		template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
		operator T *() const
		{
			return reinterpret_cast<T *>(this->p);
		}

	private:
		void *p;
	};

	ProcAddress GetProcAddress(const char *proc_name)
	{
		void *p = reinterpret_cast<void *>(::GetProcAddress(this->hmodule, proc_name));
		if (p == nullptr) this->success = false;
		return ProcAddress(p);
	}

private:
	HMODULE hmodule = nullptr;
	bool success = true;
};

char *convert_from_fs(const wchar_t *name, char *utf8_buf, size_t buflen);
wchar_t *convert_to_fs(const std::string_view name, wchar_t *utf16_buf, size_t buflen);

void Win32SetCurrentLocaleName(const char *iso_code);
int OTTDStringCompare(std::string_view s1, std::string_view s2);
int Win32StringContains(const std::string_view str, const std::string_view value, bool case_insensitive);

#endif /* WIN32_H */
