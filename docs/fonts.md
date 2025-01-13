# Fonts for OpenTTD

OpenTTD uses four different fonts:
- a medium font (used for most texts in the game)
- a small font (used for the smallmap legend etc)
- a large font (used for news headlines etc)
- a monospace font (used for text files such as NewGRF readmes).

You can use the following types of fonts with OpenTTD:

## OpenTTD's default fonts

These fonts are OpenTTD Sans (small and medium), OpenTTD Serif (large) and OpenTTD Mono (monospace). They are distributed as part of OpenTTD since version 14. The font files are included in the baseset directory of OpenTTD.

These fonts are active by default and support the Latin, Greek and Cyrillic scripts at present.

## Traditional sprite fonts

These are the classic bitmap fonts included as part of the base graphics. They support only the Latin script.

These fonts can be activated in the Graphics section of the Game options window, by enabling the option "Use traditional sprite fonts".

## System fonts

These are fonts installed on your computer. OpenTTD tries to automatically detect and activate a suitable system font in case you have selected a language not supported by the default fonts. However, if this fails, you may have to set a font manually.

There are two ways to manually set system fonts, using the `font` console command or editing the openttd.cfg file.

### Using the console

Open the console. On most keyboards, this is done by pressing the key to the left of 1 (\` on most English keyboards).

The command to change a font is `font [medium|small|large|mono] [<font name>] [<size>]`.
For example, `font large "Times New Roman" 16`.

The font name should be enclosed in double quotes if it contains spaces. Note that the size provided is multiplied by the interface scaling factor.

You can reset the font and size to the defaults by providing the font name "" (a blank font name). This will result in the OpenTTD default font or sprite font (depending on the setting) if you are using a supported language, or a default font determined by your OS otherwise.

You can view the current font configuration by running the command `font` without any arguments.

For more information, run the command `help font`.

### Using openttd.cfg

In openttd.cfg, the following settings under the `[misc]` section determine the font (this is just an example):
```
small_font =
medium_font = Arial Bold
large_font = Times New Roman
mono_font =
small_size = 6
medium_size = 10
large_size = 18
mono_size = 10
```

If these settings are not present, you can add them under the `[misc]` section.

If any font names are left blank, the default font and size are used.

If you cannot find the openttd.cfg file, see [the directory structure guide](./directory_structure.md).
