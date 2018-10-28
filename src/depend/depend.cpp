/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file depend/depend.cpp Custom implementation of Makedepend.
 *
 * We previously used makedepend, but that could not handle the amount of
 * files we have and does not handle conditional includes in a sane manner.
 * This caused many link problems because not enough files were recompiled.
 * This has lead to the development of our own dependency generator. It is
 * meant to be a substitute to the (relatively slow) dependency generation
 * via gcc. It thus helps speeding up compilation. It will also ignore
 * system headers making it less error prone when system headers are moved
 * or renamed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <map>
#include <set>
#include <stack>
#include <cassert>

/**
 * Return the length of an fixed size array.
 * Unlike sizeof this function returns the number of elements
 * of the given type.
 *
 * @param x The pointer to the first element of the array
 * @return The number of elements
 */
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/**
 * Get the last element of an fixed size array.
 *
 * @param x The pointer to the first element of the array
 * @return The pointer to the last element of the array
 */
#define lastof(x) (&x[lengthof(x) - 1])

/**
 * Copies characters from one buffer to another.
 *
 * Copies the source string to the destination buffer with respect of the
 * terminating null-character and the last pointer to the last element in
 * the destination buffer. If the last pointer is set to NULL no boundary
 * check is performed.
 *
 * @note usage: strecpy(dst, src, lastof(dst));
 * @note lastof() applies only to fixed size arrays
 *
 * @param dst The destination buffer
 * @param src The buffer containing the string to copy
 * @param last The pointer to the last element of the destination buffer
 * @return The pointer to the terminating null-character in the destination buffer
 */
char *strecpy(char *dst, const char *src, const char *last)
{
	assert(dst <= last);
	while (dst != last && *src != '\0') {
		*dst++ = *src++;
	}
	*dst = '\0';

	if (dst == last && *src != '\0') {
		fprintf(stderr, "String too long for destination buffer\n");
		exit(-3);
	}
	return dst;
}

/**
 * Appends characters from one string to another.
 *
 * Appends the source string to the destination string with respect of the
 * terminating null-character and and the last pointer to the last element
 * in the destination buffer. If the last pointer is set to NULL no
 * boundary check is performed.
 *
 * @note usage: strecat(dst, src, lastof(dst));
 * @note lastof() applies only to fixed size arrays
 *
 * @param dst The buffer containing the target string
 * @param src The buffer containing the string to append
 * @param last The pointer to the last element of the destination buffer
 * @return The pointer to the terminating null-character in the destination buffer
 */
static char *strecat(char *dst, const char *src, const char *last)
{
	assert(dst <= last);
	while (*dst != '\0') {
		if (dst == last) return dst;
		dst++;
	}

	return strecpy(dst, src, last);
}

/**
 * Version of the standard free that accepts const pointers.
 * @param ptr The data to free.
 */
static inline void free(const void *ptr)
{
	free(const_cast<void *>(ptr));
}

#ifndef PATH_MAX
/** The maximum length of paths, if we don't know it. */
#	define PATH_MAX 260
#endif

/** Simple string comparator using strcmp as implementation */
struct StringCompare {
	/**
	 * Compare a to b using strcmp.
	 * @param a string to compare.
	 * @param b string to compare.
	 * @return whether a is less than b.
	 */
	bool operator () (const char *a, const char *b) const
	{
		return strcmp(a, b) < 0;
	}
};
/** Set of C-style strings. */
typedef std::set<const char*, StringCompare> StringSet;
/** Mapping of C-style string to a set of C-style strings. */
typedef std::map<const char*, StringSet*, StringCompare> StringMap;
/** Pair of C-style string and a set of C-style strings. */
typedef std::pair<const char*, StringSet*> StringMapItem;

/** Include directory to search in. */
static StringSet _include_dirs;
/** Files that have been parsed/handled with their dependencies. */
static StringMap _files;
/** Dependencies of headers. */
static StringMap _headers;
/** The current 'active' defines. */
static StringSet _defines;

/**
 * Helper class to read a file.
 */
