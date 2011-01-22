/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file blob.hpp Support for storing random binary data. */

#ifndef BLOB_HPP
#define BLOB_HPP

#include "../core/alloc_func.hpp"

/**
 * Base class for simple binary blobs.
 *  Item is byte.
 *  The word 'simple' means:
 *    - no configurable allocator type (always made from heap)
 *    - no smart deallocation - deallocation must be called from the same
 *        module (DLL) where the blob was allocated
 *    - no configurable allocation policy (how big blocks should be allocated)
 *    - no extra ownership policy (i.e. 'copy on write') when blob is copied
 *    - no thread synchronization at all
 *
 *  Internal member layout:
 *  1. The only class member is pointer to the first item (see union).
 *  2. Allocated block contains the blob header (see BlobHeader) followed by the raw byte data.
 *     Always, when it allocates memory the allocated size is:
 *                                                      sizeof(BlobHeader) + <data capacity>
 *  3. Two 'virtual' members (items and capacity) are stored in the BlobHeader at beginning
 *     of the alloated block.
 *  4. The pointer of the union pobsize_ts behind the header (to the first data byte).
 *     When memory block is allocated, the sizeof(BlobHeader) it added to it.
 *  5. Benefits of this layout:
 *     - items are accessed in the simplest possible way - just dereferencing the pointer,
 *       which is good for performance (assuming that data are accessed most often).
 *     - sizeof(blob) is the same as the size of any other pointer
 *  6. Drawbacks of this layout:
 *     - the fact that a pointer to the allocated block is adjusted by sizeof(BlobHeader) before
 *       it is stored can lead to several confusions:
 *         - it is not a common pattern so the implementation code is bit harder to read.
 *         - valgrind may generate a warning that the allocated block is lost (not accessible).
 */
class ByteBlob {
protected:
	/** header of the allocated memory block */
	struct BlobHeader {
		size_t items;     ///< actual blob size in bytes
		size_t capacity;  ///< maximum (allocated) size in bytes
	};

	/** type used as class member */
	union {
		byte       *data;    ///< ptr to the first byte of data
		BlobHeader *header;  ///< ptr just after the BlobHeader holding items and capacity
	};

private:
	/**
	 * Just to silence an unsilencable GCC 4.4+ warning
	 * Note: This cannot be 'const' as we do a lot of 'hdrEmpty[0]->items += 0;' and 'hdrEmpty[0]->capacity += 0;'
	 *       after const_casting.
	 */
	static BlobHeader hdrEmpty[];

public:
	static const size_t tail_reserve = 4; ///< four extra bytes will be always allocated and zeroed at the end
	static const size_t header_size = sizeof(BlobHeader);

	/** default constructor - initializes empty blob */
	FORCEINLINE ByteBlob() { InitEmpty(); }

	/** copy constructor */
	FORCEINLINE ByteBlob(const ByteBlob &src)
	{
		InitEmpty();
		AppendRaw(src);
	}

	/** move constructor - take ownership of blob data */
	FORCEINLINE ByteBlob(BlobHeader * const & src)
	{
		assert(src != NULL);
		header = src;
		*const_cast<BlobHeader**>(&src) = NULL;
	}

	/** destructor */
	FORCEINLINE ~ByteBlob()
	{
		Free();
	}

protected:
	/** all allocation should happen here */
	static FORCEINLINE BlobHeader *RawAlloc(size_t num_bytes)
	{
		return (BlobHeader*)MallocT<byte>(num_bytes);
	}

	/**
	 * Return header pointer to the static BlobHeader with
	 * both items and capacity containing zero
	 */
	static FORCEINLINE BlobHeader *Zero()
	{
		return const_cast<BlobHeader *>(&ByteBlob::hdrEmpty[1]);
	}

