/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_consumer.cpp Test functionality from core/string_consumer. */

#include "../stdafx.h"

#include <iostream>

#include "../3rdparty/catch2/catch.hpp"

#include "../core/string_consumer.hpp"

#include "../safeguards.h"

using namespace std::literals;

TEST_CASE("StringConsumer - basic")
{
	StringConsumer reader("ab"sv);
	CHECK(!reader.AnyBytesRead());
	CHECK(reader.GetBytesRead() == 0);
	CHECK(reader.AnyBytesLeft());
	CHECK(reader.GetBytesLeft() == 2);
	CHECK(reader.GetOrigData() == "ab");
	CHECK(reader.GetReadData() == "");
	CHECK(reader.GetLeftData() == "ab");
	reader.Skip(1);
	CHECK(reader.AnyBytesRead());
	CHECK(reader.GetBytesRead() == 1);
	CHECK(reader.AnyBytesLeft());
	CHECK(reader.GetBytesLeft() == 1);
	CHECK(reader.GetOrigData() == "ab");
	CHECK(reader.GetReadData() == "a");
	CHECK(reader.GetLeftData() == "b");
	reader.SkipAll();
	CHECK(reader.AnyBytesRead());
	CHECK(reader.GetBytesRead() == 2);
	CHECK(!reader.AnyBytesLeft());
	CHECK(reader.GetBytesLeft() == 0);
	CHECK(reader.GetOrigData() == "ab");
	CHECK(reader.GetReadData() == "ab");
	CHECK(reader.GetLeftData() == "");
	reader.Skip(1);
	CHECK(reader.AnyBytesRead());
	CHECK(reader.GetBytesRead() == 2);
	CHECK(!reader.AnyBytesLeft());
	CHECK(reader.GetBytesLeft() == 0);
	CHECK(reader.GetOrigData() == "ab");
	CHECK(reader.GetReadData() == "ab");
	CHECK(reader.GetLeftData() == "");
}

TEST_CASE("StringConsumer - binary8")
{
	StringConsumer reader("\xFF\xFE\xFD\0"sv);
	CHECK(reader.PeekUint8() == 0xFF);
	CHECK(reader.PeekSint8() == -1);
	CHECK(reader.PeekChar() == static_cast<char>(-1));
	reader.SkipUint8();
	CHECK(reader.PeekUint8() == 0xFE);
	CHECK(reader.PeekSint8() == -2);
	CHECK(reader.PeekChar() == static_cast<char>(-2));
	CHECK(reader.ReadUint8() == 0xFE);
	CHECK(reader.PeekUint8() == 0xFD);
	CHECK(reader.PeekSint8() == -3);
	CHECK(reader.PeekChar() == static_cast<char>(-3));
	CHECK(reader.ReadSint8() == -3);
	CHECK(reader.PeekUint8() == 0);
	CHECK(reader.PeekSint8() == 0);
	CHECK(reader.PeekChar() == 0);
	CHECK(reader.ReadChar() == 0);
	CHECK(reader.PeekUint8() == std::nullopt);
	CHECK(reader.PeekSint8() == std::nullopt);
	CHECK(reader.PeekChar() == std::nullopt);
	CHECK(reader.ReadUint8(42) == 42);
	reader.SkipSint8();
	CHECK(reader.ReadSint8(42) == 42);
	CHECK(reader.ReadChar(42) == 42);
}

TEST_CASE("StringConsumer - binary16")
{
	StringConsumer reader("\xFF\xFF\xFE\xFF\xFD\xFF"sv);
	CHECK(reader.PeekUint16LE() == 0xFFFF);
	CHECK(reader.PeekSint16LE() == -1);
	reader.SkipUint16LE();
	CHECK(reader.PeekUint16LE() == 0xFFFE);
	CHECK(reader.PeekSint16LE() == -2);
	CHECK(reader.ReadUint16LE() == 0xFFFE);
	CHECK(reader.PeekUint16LE() == 0xFFFD);
	CHECK(reader.PeekSint16LE() == -3);
	CHECK(reader.ReadSint16LE() == -3);
	CHECK(reader.PeekUint16LE() == std::nullopt);
	CHECK(reader.PeekSint16LE() == std::nullopt);
	CHECK(reader.ReadUint16LE(42) == 42);
	reader.SkipSint16LE();
	CHECK(reader.ReadSint16LE(42) == 42);
}