class File {
public:
	/**
	 * Create the helper by opening the given file.
	 * @param filename the file to open
	 * @post the file is open; otherwise the application is killed.
	 */
	File(const char *filename)
	{
		this->fp = fopen(filename, "r");
		if (this->fp == NULL) {
			fprintf(stdout, "Could not open %s for reading\n", filename);
			exit(1);
		}
		this->dirname = strdup(filename);
		char *last = strrchr(this->dirname, '/');
		if (last != NULL) {
			*last = '\0';
		} else {
			*this->dirname = '\0';
		}
	}

	/** Free everything we have allocated. */
	~File()
	{
		fclose(this->fp);
		free(this->dirname);
	}

	/**
	 * Get a single character from the file.
	 * If we are reading beyond the end of the file '\0' is returned.
	 * @return the read character.
	 */
	char GetChar() const
	{
		int c = fgetc(this->fp);
		return (c == EOF) ? '\0' : c;
	}

	/**
	 * Get the directory name of the file.
	 * @return the directory name.
	 */
	const char *GetDirname() const
	{
		return this->dirname;
	}

private:
	FILE *fp;             ///< The currently opened file.
	char *dirname;        ///< The directory of the file.
};

/** A token returned by the tokenizer. */
enum Token {
	TOKEN_UNKNOWN,    ///< Unknown token
	TOKEN_END,        ///< End of document
	TOKEN_EOL,        ///< End of line
	TOKEN_SHARP,      ///< # character, usually telling something important comes.
	TOKEN_LOCAL,      ///< Read a local include
	TOKEN_GLOBAL,     ///< Read a global include
	TOKEN_IDENTIFIER, ///< Identifier within the data.
	TOKEN_DEFINE,     ///< \c \#define in code
	TOKEN_IF,         ///< \c \#if in code
	TOKEN_IFDEF,      ///< \c \#ifdef in code
	TOKEN_IFNDEF,     ///< \c \#ifndef in code
	TOKEN_ELIF,       ///< \c \#elif in code
	TOKEN_ELSE,       ///< \c \#else in code
	TOKEN_ENDIF,      ///< \c \#endif in code
	TOKEN_UNDEF,      ///< \c \#undef in code
	TOKEN_OR,         ///< '||' within \c \#if expression
	TOKEN_AND,        ///< '&&' within \c \#if expression
	TOKEN_DEFINED,    ///< 'defined' within \c \#if expression
	TOKEN_OPEN,       ///< '(' within \c \#if expression
	TOKEN_CLOSE,      ///< ')' within \c \#if expression
	TOKEN_NOT,        ///< '!' within \c \#if expression
	TOKEN_ZERO,       ///< '0' within \c \#if expression
	TOKEN_INCLUDE,    ///< \c \#include in code
};

/** Mapping from a C-style keyword representation to a Token. */
typedef std::map<const char*, Token, StringCompare> KeywordList;

/**
 * Lexer of a file.
 */
class Lexer {
public:
	/**
	 * Create the lexer and fill the keywords table.
	 * @param file the file to read from.
	 */
	Lexer(const File *file) : file(file), current_char('\0'), string(NULL), token(TOKEN_UNKNOWN)
	{
		this->keywords["define"]  = TOKEN_DEFINE;
		this->keywords["defined"] = TOKEN_DEFINED;
		this->keywords["if"]      = TOKEN_IF;
		this->keywords["ifdef"]   = TOKEN_IFDEF;
		this->keywords["ifndef"]  = TOKEN_IFNDEF;
		this->keywords["include"] = TOKEN_INCLUDE;
		this->keywords["elif"]    = TOKEN_ELIF;
		this->keywords["else"]    = TOKEN_ELSE;
		this->keywords["endif"]   = TOKEN_ENDIF;
		this->keywords["undef"]   = TOKEN_UNDEF;

		/* Initialise currently read character. */
		this->Next();

		/* Allocate the buffer. */
		this->buf_len = 32;
		this->buf = (char*)malloc(sizeof(*this->buf) * this->buf_len);
	}

