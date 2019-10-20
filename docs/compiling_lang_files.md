# How to compile lang files (OpenTTD and strgen)

Last updated:    2009-06-30

## strgen usage

This guide is only interesting for people who want to alter something
themselves without access to [translator.openttd.org](https://translator.openttd.org/).

Please note that your compiled language file will only be compatible with the OpenTTD version
you have downloaded `english.txt`, the master language file, for. While this is
not always true, namely when changes in the code have not touched language
files, your safest bet is to assume this 'limitation'.

As a first step you need to compile strgen. This is as easy as typing
`'make strgen'`. You can download the precompile strgen from:
[http://www.openttd.org/download-strgen](http://www.openttd.org/download-strgen)

strgen takes as argument a txt file and translates it to a lng file, allowing
it to be used inside OpenTTD. strgen needs the master language file
`english.txt` to work. Below are some examples of strgen usage.

## Examples

### Example 1

If you are in the root of your working copy (git repository), you should type
`./strgen/strgen -s lang lang/english.txt`
to compile `english.txt` into `english.lng`. It will be placed in the lang dir.

### Example 2

You only have the strgen executable (no working copy) and you want to compile
a txt file in the same directory. You should type
`./strgen english.txt`
and you will get and `english.lng` in the same dir.

### Example 3

You have strgen somewhere, `english.txt` in `/usr/openttd/lang` and you want the
resulting language file to go to /tmp. Use
`./strgen -s /usr/openttd/lang -d /tmp english.txt`

You can interchange `english.txt` to whichever language you want to generate a
.lng file for.

## strgen command switches

`-v | --version`
strgen will tell what git revision it was last modified

`-t | --todo`
strgen will add <TODO> to any untranslated/missing strings and use the english
strings while compiling the language file

`-w | --warning`
strgen will print any missing strings or wrongly translated (bad format)
to standard error output(stderr)

`-h | --help | -?`
Print out a summarized help message explaining these switches

`-s | --source_dir`
strgen will search for the master file english.txt in the directory specified
by this switch instead of the current directory

`-d | --dest_dir`
strgen will put <language>.lng in the directory specified by this switch; if
no dest_dir is given, output is the same as source_dir