	/** simple allocation policy - can be optimized later */
	static FORCEINLINE size_t AllocPolicy(size_t min_alloc)
	{
		if (min_alloc < (1 << 9)) {
			if (min_alloc < (1 << 5)) return (1 << 5);
			return (min_alloc < (1 << 7)) ? (1 << 7) : (1 << 9);
		}
		if (min_alloc < (1 << 15)) {
			if (min_alloc < (1 << 11)) return (1 << 11);
			return (min_alloc < (1 << 13)) ? (1 << 13) : (1 << 15);
		}
		if (min_alloc < (1 << 20)) {
			if (min_alloc < (1 << 17)) return (1 << 17);
			return (min_alloc < (1 << 19)) ? (1 << 19) : (1 << 20);
		}
		min_alloc = (min_alloc | ((1 << 20) - 1)) + 1;
		return min_alloc;
	}

	/** all deallocations should happen here */
	static FORCEINLINE void RawFree(BlobHeader *p)
	{
		/* Just to silence an unsilencable GCC 4.4+ warning. */
		assert(p != ByteBlob::hdrEmpty);

		/* In case GCC warns about the following, see GCC's PR38509 why it is bogus. */
		free(p);
	}

	/** initialize the empty blob */
	FORCEINLINE void InitEmpty()
	{
		header = Zero();
	}

	/** initialize blob by attaching it to the given header followed by data */
	FORCEINLINE void Init(BlobHeader *src)
	{
		header = &src[1];
	}

	/** blob header accessor - use it rather than using the pointer arithmetics directly - non-const version */
	FORCEINLINE BlobHeader& Hdr()
	{
		return *(header - 1);
	}

	/** blob header accessor - use it rather than using the pointer arithmetics directly - const version */
	FORCEINLINE const BlobHeader& Hdr() const
	{
		return *(header - 1);
	}

	/** return reference to the actual blob size - used when the size needs to be modified */
	FORCEINLINE size_t& LengthRef()
	{
		return Hdr().items;
	}

public:
	/** return true if blob doesn't contain valid data */
	FORCEINLINE bool IsEmpty() const
	{
		return Length() == 0;
	}

	/** return the number of valid data bytes in the blob */
	FORCEINLINE size_t Length() const
	{
		return Hdr().items;
	}

	/** return the current blob capacity in bytes */
	FORCEINLINE size_t Capacity() const
	{
		return Hdr().capacity;
	}

	/** return pointer to the first byte of data - non-const version */
	FORCEINLINE byte *Begin()
	{
		return data;
	}

	/** return pointer to the first byte of data - const version */
	FORCEINLINE const byte *Begin() const
	{
		return data;
	}

	/** invalidate blob's data - doesn't free buffer */
	FORCEINLINE void Clear()
	{
		LengthRef() = 0;
	}

	/** free the blob's memory */
	FORCEINLINE void Free()
	{
		if (Capacity() > 0) {
			RawFree(&Hdr());
			InitEmpty();
		}
	}

	/** append new bytes at the end of existing data bytes - reallocates if necessary */
	FORCEINLINE void AppendRaw(const void *p, size_t num_bytes)
	{
		assert(p != NULL);
		if (num_bytes > 0) {
			memcpy(Append(num_bytes), p, num_bytes);
		}
	}

	/** append bytes from given source blob to the end of existing data bytes - reallocates if necessary */
	FORCEINLINE void AppendRaw(const ByteBlob& src)
	{
		if (!src.IsEmpty()) {
			memcpy(Append(src.Length()), src.Begin(), src.Length());
		}
	}

	/**
	 * Reallocate if there is no free space for num_bytes bytes.
	 *  @return pointer to the new data to be added
	 */
	FORCEINLINE byte *Prepare(size_t num_bytes)
	{
		size_t new_size = Length() + num_bytes;
		if (new_size > Capacity()) SmartAlloc(new_size);
		return data + Length();
	}

	/**
	 * Increase Length() by num_bytes.
	 *  @return pointer to the new data added
	 */
	FORCEINLINE byte *Append(size_t num_bytes)
	{
		byte *pNewData = Prepare(num_bytes);
		LengthRef() += num_bytes;
		return pNewData;
	}