	/** Free everything */
	~Lexer()
	{
		free(this->buf);
	}

	/**
	 * Read the next character into 'current_char'.
	 */
	void Next()
	{
		this->current_char = this->file->GetChar();
	}

	/**
	 * Get the current token.
	 * @return the token.
	 */
	Token GetToken() const
	{
		return this->token;
	}

	/**
	 * Read the currenty processed string.
	 * @return the string, can be NULL.
	 */
	const char *GetString() const
	{
		return this->string;
	}

	/**
	 * Perform the lexing/tokenizing of the file till we can return something
	 * that must be parsed.
	 */
	void Lex()
	{
		for (;;) {
			free(this->string);
			this->string = NULL;
			this->token  = TOKEN_UNKNOWN;

			switch (this->current_char) {
				/* '\0' means End-Of-File */
				case '\0': this->token = TOKEN_END; return;

				/* Skip some chars, as they don't do anything */
				case '\t': this->Next(); break;
				case '\r': this->Next(); break;
				case ' ':  this->Next(); break;

				case '\\':
					this->Next();
					if (this->current_char == '\n') this->Next();
					break;

				case '\n':
					this->token = TOKEN_EOL;
					this->Next();
					return;

				case '#':
					this->token = TOKEN_SHARP;
					this->Next();
					return;

				case '"':
					this->ReadString('"', TOKEN_LOCAL);
					this->Next();
					return;

				case '<':
					this->ReadString('>', TOKEN_GLOBAL);
					this->Next();
					return;

				case '&':
					this->Next();
					if (this->current_char == '&') {
						this->Next();
						this->token = TOKEN_AND;
						return;
					}
					break;

				case '|':
					this->Next();
					if (this->current_char == '|') {
						this->Next();
						this->token = TOKEN_OR;
						return;
					}
					break;

				case '(':
					this->Next();
					this->token = TOKEN_OPEN;
					return;

				case ')':
					this->Next();
					this->token = TOKEN_CLOSE;
					return;

				case '!':
					this->Next();
					if (this->current_char != '=') {
						this->token = TOKEN_NOT;
						return;
					}
					break;

				/* Possible begin of comment */
				case '/':
					this->Next();
					switch (this->current_char) {
						case '*': {
							this->Next();
							char previous_char = '\0';
							while ((this->current_char != '/' || previous_char != '*') && this->current_char != '\0') {
								previous_char = this->current_char;
								this->Next();
							}
							this->Next();
							break;
						}
						case '/': while (this->current_char != '\n' && this->current_char != '\0') this->Next(); break;
						default: break;
					}
					break;

				default:
					if (isalpha(this->current_char) || this->current_char == '_') {
						/* If the name starts with a letter, it is an identifier */
						this->ReadIdentifier();
						return;
					}
					if (isdigit(this->current_char)) {
						bool zero = this->current_char == '0';
						this->Next();
						if (this->current_char == 'x' || this->current_char == 'X') Next();
						while (isdigit(this->current_char) || this->current_char == '.' || (this->current_char >= 'a' && this->current_char <= 'f') || (this->current_char >= 'A' && this->current_char <= 'F')) {
							zero &= this->current_char == '0';
							this->Next();
						}
						if (zero) this->token = TOKEN_ZERO;
						return;
					}
					this->Next();
					break;
			}
		}
	}

private:
	/**
	 * The token based on keyword with a given name.
	 * @param name the actual keyword.
	 * @return the token of the keyword.
	 */
	Token FindKeyword(const char *name) const
	{
		KeywordList::const_iterator it = this->keywords.find(name);
		if (it == this->keywords.end()) return TOKEN_IDENTIFIER;
		return (*it).second;
	}

	/**
	 * Read an identifier.
	 */
	void ReadIdentifier()
	{
		size_t count = 0;

		/* Read the rest of the identifier */
		do {
			this->buf[count++] = this->current_char;
			this->Next();

			if (count >= buf_len) {
				/* Scale the buffer if required */
				this->buf_len *= 2;
				this->buf = (char *)realloc(this->buf, sizeof(*this->buf) * this->buf_len);
			}
		} while ((isalpha(this->current_char) || this->current_char == '_' || isdigit(this->current_char)));
		this->buf[count] = '\0';

		free(this->string);
		this->string = strdup(this->buf);
		this->token = FindKeyword(this->string);
	}

