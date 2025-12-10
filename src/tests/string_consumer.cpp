/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_consumer.cpp Test functionality from core/string_consumer. */

#include "../stdafx.h"

#include <iostream>

#include "../3rdparty/catch2/catch.hpp"

#include "../core/string_consumer.hpp"

#include "../safeguards.h"

TEST_CASE("StringConsumer - basic")
{
	StringConsumer consumer("ab"sv);
	CHECK(!consumer.AnyBytesRead());
	CHECK(consumer.GetBytesRead() == 0);
	CHECK(consumer.AnyBytesLeft());
	CHECK(consumer.GetBytesLeft() == 2);
	CHECK(consumer.GetOrigData() == "ab");
	CHECK(consumer.GetReadData() == "");
	CHECK(consumer.GetLeftData() == "ab");
	consumer.Skip(1);
	CHECK(consumer.AnyBytesRead());
	CHECK(consumer.GetBytesRead() == 1);
	CHECK(consumer.AnyBytesLeft());
	CHECK(consumer.GetBytesLeft() == 1);
	CHECK(consumer.GetOrigData() == "ab");
	CHECK(consumer.GetReadData() == "a");
	CHECK(consumer.GetLeftData() == "b");
	consumer.SkipAll();
	CHECK(consumer.AnyBytesRead());
	CHECK(consumer.GetBytesRead() == 2);
	CHECK(!consumer.AnyBytesLeft());
	CHECK(consumer.GetBytesLeft() == 0);
	CHECK(consumer.GetOrigData() == "ab");
	CHECK(consumer.GetReadData() == "ab");
	CHECK(consumer.GetLeftData() == "");
	consumer.Skip(1);
	CHECK(consumer.AnyBytesRead());
	CHECK(consumer.GetBytesRead() == 2);
	CHECK(!consumer.AnyBytesLeft());
	CHECK(consumer.GetBytesLeft() == 0);
	CHECK(consumer.GetOrigData() == "ab");
	CHECK(consumer.GetReadData() == "ab");
	CHECK(consumer.GetLeftData() == "");
}

TEST_CASE("StringConsumer - binary8")
{
	StringConsumer consumer("\xFF\xFE\xFD\0"sv);
	CHECK(consumer.PeekUint8() == 0xFF);
	CHECK(consumer.PeekSint8() == -1);
	CHECK(consumer.PeekChar() == static_cast<char>(-1));
	consumer.SkipUint8();
	CHECK(consumer.PeekUint8() == 0xFE);
	CHECK(consumer.PeekSint8() == -2);
	CHECK(consumer.PeekChar() == static_cast<char>(-2));
	CHECK(consumer.ReadUint8() == 0xFE);
	CHECK(consumer.PeekUint8() == 0xFD);
	CHECK(consumer.PeekSint8() == -3);
	CHECK(consumer.PeekChar() == static_cast<char>(-3));
	CHECK(consumer.ReadSint8() == -3);
	CHECK(consumer.PeekUint8() == 0);
	CHECK(consumer.PeekSint8() == 0);
	CHECK(consumer.PeekChar() == 0);
	CHECK(consumer.ReadChar() == 0);
	CHECK(consumer.PeekUint8() == std::nullopt);
	CHECK(consumer.PeekSint8() == std::nullopt);
	CHECK(consumer.PeekChar() == std::nullopt);
	CHECK(consumer.ReadUint8(42) == 42);
	consumer.SkipSint8();
	CHECK(consumer.ReadSint8(42) == 42);
	CHECK(consumer.ReadChar(42) == 42);
}

TEST_CASE("StringConsumer - binary16")
{
	StringConsumer consumer("\xFF\xFF\xFE\xFF\xFD\xFF"sv);
	CHECK(consumer.PeekUint16LE() == 0xFFFF);
	CHECK(consumer.PeekSint16LE() == -1);
	consumer.SkipUint16LE();
	CHECK(consumer.PeekUint16LE() == 0xFFFE);
	CHECK(consumer.PeekSint16LE() == -2);
	CHECK(consumer.ReadUint16LE() == 0xFFFE);
	CHECK(consumer.PeekUint16LE() == 0xFFFD);
	CHECK(consumer.PeekSint16LE() == -3);
	CHECK(consumer.ReadSint16LE() == -3);
	CHECK(consumer.PeekUint16LE() == std::nullopt);
	CHECK(consumer.PeekSint16LE() == std::nullopt);
	CHECK(consumer.ReadUint16LE(42) == 42);
	consumer.SkipSint16LE();
	CHECK(consumer.ReadSint16LE(42) == 42);
}

