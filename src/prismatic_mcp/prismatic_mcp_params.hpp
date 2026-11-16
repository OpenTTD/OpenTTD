/*
 * Part of korczis's PRIVATE RESEARCH FORK of OpenTTD. RESEARCH-ONLY / UNSTABLE.
 * Compiled only when OTTD_PRISMATIC_MCP_SERVER is defined.
 */

/**
 * @file prismatic_mcp_params.hpp Generic conversion between JSON and command parameters.
 *
 * Every OpenTTD command declares its parameters as the types of its Cmd* function
 * (CommandTraits<Tcmd>::Args). This header derives, from those types alone, a JSON Schema
 * for each parameter and a checked conversion from a JSON value, so no command needs a
 * hand-written MCP binding. A parameter type the conversion cannot represent is reported as
 * unsupported (with its C++ type name), never guessed at. Pure: no game state is read.
 */

#ifndef PRISMATIC_MCP_PARAMS_HPP
#define PRISMATIC_MCP_PARAMS_HPP

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "../3rdparty/nlohmann/json.hpp"
#include "../core/convertible_through_base.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace PrismaticMCP::params {

using json = nlohmann::json;

/**
 * The C++ name of a type, from the compiler's own function signature.
 * @tparam T The type to name.
 * @return The spelled type, or "unknown" when the compiler's format is not recognised.
 */
template <typename T>
constexpr std::string_view TypeName()
{
	std::string_view p = __PRETTY_FUNCTION__;
	size_t s = p.find("T = ");
	if (s == std::string_view::npos) return "unknown";
	s += 4;
	size_t e = p.find_first_of(";]", s);
	if (e == std::string_view::npos) return "unknown";
	return p.substr(s, e - s);
}

/** The integer type a parameter is carried as: itself, an enum's underlying type, or a strong type's base. */
template <typename U, typename = void>
struct IntegerCarrier { using type = void; };
template <typename U>
struct IntegerCarrier<U, std::enable_if_t<std::is_integral_v<U> && !std::is_same_v<U, bool>>> { using type = U; };
template <typename U>
struct IntegerCarrier<U, std::enable_if_t<std::is_enum_v<U>>> { using type = std::underlying_type_t<U>; };

template <typename U>
constexpr bool IsStrongInteger()
{
	if constexpr (ConvertibleThroughBase<U>) {
		return std::is_integral_v<typename U::BaseType> && std::is_constructible_v<U, typename U::BaseType>;
	} else {
		return false;
	}
}

/**
 * Whether a parameter of this type can be converted from JSON.
 * @tparam T The parameter type, as the command function declares it.
 * @return true for bool, std::string, integers, enums, strong integer types and int64-like values.
 */
template <typename T>
constexpr bool IsSupported()
{
	using U = std::remove_cvref_t<T>;
	if constexpr (!std::is_default_constructible_v<U>) {
		return false;
	} else if constexpr (std::is_same_v<U, bool> || std::is_same_v<U, std::string>) {
		return true;
	} else if constexpr (std::is_integral_v<U> || std::is_enum_v<U>) {
		return true;
	} else if constexpr (IsStrongInteger<U>()) {
		return true;
	} else if constexpr (std::is_constructible_v<U, int64_t> && std::is_convertible_v<U, int64_t>) {
		return true; // Money and other overflow-safe integers
	} else {
		return false;
	}
}

/**
 * Range-checked conversion of a JSON integer into an integer type.
 * @param j The JSON value.
 * @param[out] out The converted value.
 * @param[out] err Why the value was refused.
 * @return true when the value is an integer within the type's range.
 */
template <typename B>
bool IntFromJson(const json &j, B &out, std::string &err)
{
	static_assert(std::is_integral_v<B> && !std::is_same_v<B, bool>);
	if (!j.is_number_integer()) { err = "expected an integer"; return false; }
	constexpr auto lo = std::numeric_limits<B>::min();
	constexpr auto hi = std::numeric_limits<B>::max();
	if (j.is_number_unsigned()) {
		uint64_t v = j.get<uint64_t>();
		if (v > static_cast<uint64_t>(hi)) { err = "integer above " + std::to_string(hi); return false; }
		out = static_cast<B>(v);
		return true;
	}
	int64_t v = j.get<int64_t>();
	if constexpr (std::is_signed_v<B>) {
		if (v < static_cast<int64_t>(lo) || v > static_cast<int64_t>(hi)) {
			err = "integer outside " + std::to_string(lo) + ".." + std::to_string(hi);
			return false;
		}
	} else {
		if (v < 0 || static_cast<uint64_t>(v) > static_cast<uint64_t>(hi)) {
			err = "integer outside 0.." + std::to_string(hi);
			return false;
		}
	}
	out = static_cast<B>(v);
	return true;
}

/** The JSON Schema of an integer type, with its range. */
template <typename B>
json IntSchema()
{
	json s = {{"type", "integer"}};
	if constexpr (sizeof(B) < 8 || std::is_signed_v<B>) {
		s["minimum"] = static_cast<int64_t>(std::numeric_limits<B>::min());
	} else {
		s["minimum"] = 0;
	}
	if constexpr (sizeof(B) < 8 || std::is_signed_v<B>) {
		s["maximum"] = static_cast<int64_t>(std::numeric_limits<B>::max());
	} else {
		s["maximum"] = std::numeric_limits<uint64_t>::max();
	}
	return s;
}