	/**
	 * Read a string up to a given character, then set the given token.
	 * @param end the 'marker' for the end of the string.
	 * @param token the token to set after returning.
	 */
	void ReadString(char end, Token token)
	{
		size_t count = 0;
		this->Next();
		while (this->current_char != end && this->current_char != ')' && this->current_char != '\n' && this->current_char != '\0') {
			this->buf[count++] = this->current_char;
			this->Next();

			if (count >= this->buf_len) {
				/* Scale the buffer if required */
				this->buf_len *= 2;
				this->buf = (char *)realloc(this->buf, sizeof(*this->buf) * this->buf_len);
			}
		}
		this->buf[count] = '\0';
		free(this->string);
		this->string = strdup(this->buf);
		this->token = token;
	}

	const File *file;     ///< The file to read from.
	char current_char;    ///< The current character to process.
	char *string;         ///< Currently processed string.
	Token token;          ///< The current token to process.
	char *buf;            ///< Temporary buffer.
	size_t buf_len;       ///< Length of the temporary buffer.
	KeywordList keywords; ///< All keywords we know of.
};

/**
 * Generate a path from a directory name and a relative filename.
 * If the file is not local the include directory names will be used instead
 * of the passed parameter with directory name. If the file is local both will
 * be queried where the parameter takes precedence.
 * @param dirname the directory to look in.
 * @param filename the file to look for.
 * @param local whether to look locally (in dirname) for the file.
 * @return the absolute path, or NULL if the file doesn't exist.
 */
const char *GeneratePath(const char *dirname, const char *filename, bool local)
{
	if (local) {
		if (access(filename, R_OK) == 0) return strdup(filename);

		char path[PATH_MAX];
		strecpy(path, dirname, lastof(path));
		const char *p = filename;
		/* Remove '..' from the begin of the filename. */
		while (*p == '.') {
			if (*(++p) == '.') {
				char *s = strrchr(path, '/');
				if (s != NULL) *s = '\0';
				p += 2;
			}
		}
		strecat(path, "/", lastof(path));
		strecat(path, p, lastof(path));

		if (access(path, R_OK) == 0) return strdup(path);
	}

	for (StringSet::iterator it = _include_dirs.begin(); it != _include_dirs.end(); it++) {
		char path[PATH_MAX];
		strecpy(path, *it, lastof(path));
		const char *p = filename;
		/* Remove '..' from the begin of the filename. */
		while (*p == '.') {
			if (*(++p) == '.') {
				char *s = strrchr(path, '/');
				if (s != NULL) *s = '\0';
				p += 2;
			}
		}
		strecat(path, "/", lastof(path));
		strecat(path, p, lastof(path));

		if (access(path, R_OK) == 0) return strdup(path);
	}

	return NULL;
}