TEST_CASE("StringConsumer - binary32")
{
	StringConsumer consumer("\xFF\xFF\xFF\xFF\xFE\xFF\xFF\xFF\xFD\xFF\xFF\xFF"sv);
	CHECK(consumer.PeekUint32LE() == 0xFFFFFFFF);
	CHECK(consumer.PeekSint32LE() == -1);
	consumer.SkipUint32LE();
	CHECK(consumer.PeekUint32LE() == 0xFFFFFFFE);
	CHECK(consumer.PeekSint32LE() == -2);
	CHECK(consumer.ReadUint32LE() == 0xFFFFFFFE);
	CHECK(consumer.PeekUint32LE() == 0xFFFFFFFD);
	CHECK(consumer.PeekSint32LE() == -3);
	CHECK(consumer.ReadSint32LE() == -3);
	CHECK(consumer.PeekUint32LE() == std::nullopt);
	CHECK(consumer.PeekSint32LE() == std::nullopt);
	CHECK(consumer.ReadUint32LE(42) == 42);
	consumer.SkipSint32LE();
	CHECK(consumer.ReadSint32LE(42) == 42);
}

TEST_CASE("StringConsumer - binary64")
{
	StringConsumer consumer("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFD\xFF\xFF\xFF\xFF\xFF\xFF\xFF"sv);
	CHECK(consumer.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFF);
	CHECK(consumer.PeekSint64LE() == -1);
	consumer.SkipUint64LE();
	CHECK(consumer.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFE);
	CHECK(consumer.PeekSint64LE() == -2);
	CHECK(consumer.ReadUint64LE() == 0xFFFFFFFF'FFFFFFFE);
	CHECK(consumer.PeekUint64LE() == 0xFFFFFFFF'FFFFFFFD);
	CHECK(consumer.PeekSint64LE() == -3);
	CHECK(consumer.ReadSint64LE() == -3);
	CHECK(consumer.PeekUint64LE() == std::nullopt);
	CHECK(consumer.PeekSint64LE() == std::nullopt);
	CHECK(consumer.ReadUint64LE(42) == 42);
	consumer.SkipSint64LE();
	CHECK(consumer.ReadSint64LE(42) == 42);
}

TEST_CASE("StringConsumer - utf8")
{
	StringConsumer consumer("a\u1234\xFF\xFE""b"sv);
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(1, 'a'));
	consumer.SkipUtf8();
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(3, 0x1234));
	CHECK(consumer.ReadUtf8() == 0x1234);
	CHECK(consumer.PeekUint8() == 0xFF);
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	CHECK(consumer.ReadUtf8() == '?');
	CHECK(consumer.PeekUint8() == 0xFE);
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	consumer.SkipUtf8();
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(1, 'b'));
	CHECK(consumer.ReadUtf8() == 'b');
	CHECK(!consumer.AnyBytesLeft());
	CHECK(consumer.PeekUtf8() == std::pair<StringConsumer::size_type, char32_t>(0, 0));
	CHECK(consumer.ReadUtf8() == '?');
}

