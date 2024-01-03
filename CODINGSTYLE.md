**Why a set coding style is important** <br>
This project is an open source project. To get open source working as intended, many people should be able to comprehend the code. This implies that there is a well documented and uniform flow of code.
That does not necessarily mean that a contributor should not write optimised yet cryptic code - one should always care about performance.  However, other developers need to understand the code which means complicated parts of code **must** have good documentation.

**Why we require that EVERYTHING is documented**<br>
What is simple to some might appear very complicated to others. Documentation helps these others. Anyone should be able to improve the code. But the main reason is, that when patch contributors want their patches added to the common codebase, the code needs to be reviewed by one or more developers. Documentation makes reviewing much easier.

## Coding style for OpenTTD
### Functions
* Function names use [CamelCase](http://www.wikipedia.org/wiki/Camelcase) without underscores.
* Opening curly bracket **{** for a function starts on the next line.
* Use Foo() instead of Foo(void).
```c++
void ThisIsAFunction()
{
}
```

### Variables
* Variable names are all lowercase, and use "_" as separator.
* Global variables are preceded by an underscore. ("_") Use descriptive names because leading underscores are often used for system / compiler variables.
* Own members of classes should always be referenced using "this->"
* Pointers and references should have their reference symbol next to the name (compatibility with current code).
* Variables that are declared below one another should have their type, name or reference operator, and assignment operator aligned by spaces.
* There are set names for many variables. Those are (but not limited to): Vehicle *u, *v, *w; Station *st; Town *t; Window *w; Engine *e.
* For multiple instances, use numbers "*t1, *t2" or postfixes "*st_from, *st_to".
* Declare variables upon first usage.
* Declare iterators in their loop.
* There is a space between '*' and 'const' in "const pointers"
```c++
int     number         = 10;
Vehicle *u_first_wagon = v->next;
int     value          = v->value;

uint32 _global_variable = 3750;

static const char * const _global_array[] = {
	"first string",
	"second string",
	"another string",
	"last string followed by comma",
};

protected:
	char protected_array[10];

for (int i = 0;;);
```

* Give the variables expedient names, this increases the readability of the code
```c++
bool is_maglev_train = true;
if (!is_maglev_train) DoSomething();
```

* Some people like to introduce copies of variables to increase readability, which can waste memory. However, in some cases, especially in code pieces which are often called, it makes sense to cache some calculated variables.
```c++
/* Unoptimized code:
 * foo is not touched inside the loop!
 */
for (uint8 i = 0; i < 100000; i++) {
	/* Do something */
	if (value_to_check == (foo * 4) % 5 + 6) DoSomething();
	/* Do something else */
}

/* Better:
 * The used value of foo is calculated outside the loop.
 */
const uint32 bar = (foo * 4) % 5 + 6;
for (uint8 i = 0; i < 100000; i++) {
	/* Do something */
	if (value_to_check == bar) DoSomething();
	/* Do something else */
}
```

### Enumerations / static consts
* Enumerations store integers that belong logically together (railtypes, string IDs, etc.).
* Enumeration names also use CamelCase.
* Unscoped enumerators are all caps with "_" between the words.
* Scoped enumerators are use CamelCase.
* Enums are not used to store single numbers.
* Enums have consecutive numbers OR
* Enums have consecutive powers of two. Powers of two (bits) are written in hex or with the shift operator.
* Enums may have special enumerators: "_BEGIN", "\_END", and "INVALID\_").  See example.
* The invalid always has 0xFF, 0xFFFF, 0xFFFFFFFF as a value.
* Other special values are consecutively less than the invalid.
* Variables that hold enumerators should have the type of the enumeration.
```c++
enum DiagDirection {
	DIAGDIR_BEGIN = 0,
	DIAGDIR_NE  = 0,
	DIAGDIR_SE  = 1,
	DIAGDIR_SW  = 2,
	DIAGDIR_NW  = 3,
	DIAGDIR_END,
	INVALID_DIAGDIR = 0xFF,
	BROKEN_DIAGDIR = 0xFE,
};

enum {
	DEPOT_SERVICE       = (1 << 0),
	DEPOT_MASS_SEND     = (1 << 1),
	DEPOT_DONT_CANCEL   = (1 << 2),
	DEPOT_LOCATE_HANGAR = (1 << 3),
};

DiagDirection enterdir = DIAGDIR_NE;
```

* Numbers that store single or uncorrelated data are static consts. Those may use the naming conventions of enums.
Example:
```c++
static const int MAXIMUM_STATIONS = 42;
```

* Enums are useful in GUI code: When widgets are enumerated, they are easier to access during many operations. Additionally changes caused by modified widget sequences require less code adapting. If a widget is used like this, its enum name should be present in a comment behind the corresponding widget definition.
```c++
/** Enum referring to the widgets of the build rail depot window */
enum BuildRailDepotWidgets {
	BRDW_CLOSEBOX = 0,
	BRDW_CAPTION,
	BRDW_BACKGROUND,
	BRDW_DEPOT_NE,
	BRDW_DEPOT_SE,
	BRDW_DEPOT_SW,
	BRDW_DEPOT_NW,
};
/* ... */
/** Widget definition of the build rail depot window */
static const Widget _build_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5, STR_..},   // BRDW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_...,  STR_...},  // BRDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,      STR_NULL}, // BRDW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,      STR_..},   // BRDW_DEPOT_NE
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,      STR_..},   // BRDW_DEPOT_SE
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,      STR_..},   // BRDW_DEPOT_SW
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,      STR_..},   // BRDW_DEPOT_NW
{   WIDGETS_END},
};
```

* Comments on the enum values should start with "///<" to enable doxygen documentation
```c++
enum SomeEnumeration {
	SE_BEGIN = 0,        ///< Begin of the enumeration, used for iterations
	SE_FOO = 0,          ///< Used for xy
	SE_BAR,              ///< Another value of the enumeration
	SE_SUB,              ///< Special case for z
	SE_END,              ///< End of the enumeration, used for iterations
};
```

### Control flow
* Put a space before the parentheses in **if**, **switch**, **for** and **while** statements.
* Use curly braces and put the contained statements on their own lines (e.g., don't put them directly after the **if**).
* Opening curly bracket **{** stays on the first line, closing curly bracket **}** gets a line to itself (except for the **}** preceding an **else**, which should be on the same line as the **else**).
* When only a single statement is contained, the brackets can be omitted. In this case, put the single statement on the same line as the preceding keyword (**if**, **while**, etc.). Note that this is only allowed for if statements without an **else** clause.
* All fall throughs must be documented, using a **FALLTHROUGH** define/macro.
* The NOT_REACHED() macro can be used in default constructs that should never be reached.
* Unconditional loops are written with **`for (;;) {`**

```c++
if (a == b) {
	Foo();
} else {
	Bar();
}

if (very_large_checks &&
		spread_over_two_lines) {
	Foo();
}

if (a == b) Foo();

switch (a) {
	case 0: DoSomethingShort(); break;

	case 1:
		DoSomething();
		FALLTHROUGH;

	case 2:
		DoMore();
		b = a;
		break;

	case 3: {
		int r = 2;

		DoEvenMore(a);
		FALLTHROUGH;
	}

	case 4: {
		int q = 345;

		DoIt();
		break;
	}

	default:
		NOT_REACHED();
}

for (int i = 0; i < 10; i++) {
	Foo();
	Bar();
}
```

### Classes
* Classes names also use CamelCase.
* Classes should have "public", "protected", and "private" sections.
* Within these section the order is: types, static const members, static members, members, constructors / destructors, static methods, methods.
* Deviations from above order are allowed when the code dictates it (e.g. a static const is needed for a typedef)
* Methods and members ought to be grouped logically.
* All those sorting rules sometimes conflict which one another. Please use common sense what increases legibility of the code in such a case.
* The method implementation should indicate if it is virtual or similar by using a comment.
* Very short methods can have one-line definition (if defined in the class scope).
```c++
class ThisIsAClass {
public:
	typedef Titem_   *ItemPtr;
private:
	static const int MAX_SIZE = 500;
	int              size;
	ItemPtr          *items;

public:
	explicit ThisIsAClass();
	~ThisIsAClass();

	int GetSize() { return this->size; }
	virtual void Method();
};

/*virtual*/ void Class::Method()
{
	this->size *= 2;
}
```

### Templates
Templates are a very powerful C++ tool, but they can easily confuse beginners. Thus:
* Templates are to be documented in a very clear and verbose manner. Never assume anything in the documentation.
* the template keyword and the template layout get a separate line. typenames are either "T" or preceded by a "T", integers get a single capital letter or a descriptive name preceded by "T".
```c++
template <typename T, typename Tsomething, int N, byte Tnumber_of_something>
int Func();
```

* If you are writing one or more template class in the dedicated header file, use file.hpp for its name instead of file.h. This will let others know that it is template library (includes also implementation), not just header with declarations.

### Other important rules
* Put a space before and after binary operators: "a + b", "a == b", "a & b", "a <<= b", etc.. Exceptions are ".", "->" and "[]" (no spaces) and "," (just space after it).
* Put parenthesis where it improves readability: "*(b++)" instead of "*b++", and "if ((a & b) && c == 2)" instead of "if (a & b && c == 2)".
* Do not put external declarations in implementation (i.e. cpp) files.
* Use const where possible.
* Do not typedef enums and structs.
* Don't treat non-flags as flags: use "if (char_pointer != nullptr && *char_pointer != '\0')", not "if (char_pointer && *char_pointer)".
* Use "free(p)" instead of "if (p != nullptr) free(p)". "free(nullptr)" doesn't hurt anyone.
* No trailing whitespace. The Github workflows will not allow tabs or space on the end of lines.
* Only use tabs to indent from the start of the line.
* Line length is unlimited. In practice it may be useful to split a long line. When splitting, add two tabs in front of the second part.
* The '#' of preprocessor instructions goes into the first column of a line. Indenting is done after the '#' (using tabs again).
* Use /* */ for single line comments.
* Use // at the end of a command line to indicate comments.
** However, use /* */ after # preprocessor statements as // causes warnings in some compilers and/or might have unwanted side effects.
* C++ is defined using the ASCII character set. Do not use other character sets, not even in comments.
* OpenTTD includes some Objective-C sources (*.mm, used by OSX), which has a special object method invocation syntax: "[ obj doStuff:foo ]". Please use spaces on the inside of the brackets to differentiate from the C array syntax.

## Documentation
We use [Doxygen](http://doxygen.org/) to automatically generate documentation from the source code. It scans the source files for *recognizable* comments.
* **Make your comments recognizable.**
Doxygen only comments starting with the following style:
```c++
/**
///<
```

Use /** for multi-line comment blocks. Use ///< for single line comments for variables. Use //!< for single-line comments in the NoAI .hpp files.
For comments blocks inside a function always use /* */ or //. Use // only if the comment is on the same line as an instruction, otherwise use /* */.

### Files
* Put a @file command in a JavaDoc style comment at the start of the file, followed by a description.
```c++
/**
 * @file
 * This is the brief description.
 * This is the detailed description here and on the following lines.
 */
```
> ### Warning
> If a file lacks a **file comment block** then NO entities in that file will be documented by Doxygen!

### Functions
* The documentation should be as close to the actual code as possible to maximise the chance of staying in sync.
	* Comments for functions go in the .cpp file.
	* Comments for inlines go in the .h/.hpp file.
* Small inlines can have a short 3 or 4 line JavaDoc style comment.
* Completely document larger functions.
* Document obvious parameters and return values too!

```c++
/**
 * A brief explanation of what the function does and/or what purpose it serves.
 * Then follows a more detailed explanation of the function that can span multiple lines.
 *
 * @param foo Explanation of the foo parameter
 * @param bar Explanation of the bar parameter
 * @return The sum of foo and bar (@return can be omitted if the return type is void)
 *
 * @see SomeOtherFunc()
 * @see SOME_ENUM
 *
 * @bug Some bug description
 * @bug Another bug description which continues in the next line
 *           and ends with the following blank line
 *
 * @todo Some to-do entry
 */
static int FooBar(int foo, int bar)
{
	return foo + bar;
}
```

### Classes
* Document structs similarly to functions:
```c++
/**
 * A short description of the struct.
 * More detailed description of the its usage.
 *
 * @see [link to anything of interest]
 */
struct foo {
}
```

### JavaDoc structural commands

This table shows the commands you should use with OpenTTD.  The full list is [here](http://www.stack.nl/~dimitri/doxygen/commands.html).

| Command | Action | Example |
| ------- | -------- | ------- |
| **@attention** | Starts a paragraph where a message that needs attention may be entered. The paragraph will be indented. | @attention Whales crossing! |
| **@brief** | Starts a paragraph that serves as a brief description. For classes and files the brief description will be used in lists and at the start of the documentation page. For class and file members, the brief description will be placed at the declaration of the member and prepended to the detailed description. A brief description may span several lines (although it is advised to keep it brief!). | @brief This is the brief description. |
| **@bug** | Starts a paragraph where one or more bugs may be reported. The paragraph will be indented. Multiple adjacent @bug commands will be joined into a single paragraph. Each bug description will start on a new line. Alternatively, one @bug command may mention several bugs. | @bug Memory leak in here? |
| **@note** | Starts a paragraph where a note can be entered. The paragraph will be indented. | @note Might be slow |
| **@todo** | Starts a paragraph where a TODO item is described. The description will also add an item to a separate TODO list. The two instances of the description will be cross-referenced. Each item in the TODO list will be preceded by a header that indicates the origin of the item. | @todo Better error checking |
| **@warning** | Starts a paragraph where one or more warning messages may be entered. The paragraph will be indented. | @warning Not thread safe! |
| | <small>**Function related commands**</small> | |
| **@return** | Starts a return value description for a function. | @return a character pointer |
| **@param** | Starts a parameter description for a function parameter with name <parameter-name>. Followed by a description of the parameter. The existence of the parameter is checked and a warning is given if the documentation of this (or any other) parameter is missing or not present in the function declaration or definition.<br><br>The @param command has an optional attribute specifying the direction of the attribute. Possible values are "in" and "out". | @param  n    The number of bytes to copy<br>@param[out] dest The memory area to copy to.<br>@param[in]  src  The memory area to copy from. |
| **@see** | Starts a paragraph where one or more cross-references to classes, functions, methods, variables, files or URL may be specified. Two names joined by either :: or # are understood as referring to a class and one of its members. One of several overloaded methods or constructors may be selected by including a parenthesized list of argument types after the method name. [Here](http://www.stack.nl/~dimitri/doxygen/autolink.html) you can find detailed information about this feature. | @see OtherFunc() |
| **@b** | Displays the following word using a bold font. Equivalent to &lt;b&gt;word&lt;/b&gt;. To put multiple words in bold use &lt;b&gt;multiple words&lt;/b&gt;.| ...@b this and @b that... |
| **@c / @p** | Displays the parameter <word> using a typewriter font. You can use this command to refer to member function parameters in the running text. To have multiple words in typewriter font use &lt;tt&gt;multiple words&lt;/tt&gt;. | ... the @p x and @p y coordinates are used to... |
| **@arg / @li** | This command has one argument that continues until the first blank line or until another @arg / @li is encountered. The command can be used to generate a simple, not nested list of arguments. Each argument should start with an @arg / @li command. | @arg @c AlignLeft left alignment.<br>@arg @c AlignCenter center alignment.<br>@arg @c AlignRight right alignment |
| **@n** | Forces a new line. Equivalent to and inspired by the printf function. |@n |

### More on Doxygen and JavaDoc

Doxygen knows two different kinds of comments:
* *Brief descriptions*: one-liners that describe the function roughly ([example](http://docs.openttd.org/annotated.html))
* *Detailed descriptions*: as the name suggests, these contain the detailed function/purpose of the entity they describe ([example](http://docs.openttd.org/structBridge.html))

You can omit either one or put them into different places but there's only one brief and one detailed description allowed for the same entity.

Doxygen knows three modes for documenting an entity:
* Before the entity
* After the entity
* In a different file

The latter is a little harder to maintain since the prototype of the entity it describes then is stored in several places (e.g. the .h file and the file with the descriptions). Also while it makes the code easier to read it also makes it easier to omit the important step of updating the description of an entity if it was changed - and we all know why that shouldn't happen ;)<br>
Because of those reasons, we will only use the first two documentation schemes.


Doxygen supports both Qt and JavaDoc comment styles:
* Qt style example: **int i; //!< The counter for the main loop**
* JavaDoc style example: **int i; /*\*< The counter for the main loop \*/**

It also supports more comment styles but those two are the ones which are standardized. For OTTD we'll be using the JavaDoc style. One of the reasons is that it has a feature that the Qt style doesn't offer: JavaDoc style comment blocks will automatically start a brief description which ends at the first dot followed by a space or new line. Everything after that will also be part of the detailed description.

The general structure of a JavaDoc style comment is
```c++
/**
 * This is the brief description. And this sentence contains some further explanations that will appear in the detailed description only.
 */
```

and the resulting descriptions of that block would be:
* *Brief description*: This is the brief description.
* *Detailed description*: This is the brief description. And this sentence contains some further explanations that will appear in the detailed description only.

The distinction between the brief and detailed descriptions is made by the dot followed by a space/newline, so if you want to use that inside the brief description you need to escape the space/newline:
```c++
/**
 * This is a brief description (e.g.\ using only a few words). Details go here.
 */
```

If you're doing a one-line comment, use:
```c++
int i; ///< This is the description.
```

Also in the comment block you can include so-called structural commands which tell Doxygen what follows. In general, their area of effect begins after the command word and ends when a blank line or some other command is encountered. Also, multiple occurrences of the same structural command within a comment block or the referring entity will be joined in the documentation output usually.

## Commit message
To achieve a coherent whole and to make changelog writing easier, here are some guidelines for commit messages.
There is a check-script on the git server (also available for clients, see below) to make sure we all follow those rules.

The first line of a message must match:
```
<keyword>( #<issue>| <commit>(, (<keyword> #<issue>|<commit>))*)?: ([<section])? <Details>
```
Keywords are:
* Add, Feature: Adding new stuff. Difference between "Feature" and "Add" is somewhat subjective. "Feature" for user-point-of-view stuff, "Add" for other.
* Change: Changing behaviour from user-point-of-view.
* Remove: Removing something from user-point-of-view.
* Codechange, Cleanup: Changes without intentional change of behaviour from user-point-of-view. Difference between "Codechange" and "Cleanup" is somewhat subjective.
* Fix, Revert: Fixing stuff.
* Doc, Update: Documentation changes, version increments, translator commits.
* Prepare: Preparation for bigger changes. Rarely used.

If you commit a fix for an [issue](https://github.com/OpenTTD/OpenTTD/issues), add the corresponding issue number in the form of #NNNN. Do it as well if you implement a feature with a matching entry.

In the case of bugfixes, if you know what revision the bug was introduced (eg regression), please mention that revision as well just after the prefix. Finding the trouble-causing revision is highly encouraged as it makes backporting/branching/releases that much easier.

To further structure the changelog, you can add sections. Example are:
* "Network" for network specific changes
* "NewGRF" for NewGRF additions
* "YAPP", "NPF", for changes in these features
* "OSX", "Debian", "win32", for OS-specific packaging changes

Further explanations, general bitching, etc. don't go into the first line. Use a new line for those.

Complete examples:
* Fix: [YAPF] Infinite loop in pathfinder.
* Fix #5926: [YAPF] Infinite loop in pathfinder.
* Fix 80dffae130: Warning about unsigned unary minus.
* Fix #6673, 99bb3a95b4: Store the map variety setting in the samegame.
* Revert d9065fbfbe, Fix #5922: ClientSizeChanged is only called via WndProcGdi which already has the mutex.
* Fix #1264, Fix #2037, Fix #2038, Fix #2110: Rewrite the autoreplace kernel.


## Other tips
### Remove trailing whitespace
To find out if/where you have trailing whitespace, you can use the following (unix/bash) command:
```
grep -n -R --include "*.[ch]" '[ 	]$' * | grep --invert-match ".diff" | grep --invert-match ".patch"
```
Automatically removing whitespace is also possible with the following shell script (Note that it only checks .c, .cpp, .h, .hpp and .mm files):
```
#!/bin/sh
IFS='
'
for i in Makefile `find . -name \*.c -o -name \*.cpp -o -name \*.h -o -name \*.hpp -o -name \*.mm`
do
  (
	echo '%s/[ 	]\{1,\}$/'
	echo w
	echo q
  ) | ed $i 2> /dev/null > /dev/null
done
```

### Install the client-side git commit hooks

The client-side hooks perform various checks when you commit changes locally.
* Whitespace and indentation checks.
* **Coding style** checks.

Get the hooks:
```
git clone https://github.com/OpenTTD/OpenTTD-git-hooks.git openttd_hooks
```

Install the hooks, assuming "openttd" is your work tree folder:
```
cd openttd/.git/hooks
ln -s -t . ../../../openttd_hooks/hooks/*
```