/**
 * Try to parse a 'defined(expr)' expression.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionDefined(Lexer *lexer, StringSet *defines, bool verbose);

/**
 * Try to parse a 'expr || expr' expression.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionOr(Lexer *lexer, StringSet *defines, bool verbose);

/**
 * Try to parse a '!expr' expression. Also parses the '(expr)', '0' and
 * identifiers. Finally it also consumes any unknown tokens.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionNot(Lexer *lexer, StringSet *defines, bool verbose)
{
	if (lexer->GetToken() == TOKEN_NOT) {
		if (verbose) fprintf(stderr, "!");
		lexer->Lex();
		bool value = !ExpressionDefined(lexer, defines, verbose);
		if (verbose) fprintf(stderr, "[%d]", value);
		return value;
	}

	if (lexer->GetToken() == TOKEN_OPEN) {
		if (verbose) fprintf(stderr, "(");
		lexer->Lex();
		bool value = ExpressionOr(lexer, defines, verbose);
		if (verbose) fprintf(stderr, ")[%d]", value);
		lexer->Lex();
		return value;
	}

	if (lexer->GetToken() == TOKEN_ZERO) {
		if (verbose) fprintf(stderr, "0");
		lexer->Lex();
		if (verbose) fprintf(stderr, "[0]");
		return false;
	}

	bool first = true;
	while (lexer->GetToken() == TOKEN_UNKNOWN || lexer->GetToken() == TOKEN_IDENTIFIER) {
		if (verbose && first) fprintf(stderr, "<assumed true>");
		first = false;
		lexer->Lex();
	}

	return true;
}

/**
 * Try to parse a 'defined(expr)' expression.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionDefined(Lexer *lexer, StringSet *defines, bool verbose)
{
	bool value = ExpressionNot(lexer, defines, verbose);

	if (lexer->GetToken() != TOKEN_DEFINED) return value;
	lexer->Lex();
	if (verbose) fprintf(stderr, "defined");
	bool open = (lexer->GetToken() == TOKEN_OPEN);
	if (open) lexer->Lex();
	if (verbose) fprintf(stderr, open ? "(" : " ");
	if (lexer->GetToken() == TOKEN_IDENTIFIER) {
		if (verbose) fprintf(stderr, "%s", lexer->GetString());
		value = defines->find(lexer->GetString()) != defines->end();
	}
	if (open) {
		if (verbose) fprintf(stderr, ")");
		lexer->Lex();
	}
	lexer->Lex();
	if (verbose) fprintf(stderr, "[%d]", value);
	return value;
}

/**
 * Try to parse a 'expr && expr' expression.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionAnd(Lexer *lexer, StringSet *defines, bool verbose)
{
	bool value = ExpressionDefined(lexer, defines, verbose);

	for (;;) {
		if (lexer->GetToken() != TOKEN_AND) return value;
		if (verbose) fprintf(stderr, " && ");
		lexer->Lex();
		value = value && ExpressionDefined(lexer, defines, verbose);
	}
}

/**
 * Try to parse a 'expr || expr' expression.
 * @param lexer the lexer to get tokens from.
 * @param defines the set of known defines.
 * @param verbose whether to give verbose debugging information.
 * @return the value of the expression.
 */
bool ExpressionOr(Lexer *lexer, StringSet *defines, bool verbose)
{
	bool value = ExpressionAnd(lexer, defines, verbose);

	for (;;) {
		if (lexer->GetToken() != TOKEN_OR) return value;
		if (verbose) fprintf(stderr, " || ");
		lexer->Lex();
		value = value || ExpressionAnd(lexer, defines, verbose);
	}
}

/** Enumerator to tell how long to ignore 'stuff'. */
enum Ignore {
	NOT_IGNORE,         ///< No ignoring.
	IGNORE_UNTIL_ELSE,  ///< Ignore till a \c \#else is reached.
	IGNORE_UNTIL_ENDIF, ///< Ignore till a \c \#endif is reached.
};

/**
 * Scan a file for includes, defines and the lot.
 * @param filename the name of the file to scan.
 * @param ext the extension of the filename.
 * @param header whether the file is a header or not.
 * @param verbose whether to give verbose debugging information.
 */
