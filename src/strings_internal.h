/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file strings_internal.h Types and functions related to the internal workings of formatting OpenTTD's strings. */

#ifndef STRINGS_INTERNAL_H
#define STRINGS_INTERNAL_H

#include "strings_func.h"
#include "string_func.h"
#include "core/string_builder.hpp"
#include "core/string_consumer.hpp"

class StringParameters {
protected:
	StringParameters *parent = nullptr; ///< If not nullptr, this instance references data from this parent instance.
	std::span<StringParameter> parameters = {}; ///< Array with the actual parameters.

	size_t offset = 0; ///< Current offset in the parameters span.
	char32_t next_type = 0; ///< The type of the next data that is retrieved.

	const StringParameter &GetNextParameterReference();

public:
	/**
	 * Create a new StringParameters instance that can reference part of the data of
	 * the given parent instance.
	 */
	StringParameters(StringParameters &parent, size_t size) :
		parent(&parent),
		parameters(parent.parameters.subspan(parent.offset, size))
	{}

	StringParameters(std::span<StringParameter> parameters = {}) : parameters(parameters) {}

	void SetTypeOfNextParameter(char32_t type) { this->next_type = type; }

	/**
	 * Get the current offset, so it can be backed up for certain processing
	 * steps, or be used to offset the argument index within sub strings.
	 * @return The current offset.
	 */
	size_t GetOffset() { return this->offset; }

	/**
	 * Set the offset within the string from where to return the next result of
	 * \c GetInt64 or \c GetInt32.
	 * @param offset The offset.
	 */
	void SetOffset(size_t offset)
	{
		/*
		 * The offset must be fewer than the number of parameters when it is
		 * being set. Unless restoring a backup, then the original value is
		 * correct as well as long as the offset was not changed. In other
		 * words, when the offset was already at the end of the parameters and
		 * the string did not consume any parameters.
		 */
		assert(offset < this->parameters.size() || this->offset == offset);
		this->offset = offset;
	}

	/**
	 * Advance the offset within the string from where to return the next result of
	 * \c GetInt64 or \c GetInt32.
	 * @param advance The amount to advance the offset by.
	 */
	void AdvanceOffset(size_t advance)
	{
		this->offset += advance;
		assert(this->offset <= this->parameters.size());
	}

	/**
	 * Get the next parameter from our parameters.
	 * This updates the offset, so the next time this is called the next parameter
	 * will be read.
	 * @return The next parameter's value.
	 */
	uint64_t GetNextParameter()
	{
		struct visitor {
			uint64_t operator()(const std::monostate &) { throw std::out_of_range("Attempt to read uninitialised parameter as integer"); }
			uint64_t operator()(const uint64_t &arg) { return arg; }
			uint64_t operator()(const std::string &) { throw std::out_of_range("Attempt to read string parameter as integer"); }
		};

		const auto &param = this->GetNextParameterReference();
		return std::visit(visitor{}, param.data);
	}

	/**
	 * Get the next parameter from our parameters.
	 * This updates the offset, so the next time this is called the next parameter
	 * will be read.
	 * @tparam T The return type of the parameter.
	 * @return The next parameter's value.
	 */
	template <typename T>
	T GetNextParameter()
	{
		return static_cast<T>(this->GetNextParameter());
	}

	/**
	 * Get the next string parameter from our parameters.
	 * This updates the offset, so the next time this is called the next parameter
	 * will be read.
	 * @return The next parameter's value.
	 */
	std::string_view GetNextParameterString()
	{
		struct visitor {
			std::string_view operator()(const std::monostate &) { throw std::out_of_range("Attempt to read uninitialised parameter as string"); }
			std::string_view operator()(const uint64_t &) { throw std::out_of_range("Attempt to read integer parameter as string"); }
			std::string_view operator()(const std::string &arg) { return arg; }
		};

		const auto &param = this->GetNextParameterReference();
		return std::visit(visitor{}, param.data);
	}

	/**
	 * Get a new instance of StringParameters that is a "range" into the
	 * remaining existing parameters. Upon destruction the offset in the parent
	 * is not updated. However, calls to SetParam do update the parameters.
	 *
	 * The returned StringParameters must not outlive this StringParameters.
	 * @return A "range" of the string parameters.
	 */
	StringParameters GetRemainingParameters() { return GetRemainingParameters(this->offset); }

	/**
	 * Get a new instance of StringParameters that is a "range" into the
	 * remaining existing parameters from the given offset. Upon destruction the
	 * offset in the parent is not updated. However, calls to SetParam do
	 * update the parameters.
	 *
	 * The returned StringParameters must not outlive this StringParameters.
	 * @param offset The offset to get the remaining parameters for.
	 * @return A "range" of the string parameters.
	 */
	StringParameters GetRemainingParameters(size_t offset)
	{
		return StringParameters(this->parameters.subspan(offset, this->parameters.size() - offset));
	}

	/** Return the amount of elements which can still be read. */
	size_t GetDataLeft() const
	{
		return this->parameters.size() - this->offset;
	}

	/** Return the number of parameters. */
	size_t GetNumParameters() const
	{
		return this->parameters.size();
	}

	/** Get the type of a specific element. */
	char32_t GetTypeAtOffset(size_t offset) const
	{
		assert(offset < this->parameters.size());
		return this->parameters[offset].type;
	}

	void SetParam(size_t n, const StringParameterData &v)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = v;
	}

	void SetParam(size_t n, uint64_t v)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = v;
	}

	void SetParam(size_t n, ConvertibleThroughBase auto v)
	{
		SetParam(n, v.base());
	}

	void SetParam(size_t n, const std::string &str)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = str;
	}

	void SetParam(size_t n, std::string &&str)
	{
		assert(n < this->parameters.size());
		this->parameters[n].data = std::move(str);
	}

	const StringParameterData &GetParam(size_t n) const
	{
		assert(n < this->parameters.size());
		return this->parameters[n].data;
	}
};

void GetStringWithArgs(StringBuilder &builder, StringID string, StringParameters &args, uint case_index = 0, bool game_script = false);
void GetStringWithArgs(StringBuilder &builder, StringID string, std::span<StringParameter> params, uint case_index = 0, bool game_script = false);
std::string GetStringWithArgs(StringID string, StringParameters &args);

/* Do not leak the StringBuilder to everywhere. */
void GenerateTownNameString(StringBuilder &builder, size_t lang, uint32_t seed);
void GetTownName(StringBuilder &builder, const struct Town *t);
void GRFTownNameGenerate(StringBuilder &builder, uint32_t grfid, uint16_t gen, uint32_t seed);

char32_t RemapNewGRFStringControlCode(char32_t scc, StringConsumer &consumer);

#endif /* STRINGS_INTERNAL_H */