	/** reallocate blob data if needed */
	void SmartAlloc(size_t new_size)
	{
		if (Capacity() >= new_size) return;
		/* calculate minimum block size we need to allocate
		 * and ask allocation policy for some reasonable block size */
		new_size = AllocPolicy(header_size + new_size + tail_reserve);

		/* allocate new block and setup header */
		BlobHeader *tmp = RawAlloc(new_size);
		tmp->items = Length();
		tmp->capacity = new_size - (header_size + tail_reserve);

		/* copy existing data */
		if (tmp->items != 0) {
			memcpy(tmp + 1, data, tmp->items);
		}

		/* replace our block with new one */
		if (Capacity() > 0) {
			RawFree(&Hdr());
		}
		Init(tmp);
	}

	/** fixing the four bytes at the end of blob data - useful when blob is used to hold string */
	FORCEINLINE void FixTail() const
	{
		if (Capacity() > 0) {
			byte *p = &data[Length()];
			for (uint i = 0; i < tail_reserve; i++) {
				p[i] = 0;
			}
		}
	}
};

/**
 * Blob - simple dynamic T array. T (template argument) is a placeholder for any type.
 *  T can be any integral type, pointer, or structure. Using Blob instead of just plain C array
 *  simplifies the resource management in several ways:
 *  1. When adding new item(s) it automatically grows capacity if needed.
 *  2. When variable of type Blob comes out of scope it automatically frees the data buffer.
 *  3. Takes care about the actual data size (number of used items).
 *  4. Dynamically constructs only used items (as opposite of static array which constructs all items)
 */
template <typename T>
class CBlobT : public ByteBlob {
	/* make template arguments public: */
public:
	typedef ByteBlob base;

	static const size_t type_size = sizeof(T);

	struct OnTransfer {
		typename base::BlobHeader *header;
		OnTransfer(const OnTransfer& src) : header(src.header) {assert(src.header != NULL); *const_cast<typename base::BlobHeader**>(&src.header) = NULL;}
		OnTransfer(CBlobT& src) : header(src.header) {src.InitEmpty();}
		~OnTransfer() {assert(header == NULL);}
	};

	/** Default constructor - makes new Blob ready to accept any data */
	FORCEINLINE CBlobT()
		: base()
	{}

	/** Take ownership constructor */
	FORCEINLINE CBlobT(const OnTransfer& ot)
		: base(ot.header)
	{}

	/** Destructor - ensures that allocated memory (if any) is freed */
	FORCEINLINE ~CBlobT()
	{
		Free();
	}

	/** Check the validity of item index (only in debug mode) */
	FORCEINLINE void CheckIdx(size_t index) const
	{
		assert(index < Size());
	}

	/** Return pointer to the first data item - non-const version */
	FORCEINLINE T *Data()
	{
		return (T*)base::Begin();
	}

	/** Return pointer to the first data item - const version */
	FORCEINLINE const T *Data() const
	{
		return (const T*)base::Begin();
	}

	/** Return pointer to the index-th data item - non-const version */
	FORCEINLINE T *Data(size_t index)
	{
		CheckIdx(index);
		return (Data() + index);
	}

	/** Return pointer to the index-th data item - const version */
	FORCEINLINE const T *Data(size_t index) const
	{
		CheckIdx(index);
		return (Data() + index);
	}

	/** Return number of items in the Blob */
	FORCEINLINE size_t Size() const
	{
		return (base::Length() / type_size);
	}

	/** Return total number of items that can fit in the Blob without buffer reallocation */
	FORCEINLINE size_t MaxSize() const
	{
		return (base::Capacity() / type_size);
	}

	/** Return number of additional items that can fit in the Blob without buffer reallocation */
	FORCEINLINE size_t GetReserve() const
	{
		return ((base::Capacity() - base::Length()) / type_size);
	}

	/** Grow number of data items in Blob by given number - doesn't construct items */
	FORCEINLINE T *GrowSizeNC(size_t num_items)
	{
		return (T*)base::Append(num_items * type_size);
	}

	/**
	 * Ensures that given number of items can be added to the end of Blob. Returns pointer to the
	 *  first free (unused) item
	 */
	FORCEINLINE T *MakeFreeSpace(size_t num_items)
	{
		return (T*)base::Prepare(num_items * type_size);
	}

	FORCEINLINE OnTransfer Transfer()
	{
		return OnTransfer(*this);
	}
};


#endif /* BLOB_HPP */
