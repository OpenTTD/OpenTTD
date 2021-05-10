/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file saveload_filter.h Declaration of filters used for saving and loading savegames. */

#ifndef SAVELOAD_FILTER_H
#define SAVELOAD_FILTER_H

/** Interface for filtering a savegame till it is loaded. */
struct LoadFilter {
	/** Chained to the (savegame) filters. */
	LoadFilter *chain;

	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	LoadFilter(LoadFilter *chain) : chain(chain)
	{
	}

	/** Make sure the writers are properly closed. */
	virtual ~LoadFilter()
	{
		delete this->chain;
	}

	/**
	 * Read a given number of bytes from the savegame.
	 * @param buf The bytes to read.
	 * @param len The number of bytes to read.
	 * @return The number of actually read bytes.
	 */
	virtual size_t Read(byte *buf, size_t len) = 0;

	/**
	 * Reset this filter to read from the beginning of the file.
	 */
	virtual void Reset()
	{
		this->chain->Reset();
	}
};

/**
 * Instantiator for a load filter.
 * @param chain The next filter in this chain.
 * @tparam T    The type of load filter to create.
 */
template <typename T> LoadFilter *CreateLoadFilter(LoadFilter *chain)
{
	return new T(chain);
}

/** Interface for filtering a savegame till it is written. */
struct SaveFilter {
	/** Chained to the (savegame) filters. */
	SaveFilter *chain;

	/**
	 * Initialise this filter.
	 * @param chain The next filter in this chain.
	 */
	SaveFilter(SaveFilter *chain) : chain(chain)
	{
	}

	/** Make sure the writers are properly closed. */
	virtual ~SaveFilter()
	{
		delete this->chain;
	}

	/**
	 * Write a given number of bytes into the savegame.
	 * @param buf The bytes to write.
	 * @param len The number of bytes to write.
	 */
	virtual void Write(byte *buf, size_t len) = 0;

	/**
	 * Prepare everything to finish writing the savegame.
	 */
	virtual void Finish()
	{
		if (this->chain != nullptr) this->chain->Finish();
	}
};

/**
 * Instantiator for a save filter.
 * @param chain             The next filter in this chain.
 * @param compression_level The requested level of compression.
 * @tparam T                The type of save filter to create.
 */
template <typename T> SaveFilter *CreateSaveFilter(SaveFilter *chain, byte compression_level)
{
	return new T(chain, compression_level);
}

#endif /* SAVELOAD_FILTER_H */