/**
 * The JSON Schema of one parameter, derived from its type.
 * @tparam T The parameter type.
 * @return A schema carrying the C++ type name as x-cpp-type.
 */
template <typename T>
json Schema()
{
	using U = std::remove_cvref_t<T>;
	json s;
	if constexpr (!IsSupported<U>()) {
		s = json::object();
	} else if constexpr (std::is_same_v<U, bool>) {
		s = {{"type", "boolean"}};
	} else if constexpr (std::is_same_v<U, std::string>) {
		s = {{"type", "string"}};
	} else if constexpr (std::is_enum_v<U>) {
		s = IntSchema<std::underlying_type_t<U>>();
		s["x-kind"] = "enum";
	} else if constexpr (std::is_integral_v<U>) {
		s = IntSchema<U>();
	} else if constexpr (IsStrongInteger<U>()) {
		s = IntSchema<typename U::BaseType>();
		s["x-kind"] = "id";
	} else {
		s = IntSchema<int64_t>();
		s["x-kind"] = "money";
	}
	s["x-cpp-type"] = std::string(TypeName<U>());
	if (TypeName<U>().find("TileIndex") != std::string_view::npos) {
		s["description"] = "tile index: y * map_width + x";
	}
	return s;
}

/**
 * Convert one JSON value into a parameter.
 * @param j The JSON value.
 * @param[out] out The parameter.
 * @param[out] err Why the value was refused.
 * @return true on success.
 */
template <typename T>
bool FromJson(const json &j, T &out, std::string &err)
{
	using U = std::remove_cvref_t<T>;
	if constexpr (!IsSupported<U>()) {
		err = "unsupported parameter type " + std::string(TypeName<U>());
		return false;
	} else if constexpr (std::is_same_v<U, bool>) {
		if (!j.is_boolean()) { err = "expected a boolean"; return false; }
		out = j.get<bool>();
		return true;
	} else if constexpr (std::is_same_v<U, std::string>) {
		if (!j.is_string()) { err = "expected a string"; return false; }
		out = j.get<std::string>();
		return true;
	} else if constexpr (std::is_enum_v<U>) {
		std::underlying_type_t<U> v{};
		if (!IntFromJson(j, v, err)) return false;
		out = static_cast<U>(v);
		return true;
	} else if constexpr (std::is_integral_v<U>) {
		return IntFromJson(j, out, err);
	} else if constexpr (IsStrongInteger<U>()) {
		typename U::BaseType v{};
		if (!IntFromJson(j, v, err)) return false;
		out = U(v);
		return true;
	} else {
		int64_t v = 0;
		if (!IntFromJson(j, v, err)) return false;
		out = U(v);
		return true;
	}
}

/** Whether every element type of a tuple is supported. */
template <typename Tuple, size_t... I>
constexpr bool AllSupported(std::index_sequence<I...>)
{
	return (IsSupported<std::tuple_element_t<I, Tuple>>() && ...);
}

template <typename Tuple>
constexpr bool TupleSupported()
{
	return AllSupported<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/** The schema of every parameter of a tuple, in order. */
template <typename Tuple, size_t... I>
json TupleSchemas(std::index_sequence<I...>)
{
	json arr = json::array();
	(arr.push_back(Schema<std::tuple_element_t<I, Tuple>>()), ...);
	return arr;
}

template <typename Tuple>
json TupleSchemas()
{
	return TupleSchemas<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/** The C++ names of the parameter types a tuple carries that cannot be converted. */
template <typename Tuple, size_t... I>
json UnsupportedTypes(std::index_sequence<I...>)
{
	json arr = json::array();
	((IsSupported<std::tuple_element_t<I, Tuple>>() ? void() : arr.push_back(std::string(TypeName<std::tuple_element_t<I, Tuple>>()))), ...);
	return arr;
}

template <typename Tuple>
json UnsupportedTypes()
{
	return UnsupportedTypes<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename Tuple, size_t... I>
bool TupleFromJsonImpl(const json &arr, Tuple &t, std::string &err, std::index_sequence<I...>)
{
	bool ok = true;
	((ok = ok && [&] {
		std::string e;
		if (FromJson(arr[I], std::get<I>(t), e)) return true;
		err = "params[" + std::to_string(I) + "] (" + std::string(TypeName<std::tuple_element_t<I, Tuple>>()) + "): " + e;
		return false;
	}()), ...);
	return ok;
}

/**
 * Convert a JSON array into a parameter tuple, positionally.
 * @param arr The JSON array; its length must equal the tuple's.
 * @param[out] t The tuple.
 * @param[out] err Why the array was refused, naming the parameter.
 * @return true on success.
 */
template <typename Tuple>
bool TupleFromJson(const json &arr, Tuple &t, std::string &err)
{
	constexpr size_t n = std::tuple_size_v<Tuple>;
	if (!arr.is_array()) { err = "params must be an array"; return false; }
	if (arr.size() != n) { err = "expected " + std::to_string(n) + " params, got " + std::to_string(arr.size()); return false; }
	return TupleFromJsonImpl(arr, t, err, std::make_index_sequence<n>{});
}

} // namespace PrismaticMCP::params

#endif /* OTTD_PRISMATIC_MCP_SERVER */
#endif /* PRISMATIC_MCP_PARAMS_HPP */