TEST_CASE("StringConsumer - conditions")
{
	StringConsumer consumer("ABCDabcde\u0234@@@gh\0\0\0ij\0\0\0kl"sv);
	CHECK(consumer.PeekIf("AB"));
	CHECK(consumer.PeekCharIf('A'));
	CHECK(consumer.PeekUtf8If('A'));
	CHECK(!consumer.PeekIf("CD"));
	CHECK(!consumer.ReadIf("CD"));
	consumer.SkipIf("CD");
	CHECK(consumer.ReadIf("AB"));
	CHECK(consumer.PeekIf("CD"));
	consumer.SkipIf("CD");
	CHECK(consumer.Peek(2) == "ab");
	CHECK(consumer.Read(2) == "ab");
	CHECK(consumer.Peek(2) == "cd");
	CHECK(consumer.Find("e\u0234") == 2);
	CHECK(consumer.Find("ab") == StringConsumer::npos);
	CHECK(consumer.FindChar('e') == 2);
	CHECK(consumer.FindChar('a') == StringConsumer::npos);
	CHECK(consumer.FindUtf8(0x234) == 3);
	CHECK(consumer.FindUtf8(0x1234) == StringConsumer::npos);
	consumer.Skip(2);
	CHECK(consumer.Peek(3) == "e\u0234");
	CHECK(consumer.PeekUntil("e", StringConsumer::READ_ALL_SEPARATORS) == "e");
	CHECK(consumer.PeekUntil("e", StringConsumer::READ_ONE_SEPARATOR) == "e");
	CHECK(consumer.PeekUntil("e", StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(consumer.PeekUntil("e", StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(consumer.PeekUntil("e", StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(consumer.PeekUntil("@", StringConsumer::READ_ALL_SEPARATORS) == "e\u0234@@@");
	CHECK(consumer.PeekUntil("@", StringConsumer::READ_ONE_SEPARATOR) == "e\u0234@");
	CHECK(consumer.PeekUntil("@", StringConsumer::KEEP_SEPARATOR) == "e\u0234");
	CHECK(consumer.PeekUntil("@", StringConsumer::SKIP_ONE_SEPARATOR) == "e\u0234");
	CHECK(consumer.PeekUntil("@", StringConsumer::SKIP_ALL_SEPARATORS) == "e\u0234");
	CHECK(consumer.ReadUntil("@", StringConsumer::KEEP_SEPARATOR) == "e\u0234");
	CHECK(consumer.ReadUntil("@", StringConsumer::READ_ONE_SEPARATOR) == "@");
	CHECK(consumer.ReadUntil("@", StringConsumer::READ_ALL_SEPARATORS) == "@@");
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::READ_ALL_SEPARATORS) == "gh\0\0\0"sv);
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::READ_ONE_SEPARATOR) == "gh\0"sv);
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::KEEP_SEPARATOR) == "gh");
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::SKIP_ONE_SEPARATOR) == "gh");
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::SKIP_ALL_SEPARATORS) == "gh");
	CHECK(consumer.ReadUntilChar('\0', StringConsumer::READ_ONE_SEPARATOR) == "gh\0"sv);
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::READ_ALL_SEPARATORS) == "\0\0"sv);
	CHECK(consumer.ReadUntilChar('\0', StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(consumer.PeekUntilChar('\0', StringConsumer::READ_ALL_SEPARATORS) == "\0"sv);
	consumer.SkipUntilUtf8(0, StringConsumer::READ_ALL_SEPARATORS);
	CHECK(consumer.PeekUntilUtf8(0, StringConsumer::KEEP_SEPARATOR) == "ij");
	consumer.SkipUntilUtf8(0, StringConsumer::SKIP_ALL_SEPARATORS);
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "kl");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "kl");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "kl");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "kl");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "kl");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "kl");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(consumer.PeekUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::READ_ALL_SEPARATORS) == "");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::READ_ONE_SEPARATOR) == "");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::KEEP_SEPARATOR) == "");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::SKIP_ONE_SEPARATOR) == "");
	CHECK(consumer.ReadUntilUtf8(0x234, StringConsumer::SKIP_ALL_SEPARATORS) == "");
	CHECK(consumer.Peek(2) == "");
	CHECK(consumer.Read(2) == "");
}

