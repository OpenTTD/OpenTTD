/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file library_loader.h Functions/types related to loading libraries dynamically. */

#ifndef LIBRARY_LOADER_H
#define LIBRARY_LOADER_H

class LibraryLoader {
public:
	/**
	 * A function loaded from a library.
	 *
	 * Will automatically cast to the correct function pointer type on retrieval.
	 */
	class Function {
	public:
		explicit Function(void *p) : p(p) {}

		template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
		operator T *() const
		{
			return reinterpret_cast<T *>(this->p);
		}

	private:
		void *p;
	};

	/**
	 * Load a library with the given filename.
	 */
	explicit LibraryLoader(const std::string &filename)
	{
		this->handle = this->OpenLibrary(filename);
	}

	/**
	 * Close the library.
	 */
	~LibraryLoader()
	{
		if (this->handle != nullptr) {
			this->CloseLibrary();
		}
	}

	/**
	 * Check whether an error occurred while loading the library or a function.
	 *
	 * @return Whether an error occurred.
	 */
	bool HasError()
	{
		return this->error.has_value();
	}

	/**
	 * Get the last error that occurred while loading the library or a function.
	 *
	 * @return The error message.
	 */
	std::string GetLastError()
	{
		return this->error.value_or("No error");
	}

	/**
	 * Get a function from a loaded library.
	 *
	 * @param symbol_name The name of the function to get.
	 * @return The function. Check HasError() before using.
	 */
	Function GetFunction(const std::string &symbol_name)
	{
		if (this->error.has_value()) return Function(nullptr);
		return Function(this->GetSymbol(symbol_name));
	}

private:
	/**
	 * Open the library with the given filename.
	 *
	 * Should set error if any error occurred.
	 *
	 * @param filename The filename of the library to open.
	 */
	void *OpenLibrary(const std::string &filename);

	/**
	 * Close the library.
	 */
	void CloseLibrary();

	/**
	 * Get a symbol from the library.
	 *
	 * Should set error if any error occurred.
	 *
	 * @param symbol_name The name of the symbol to get.
	 */
	void *GetSymbol(const std::string &symbol_name);

	std::optional<std::string> error = {}; ///< The last error that occurred, if set.
	void *handle = nullptr; ///< Handle to the library.
};

#endif /* LIBRARY_LOADER_H */