void ScanFile(const char *filename, const char *ext, bool header, bool verbose)
{
	static StringSet defines;
	static std::stack<Ignore> ignore;
	/* Copy in the default defines (parameters of depend) */
	if (!header) {
		for (StringSet::iterator it = _defines.begin(); it != _defines.end(); it++) {
			defines.insert(strdup(*it));
		}
	}

	File file(filename);
	Lexer lexer(&file);

	/* Start the lexing! */
	lexer.Lex();

	while (lexer.GetToken() != TOKEN_END) {
		switch (lexer.GetToken()) {
			/* We reached the end of the file... yay, we're done! */
			case TOKEN_END: break;

			/* The line started with a # (minus whitespace) */
			case TOKEN_SHARP:
				lexer.Lex();
				switch (lexer.GetToken()) {
					case TOKEN_INCLUDE:
						if (verbose) fprintf(stderr, "%s #include ", filename);
						lexer.Lex();
						switch (lexer.GetToken()) {
							case TOKEN_LOCAL:
							case TOKEN_GLOBAL: {
								if (verbose) fprintf(stderr, "%s", lexer.GetString());
								if (!ignore.empty() && ignore.top() != NOT_IGNORE) {
									if (verbose) fprintf(stderr, " (ignored)");
									break;
								}
								const char *h = GeneratePath(file.GetDirname(), lexer.GetString(), lexer.GetToken() == TOKEN_LOCAL);
								if (h != NULL) {
									StringMap::iterator it = _headers.find(h);
									if (it == _headers.end()) {
										it = (_headers.insert(StringMapItem(strdup(h), new StringSet()))).first;
										if (verbose) fprintf(stderr, "\n");
										ScanFile(h, ext, true, verbose);
									}
									StringMap::iterator curfile;
									if (header) {
										curfile = _headers.find(filename);
									} else {
										/* Replace the extension with the provided extension of '.o'. */
										char path[PATH_MAX];
										strecpy(path, filename, lastof(path));
										*(strrchr(path, '.')) = '\0';
										strecat(path, ext != NULL ? ext : ".o", lastof(path));
										curfile = _files.find(path);
										if (curfile == _files.end()) {
											curfile = (_files.insert(StringMapItem(strdup(path), new StringSet()))).first;
										}
									}
									if (it != _headers.end()) {
										for (StringSet::iterator header = it->second->begin(); header != it->second->end(); header++) {
											if (curfile->second->find(*header) == curfile->second->end()) curfile->second->insert(strdup(*header));
										}
									}
									if (curfile->second->find(h) == curfile->second->end()) curfile->second->insert(strdup(h));
									free(h);
								}
							}
							/* FALL THROUGH */
							default: break;
						}
						break;

					case TOKEN_DEFINE:
						if (verbose) fprintf(stderr, "%s #define ", filename);
						lexer.Lex();
						if (lexer.GetToken() == TOKEN_IDENTIFIER) {
							if (verbose) fprintf(stderr, "%s", lexer.GetString());
							if (!ignore.empty() && ignore.top() != NOT_IGNORE) {
								if (verbose) fprintf(stderr, " (ignored)");
								break;
							}
							if (defines.find(lexer.GetString()) == defines.end()) defines.insert(strdup(lexer.GetString()));
							lexer.Lex();
						}
						break;

					case TOKEN_UNDEF:
						if (verbose) fprintf(stderr, "%s #undef ", filename);
						lexer.Lex();
						if (lexer.GetToken() == TOKEN_IDENTIFIER) {
							if (verbose) fprintf(stderr, "%s", lexer.GetString());
							if (!ignore.empty() && ignore.top() != NOT_IGNORE) {
								if (verbose) fprintf(stderr, " (ignored)");
								break;
							}
							StringSet::iterator it = defines.find(lexer.GetString());
							if (it != defines.end()) {
								free(*it);
								defines.erase(it);
							}
							lexer.Lex();
						}
						break;

					case TOKEN_ENDIF:
						if (verbose) fprintf(stderr, "%s #endif", filename);
						lexer.Lex();
						if (!ignore.empty()) ignore.pop();
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;

					case TOKEN_ELSE: {
						if (verbose) fprintf(stderr, "%s #else", filename);
						lexer.Lex();
						Ignore last = ignore.empty() ? NOT_IGNORE : ignore.top();
						if (!ignore.empty()) ignore.pop();
						if (ignore.empty() || ignore.top() == NOT_IGNORE) {
							ignore.push(last == IGNORE_UNTIL_ELSE ? NOT_IGNORE : IGNORE_UNTIL_ENDIF);
						} else {
							ignore.push(IGNORE_UNTIL_ENDIF);
						}
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;
					}

					case TOKEN_ELIF: {
						if (verbose) fprintf(stderr, "%s #elif ", filename);
						lexer.Lex();
						Ignore last = ignore.empty() ? NOT_IGNORE : ignore.top();
						if (!ignore.empty()) ignore.pop();
						if (ignore.empty() || ignore.top() == NOT_IGNORE) {
							bool value = ExpressionOr(&lexer, &defines, verbose);
							ignore.push(last == IGNORE_UNTIL_ELSE ? (value ? NOT_IGNORE : IGNORE_UNTIL_ELSE) : IGNORE_UNTIL_ENDIF);
						} else {
							ignore.push(IGNORE_UNTIL_ENDIF);
						}
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;
					}

					case TOKEN_IF: {
						if (verbose) fprintf(stderr, "%s #if ", filename);
						lexer.Lex();
						if (ignore.empty() || ignore.top() == NOT_IGNORE) {
							bool value = ExpressionOr(&lexer, &defines, verbose);
							ignore.push(value ? NOT_IGNORE : IGNORE_UNTIL_ELSE);
						} else {
							ignore.push(IGNORE_UNTIL_ENDIF);
						}
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;
					}

					case TOKEN_IFDEF:
						if (verbose) fprintf(stderr, "%s #ifdef ", filename);
						lexer.Lex();
						if (lexer.GetToken() == TOKEN_IDENTIFIER) {
							bool value = defines.find(lexer.GetString()) != defines.end();
							if (verbose) fprintf(stderr, "%s[%d]", lexer.GetString(), value);
							if (ignore.empty() || ignore.top() == NOT_IGNORE) {
								ignore.push(value ? NOT_IGNORE : IGNORE_UNTIL_ELSE);
							} else {
								ignore.push(IGNORE_UNTIL_ENDIF);
							}
						}
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;

					case TOKEN_IFNDEF:
						if (verbose) fprintf(stderr, "%s #ifndef ", filename);
						lexer.Lex();
						if (lexer.GetToken() == TOKEN_IDENTIFIER) {
							bool value = defines.find(lexer.GetString()) != defines.end();
							if (verbose) fprintf(stderr, "%s[%d]", lexer.GetString(), value);
							if (ignore.empty() || ignore.top() == NOT_IGNORE) {
								ignore.push(!value ? NOT_IGNORE : IGNORE_UNTIL_ELSE);
							} else {
								ignore.push(IGNORE_UNTIL_ENDIF);
							}
						}
						if (verbose) fprintf(stderr, " -> %signore", (!ignore.empty() && ignore.top() != NOT_IGNORE) ? "" : "not ");
						break;

					default:
						if (verbose) fprintf(stderr, "%s #<unknown>", filename);
						lexer.Lex();
						break;
				}
				if (verbose) fprintf(stderr, "\n");
				/* FALL THROUGH */
			default:
				/* Ignore the rest of the garbage on this line */
				while (lexer.GetToken() != TOKEN_EOL && lexer.GetToken() != TOKEN_END) lexer.Lex();
				lexer.Lex();
				break;
		}
	}

	if (!header) {
		for (StringSet::iterator it = defines.begin(); it != defines.end(); it++) {
			free(*it);
		}
		defines.clear();
		while (!ignore.empty()) ignore.pop();
	}
}