TEST_CASE("StringConsumer - ascii")
{
	StringConsumer consumer("abcdefgh  \r\n\tAB  \r\n\t"sv);
	CHECK(consumer.FindCharIn("dc") == 2);
	CHECK(consumer.FindCharIn("xy") == StringConsumer::npos);
	CHECK(consumer.FindCharNotIn("ba") == 2);
	CHECK(consumer.PeekUntilCharNotIn("ba") == "ab");
	CHECK(consumer.PeekUntilCharNotIn("dc") == "");
	CHECK(consumer.PeekUntilCharIn("ba") == "");
	CHECK(consumer.PeekUntilCharIn("dc") == "ab");
	CHECK(consumer.ReadUntilCharNotIn("dc") == "");
	CHECK(consumer.ReadUntilCharNotIn("ba") == "ab");
	CHECK(consumer.ReadUntilCharIn("dc") == "");
	CHECK(consumer.ReadUntilCharIn("fe") == "cd");
	CHECK(consumer.PeekIf("ef"));
	consumer.SkipUntilCharNotIn("ji");
	CHECK(consumer.PeekIf("ef"));
	consumer.SkipUntilCharNotIn("fe");
	CHECK(consumer.PeekIf("gh"));
	consumer.SkipUntilCharIn("hg");
	CHECK(consumer.PeekIf("gh"));
	consumer.SkipUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(consumer.PeekCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == ' ');
	CHECK(consumer.ReadCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == ' ');
	consumer.SkipCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(consumer.PeekUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\r");
	CHECK(consumer.ReadUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\r");
	consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	CHECK(consumer.PeekCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == '\n');
	CHECK(consumer.ReadCharIfIn(StringConsumer::WHITESPACE_OR_NEWLINE) == '\n');
	CHECK(consumer.PeekUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\t");
	CHECK(consumer.ReadUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE) == "\t");
	consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_NO_NEWLINE);
	CHECK(consumer.PeekUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "AB");
	CHECK(consumer.ReadUntilCharIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "AB");
	CHECK(consumer.PeekUntilCharNotIn(StringConsumer::WHITESPACE_OR_NEWLINE) == "  \r\n\t");
	consumer.SkipUntilCharNotIn(StringConsumer::WHITESPACE_OR_NEWLINE);
	CHECK(!consumer.AnyBytesLeft());
}

TEST_CASE("StringConsumer - parse int")
{
	StringConsumer consumer("1 a -a -2 -8 ffffFFFF ffffFFFF -1aaaAAAA -1aaaAAAA +3 1234567890123 1234567890123 1234567890123 ffffFFFFffffFFFE ffffFFFFffffFFFE ffffFFFFffffFFFE ffffFFFFffffFFFE -0x1aaaAAAAaaaaAAAA -1234567890123 "sv);
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(1, 1));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(1, 1));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(10) == 1);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(1, 0xa));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(1, 0xa));
	CHECK(consumer.ReadIntegerBase<uint32_t>(16) == 0xa);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(2, -0xa));
	CHECK(consumer.ReadIntegerBase<int32_t>(16) == -0xa);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(2, -2));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<uint32_t>(10) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(8) == std::nullopt);
	CHECK(consumer.TryReadIntegerBase<int32_t>(8) == std::nullopt);
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(2, -8));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<int32_t>(10) == -8);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(8, 0xffffffff));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16, true) == std::pair<StringConsumer::size_type, uint32_t>(8, 0xffffffff));
	CHECK(consumer.PeekIntegerBase<int32_t>(16, true) == std::pair<StringConsumer::size_type, int32_t>(8, 0x7fffffff));
	CHECK(consumer.TryReadIntegerBase<int32_t>(16) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<int32_t>(16) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.TryReadIntegerBase<uint32_t>(16) == 0xffffffff);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(9, -0x1aaaaaaa));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16, true) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16, true) == std::pair<StringConsumer::size_type, int32_t>(9, -0x1aaaaaaa));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(16) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<uint32_t>(16) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(9, -0x1aaaaaaa));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(16) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<int32_t>(16) == -0x1aaaaaaa);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(8) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(8) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	consumer.SkipIntegerBase(10);
	CHECK(consumer.ReadUtf8() == '+');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 3));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 3));
	consumer.SkipIntegerBase(10);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10, true) == std::pair<StringConsumer::size_type, uint32_t>(13, 0xffffffff));
	CHECK(consumer.PeekIntegerBase<int32_t>(10, true) == std::pair<StringConsumer::size_type, int32_t>(13, 0x7fffffff));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10, true) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(consumer.PeekIntegerBase<int64_t>(10, true) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(consumer.TryReadIntegerBase<uint32_t>(10) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<uint32_t>(10) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(consumer.TryReadIntegerBase<int32_t>(10) == std::nullopt);
	CHECK(consumer.ReadIntegerBase<int32_t>(10) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(13, 1234567890123));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(13, 1234567890123));
	CHECK(consumer.ReadIntegerBase<uint64_t>(10) == 1234567890123);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(consumer.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16, true) == std::pair<StringConsumer::size_type, uint32_t>(16, 0xffffffff));
	CHECK(consumer.PeekIntegerBase<int32_t>(16, true) == std::pair<StringConsumer::size_type, int32_t>(16, 0x7fffffff));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16, true) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(consumer.PeekIntegerBase<int64_t>(16, true) == std::pair<StringConsumer::size_type, int64_t>(16, 0x7fffffff'ffffffff));
	CHECK(consumer.ReadIntegerBase<uint32_t>(16) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(consumer.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.ReadIntegerBase<int32_t>(16) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(consumer.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.ReadIntegerBase<int64_t>(16) == 0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(16, 0xffffffff'fffffffe));
	CHECK(consumer.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.ReadIntegerBase<uint64_t>(16) == 0xffffffff'fffffffe);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(2, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(16) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(16) == std::pair<StringConsumer::size_type, int64_t>(2, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(19, -0x1aaaaaaa'aaaaaaaa));
	CHECK(consumer.ReadIntegerBase<int64_t>(0) == -0x1aaaaaaa'aaaaaaaa);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(14, -1234567890123));
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(14, -1234567890123));
	CHECK(consumer.ReadIntegerBase<int64_t>(0) == -1234567890123);
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	consumer.SkipIntegerBase(10);
	consumer.SkipIntegerBase(10);
	consumer.SkipIntegerBase(0);
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadIntegerBase<uint32_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int32_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint64_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int64_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint32_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int32_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint64_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int64_t>(0, 42) == 42);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(0) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(0) == std::pair<StringConsumer::size_type, int64_t>(0, 0));
	consumer.SkipIntegerBase(10);
	consumer.SkipIntegerBase(10);
	CHECK(consumer.ReadIntegerBase<uint32_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int32_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint64_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int64_t>(10, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint32_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int32_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<uint64_t>(0, 42) == 42);
	CHECK(consumer.ReadIntegerBase<int64_t>(0, 42) == 42);
}

TEST_CASE("StringConsumer - invalid int")
{
	StringConsumer consumer("x 0x - -0x 0y"sv);
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	consumer.SkipIntegerBase(0);
	consumer.SkipIntegerBase(10);
	consumer.SkipIntegerBase(16);
	CHECK(consumer.ReadUtf8() == 'x');
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(1, 0));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(2, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(2, 0));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(1, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(1, 0));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == 'y');
}

TEST_CASE("StringConsumer - most negative")
{
	StringConsumer consumer("-80000000 -0x80000000 -2147483648 -9223372036854775808"sv);
	CHECK(consumer.PeekIntegerBase<uint32_t>(16) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16) == std::pair<StringConsumer::size_type, int32_t>(9, 0x80000000));
	CHECK(consumer.PeekIntegerBase<uint32_t>(16, true) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(16, true) == std::pair<StringConsumer::size_type, int32_t>(9, 0x80000000));
	consumer.SkipIntegerBase(16);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(0) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(0) == std::pair<StringConsumer::size_type, int32_t>(11, 0x80000000));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(11, 0x80000000));
	consumer.SkipIntegerBase(0);
	CHECK(consumer.ReadUtf8() == ' ');
	CHECK(consumer.PeekIntegerBase<uint32_t>(10) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10) == std::pair<StringConsumer::size_type, int32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(10) == std::pair<StringConsumer::size_type, int64_t>(20, 0x80000000'00000000));
	CHECK(consumer.PeekIntegerBase<uint32_t>(10, true) == std::pair<StringConsumer::size_type, uint32_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int32_t>(10, true) == std::pair<StringConsumer::size_type, int32_t>(20, 0x80000000));
	CHECK(consumer.PeekIntegerBase<uint64_t>(10, true) == std::pair<StringConsumer::size_type, uint64_t>(0, 0));
	CHECK(consumer.PeekIntegerBase<int64_t>(10, true) == std::pair<StringConsumer::size_type, int64_t>(20, 0x80000000'00000000));
}