TEST_CASE("StringConsumer - binary32")
{
	StringConsumer reader("\xFF\xFF\xFF\xFF\xFE\xFF\xFF\xFF\xFD\xFF\xFF\xFF"sv);
	CHECK(reader.PeekUint32LE() == 0xFFFFFFFF);
	CHECK(reader.PeekSint32LE() == -1);
	reader.SkipUint32LE();
	CHECK(reader.PeekUint32LE() == 0xFFFFFFFE);
	CHECK(reader.PeekSint32LE() == -2);
	CHECK(reader.ReadUint32LE() == 0xFFFFFFFE);
	CHECK(reader.PeekUint32LE() == 0xFFFFFFFD);
	CHECK(reader.PeekSint32LE() == -3);
	CHECK(reader.ReadSint32LE() == -3);
	CHECK(reader.PeekUint32LE() == std::nullopt);
	CHECK(reader.PeekSint32LE() == std::nullopt);
	CHECK(reader.ReadUint32LE(42) == 42);
	reader.SkipSint32LE();
	CHECK(reader.ReadSint32LE(42) == 42);
}

TEST_CASE("StringConsumer - binary64")
{
	StringConsumer reader("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFD\xFF\xFF\xFF\xFF\xFF\xFF\xFF"sv);
	CHECK(reader.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFF);
	CHECK(reader.PeekSint64LE() == -1);
	reader.SkipUint64LE();
	CHECK(reader.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFE);
	CHECK(reader.PeekSint64LE() == -2);
	CHECK(reader.ReadUint64LE() == 0xFFFFFFFF'FFFFFFFE);
	CHECK(reader.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFD);
	CHECK(reader.PeekSint64LE() == -3);
	CHECK(reader.ReadSint64LE() == -3);
	CHECK(reader.PeekUint64LE() == std::nullopt);
	CHECK(reader.PeekSint64LE() == std::nullopt);
	CHECK(reader.ReadUint64LE(42) == 42);
	reader.SkipSint64LE();
	CHECK(reader.ReadSint64LE(42) == 42);
}

TEST_CASE("StringConsumer - utf8")
{
	StringConsumer reader("a\u1234\xFF\xFE""b"sv);
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(1, 'a'));
	reader.SkipUtf8();
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(3, 0x1234));
	CHECK(reader.ReadUtf8() == 0x1234);
	CHECK(reader.PeekUint8() == 0xFF);
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	CHECK(reader.ReadUtf8() == '?');
	CHECK(reader.PeekUint8() == 0xFE);
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	reader.SkipUtf8();
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(1, 'b'));
	CHECK(reader.ReadUtf8() == 'b');
	CHECK(!reader.AnyBytesLeft());
	CHECK(reader.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	CHECK(reader.ReadUtf8() == '?');
}