/**
 * Entry point. Arguably the most common function in all applications.
 * @param argc the number of arguments.
 * @param argv the actual arguments.
 * @return return value for the caller to tell we succeed or not.
 */
int main(int argc, char *argv[])
{
	bool ignorenext = true;
	char *filename = NULL;
	char *ext = NULL;
	char *delimiter = NULL;
	bool append = false;
	bool verbose = false;

	for (int i = 0; i < argc; i++) {
		if (ignorenext) {
			ignorenext = false;
			continue;
		}
		if (argv[i][0] == '-') {
			/* Append */
			if (strncmp(argv[i], "-a", 2) == 0) append = true;
			/* Include dir */
			if (strncmp(argv[i], "-I", 2) == 0) {
				if (argv[i][2] == '\0') {
					i++;
					_include_dirs.insert(strdup(argv[i]));
				} else {
					_include_dirs.insert(strdup(&argv[i][2]));
				}
				continue;
			}
			/* Define */
			if (strncmp(argv[i], "-D", 2) == 0) {
				char *p = strchr(argv[i], '=');
				if (p != NULL) *p = '\0';
				_defines.insert(strdup(&argv[i][2]));
				continue;
			}
			/* Output file */
			if (strncmp(argv[i], "-f", 2) == 0) {
				if (filename != NULL) continue;
				filename = strdup(&argv[i][2]);
				continue;
			}
			/* Object file extension */
			if (strncmp(argv[i], "-o", 2) == 0) {
				if (ext != NULL) continue;
				ext = strdup(&argv[i][2]);
				continue;
			}
			/* Starting string delimiter */
			if (strncmp(argv[i], "-s", 2) == 0) {
				if (delimiter != NULL) continue;
				delimiter = strdup(&argv[i][2]);
				continue;
			}
			/* Verbose */
			if (strncmp(argv[i], "-v", 2) == 0) verbose = true;
			continue;
		}
		ScanFile(argv[i], ext, false, verbose);
	}

	/* Default output file is Makefile */
	if (filename == NULL) filename = strdup("Makefile");

	/* Default delimiter string */
	if (delimiter == NULL) delimiter = strdup("# DO NOT DELETE");

	char backup[PATH_MAX];
	strecpy(backup, filename, lastof(backup));
	strecat(backup, ".bak", lastof(backup));

	char *content = NULL;
	long size = 0;

	/* Read in the current file; so we can overwrite everything from the
	 * end of non-depend data marker down till the end. */
	FILE *src = fopen(filename, "rb");
	if (src != NULL) {
		fseek(src, 0, SEEK_END);
		if ((size = ftell(src)) < 0) {
			fprintf(stderr, "Could not read %s\n", filename);
			exit(-2);
		}
		rewind(src);
		content = (char*)malloc(size * sizeof(*content));
		if (fread(content, 1, size, src) != (size_t)size) {
			fprintf(stderr, "Could not read %s\n", filename);
			exit(-2);
		}
		fclose(src);
	}

	FILE *dst = fopen(filename, "w");
	bool found_delimiter = false;

	if (size != 0) {
		src = fopen(backup, "wb");
		if (fwrite(content, 1, size, src) != (size_t)size) {
			fprintf(stderr, "Could not write %s\n", filename);
			exit(-2);
		}
		fclose(src);

		/* Then append it to the real file. */
		src = fopen(backup, "rb");
		while (fgets(content, size, src) != NULL) {
			fputs(content, dst);
			if (!strncmp(content, delimiter, strlen(delimiter))) found_delimiter = true;
			if (!append && found_delimiter) break;
		}
		fclose(src);
	}
	if (!found_delimiter) fprintf(dst, "\n%s\n", delimiter);

	for (StringMap::iterator it = _files.begin(); it != _files.end(); it++) {
		for (StringSet::iterator h = it->second->begin(); h != it->second->end(); h++) {
			fprintf(dst, "%s: %s\n", it->first, *h);
		}
	}

	/* Clean up our mess. */
	fclose(dst);

	free(delimiter);
	free(filename);
	free(ext);
	free(content);

	for (StringMap::iterator it = _files.begin(); it != _files.end(); it++) {
		for (StringSet::iterator h = it->second->begin(); h != it->second->end(); h++) {
			free(*h);
		}
		it->second->clear();
		delete it->second;
		free(it->first);
	}
	_files.clear();

	for (StringMap::iterator it = _headers.begin(); it != _headers.end(); it++) {
		for (StringSet::iterator h = it->second->begin(); h != it->second->end(); h++) {
			free(*h);
		}
		it->second->clear();
		delete it->second;
		free(it->first);
	}
	_headers.clear();

	for (StringSet::iterator it = _defines.begin(); it != _defines.end(); it++) {
		free(*it);
	}
	_defines.clear();

	for (StringSet::iterator it = _include_dirs.begin(); it != _include_dirs.end(); it++) {
		free(*it);
	}
	_include_dirs.clear();

	return 0;
}