TEST_CASE("StringConsumer - conditions")
{
	StringConsumer reader("ABCDabcde\u0234@@@gh\0\0\0ij\0\0\0kl"sv);
	CHECK(reader.PeekIf("AB"));
	CHECK(reader.PeekCharIf('A'));
	CHECK(reader.PeekUtf8If('A'));
	CHECK(!reader.PeekIf("CD"));
	CHECK(!reader.ReadIf("CD"));
	reader.SkipIf("CD");
	CHECK(reader.ReadIf("AB"));
	CHECK(reader.PeekIf("CD"));
	reader.SkipIf("CD");
	CHECK(reader.Peek(2) == "ab");
	CHECK(reader.Read(2) == "ab");
	CHECK(reader.Peek(2) == "cd");
	CHECK(reader.Find("e\u0234") == 2);
	CHECK(reader.Find("ab") == StringConsumer::npos);
	CHECK(reader.FindChar('e') == 2);
	CHECK(reader.FindChar('a') == StringConsumer::npos);
	CHECK(reader.FindUtf8(0x234) == 3);
	CHECK(reader.FindUtf8(0x1234) == StringConsumer::npos);
	reader.Skip(2);
	CHECK(reader.Peek(3) == "e\u0234");
	CHECK(reader.PeekUntil("e", StringConsumer::READ_ALL_SEPARATORS) == "e");
	CHECK(reader.PeekUntil("e", StringConsumer::READ_ONE_SEPARATOR) == "e");
	CHECK(reader.PeekUntil("e", StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(reader.PeekUntil("e", StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(reader.PeekUntil("e", StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(reader.PeekUntil("@", StringConsumer::READ_ALL_SEPARATORS) == "e\u0234@@@");
	CHECK(reader.PeekUntil("@", StringConsumer::READ_ONE_SEPARATOR) == "e\u0234@");
	CHECK(reader.PeekUntil("@", StringConsumer::KEEP_SEPARATOR) == "e\u0234");
	CHECK(reader.PeekUntil("@", StringConsumer::SKIP_ONE_SEPARATOR) == "e\u0234");
	CHECK(reader.PeekUntil("@", StringConsumer::SKIP_ALL_SEPARATORS) == "e\u0234");
	CHECK(reader.ReadUntil("@", StringConsumer::KEEP_SEPARATOR) == "e\u0234");
	CHECK(reader.ReadUntil("@", StringConsumer::READ_ONE_SEPARATOR) == "@");
	CHECK(reader.ReadUntil("@", StringConsumer::READ_ALL_SEPARATORS) == "@@");
	CHECK(reader.PeekUntilChar(0, StringConsumer::READ_ALL_SEPARATORS) == "gh\0\0\0"sv);
	CHECK(reader.PeekUntilChar(0, StringConsumer::READ_ONE_SEPARATOR) == "gh\0"sv);
	CHECK(reader.PeekUntilChar(0, StringConsumer::KEEP_SEPARATOR) == "gh");
	CHECK(reader.PeekUntilChar(0, StringConsumer::SKIP_ONE_SEPARATOR) == "gh");
	CHECK(reader.PeekUntilChar(0, StringConsumer::SKIP_ALL_SEPARATORS) == "gh");
	CHECK(reader.ReadUntilChar(0, StringConsumer::READ_ONE_SEPARATOR) == "gh\0"sv);
	CHECK(reader.PeekUntilChar(0, StringConsumer::READ_ALL_SEPARATORS) == "\0\0"sv);
	CHECK(reader.ReadUntilChar(0, StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(reader.PeekUntilChar(0, StringConsumer::READ_ALL_SEPARATORS) == "\0"sv);
	reader.SkipUntilUtf8(0, StringConsumer::READ_ALL_SEPARATORS);
	CHECK(reader.PeekUntilUtf8(0, StringConsumer::KEEP_SEPARATOR) == "ij");
	reader.SkipUntilUtf8(0, StringConsumer::SKIP_ALL_SEPARATORS);
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "kl");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "kl");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "kl");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "kl");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "kl");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "kl");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(reader.PeekUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(reader.ReadUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(reader.Peek(2) == "");
	CHECK(reader.Read(2) == "");
}

TEST_CASE("StringConsumer - ascii")
{
	StringConsumer reader("abcdefgh  \r\n\tAB  \r\n\t"sv);
	CHECK(reader.FindCharIn("dc") == 2);
	CHECK(reader.FindCharIn("xy") == StringConsumer::npos);
	CHECK(reader.FindCharNotIn("ba") == 2);
	CHECK(reader.PeekUntilCharNotIn("ba") == "ab");
	CHECK(reader.PeekUntilCharNotIn("dc") == "");
	CHECK(reader.PeekUntilCharIn("ba") == "");
	CHECK(reader.PeekUntilCharIn("dc") == "ab");
	CHECK(reader.ReadUntilCharNotIn("dc") == "");
	CHECK(reader.ReadUntilCharNotIn("ba") == "ab");
	CHECK(reader.ReadUntilCharIn("dc") == "");
	CHECK(reader.ReadUntilCharIn("fe") == "cd");
	CHECK(reader.PeekIf("ef"));
	reader.SkipUntilCharNotIn("ji");
	CHECK(reader.PeekIf("ef"));
	reader.SkipUntilCharNotIn("fe");
	CHECK(reader.PeekIf("gh"));
	reader.SkipUntilCharIn("hg");
	CHECK(reader.PeekIf("gh"));
	reader.SkipUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(reader.PeekCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == ' ');
	CHECK(reader.ReadCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == ' ');
	reader.SkipCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(reader.PeekUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\r");
	CHECK(reader.ReadUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\r");
	reader.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	CHECK(reader.PeekCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == '\n');
	CHECK(reader.ReadCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == '\n');
	CHECK(reader.PeekUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\t");
	CHECK(reader.ReadUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\t");
	reader.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	CHECK(reader.PeekUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "AB");
	CHECK(reader.ReadUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "AB");
	CHECK(reader.PeekUntilCharNotIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "  \r\n\t");
	reader.SkipUntilCharNotIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(!reader.AnyBytesLeft());
}

TEST_CASE("StringConsumer - parse int")
{
	StringConsumer reader("1 a -2 -2 ffffFFFF ffffFFFF -1aaaAAAA -1aaaAAAA +3 1234567890123 1234567890123 1234567890123 ffffFFFFffffFFFE ffffFFFFffffFFFE ffffFFFFffffFFFE ffffFFFFffffFFFE -0x1aaaAAAAaaaaAAAA -1234567890123 "sv);
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 1));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 1));
	CHECK(reader.TryReadIntegerBase<uint32_t>(10) == 1);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(1, 0xa));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(1, 0xa));
	CHECK(reader.ReadIntegerBase<uint32_t>(16) == 0xa);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(reader.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(reader.ReadIntegerBase<uint32_t>(10) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(reader.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(reader.ReadIntegerBase<int32_t>(10) == -2);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(8, 0xffffffff));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.TryReadIntegerBase<int32_t>(16) == std::nullopt);
	CHECK(reader.ReadIntegerBase<int32_t>(16) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.TryReadIntegerBase<uint32_t>(16) == 0xffffffff);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(9, -0x1aaaaaaa));
	CHECK(reader.TryReadIntegerBase<uint32_t>(16) == std::nullopt);
	CHECK(reader.ReadIntegerBase<uint32_t>(16) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(9, -0x1aaaaaaa));
	CHECK(reader.TryReadIntegerBase<uint32_t>(16) == std::nullopt);
	CHECK(reader.ReadIntegerBase<int32_t>(16) == -0x1aaaaaaa);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	reader.SkipIntegerBase(10);
	CHECK(reader.ReadUtf8() == '+');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 3));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 3));
	reader.SkipIntegerBase(10);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(reader.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(reader.ReadIntegerBase<uint32_t>(10) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(reader.TryReadIntegerBase<int32_t>(10) == std::nullopt);
	CHECK(reader.ReadIntegerBase<int32_t>(10) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(reader.ReadIntegerBase<uint64_t>(10) == 1234567890123);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(reader.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.ReadIntegerBase<uint32_t>(16) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(reader.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.ReadIntegerBase<int32_t>(16) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(reader.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.ReadIntegerBase<int64_t>(16) == 0);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(reader.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.ReadIntegerBase<uint64_t>(16) == 0xffffffff'fffffffe);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(2, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(2, 0));
	CHECK(reader.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(19, -0x1aaaaaaa'aaaaaaaa));
	CHECK(reader.ReadIntegerBase<int64_t>(0) == -0x1aaaaaaa'aaaaaaaa);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(14, -1234567890123));
	CHECK(reader.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(14, -1234567890123));
	CHECK(reader.ReadIntegerBase<int64_t>(0) == -1234567890123);
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	reader.SkipIntegerBase(10);
	reader.SkipIntegerBase(10);
	reader.SkipIntegerBase(0);
	reader.SkipIntegerBase(0);
	CHECK(reader.ReadIntegerBase<uint32_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<int32_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint64_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<int64_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint32_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<int32_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint64_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<int64_t>(0, 42) == 42);
	CHECK(reader.ReadUtf8() == ' ');
	CHECK(reader.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(reader.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(reader.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	reader.SkipIntegerBase(10);
	reader.SkipIntegerBase(10);
	CHECK(reader.ReadIntegerBase<uint32_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<int32_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint64_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<int64_t>(10, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint32_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<int32_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<uint64_t>(0, 42) == 42);
	CHECK(reader.ReadIntegerBase<int64_t>(0, 42) == 42);
}
