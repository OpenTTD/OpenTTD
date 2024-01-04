# Translations for OpenTTD

Eints is [OpenTTD's WebTranslator](https://translator.openttd.org/).

- Registered translators translate from English to their language.
- Eints validates the translations syntactically and that parameter usage matches the English base language.
- Eints synchronises translations to OpenTTD's repository every day, shortly before the nightly build.

When adding or altering strings in english.txt, you should stick to some rules, so translations are handled smoothly by Eints and translators.
This document gives some guidelines.


## I want to change a translation.

### I want to become a regular translator.

Just [sign up](https://github.com/OpenTTD/team/issues/new/choose) as a translator.

### I only want to point out some issues / typos in the current translation, or suggest a change.

[Open an issue](https://github.com/OpenTTD/OpenTTD/issues/new/choose), so it can be assigned to the translation team of the language.
The translators will decide whether, where and how to apply your suggestion.

### I want to submit translations via PR.

Sorry, we don't offer this option.

Only when there is a consistency problem that needs addressing, this can be done via a PR.
We are very strict about this, and in general all PRs making translation changes will be closed.
But if it is really needed, and the change is not a revert of any older change, a PR can be created to do mass changes to a translation.

### I want to change the language definition (plural form, genders, cases) of a translation.

Please [create an issue](https://github.com/OpenTTD/OpenTTD/issues/new/choose) for this.

### I want to add an entirely new translation language.

OpenTTD has more than 4000 strings, translating all of them is a lot of work.
Despite the initial enthusiasm, only few people have the endurance to get to even 20% translation progress.

As such, starting a new translation requires the prospect that there is also translation interest in the future.
And, frankly, OpenTTD probably already covers all languages to which this applies, and a few more.

If you still want to make the case, that your language is spoken by several 100 million people, please [create an issue](https://github.com/OpenTTD/OpenTTD/issues/new/choose) for adding a new language.


## I want to change the English base language (english.txt).

### I want to change the wording / fix a typo in an English string, without changing the meaning (drastically).

Just change it in your PR.

Translators will be notified that their translation became "outdated", so they can double-check whether the translation needs updating.

### I want to add/change/remove parameters from an English string.

Just change the parameters in english.txt in your PR.
Don't touch the translations, please ignore compile warnings about them.

Translators will be notified that their translation became "invalid", so they can adjust the translation.
Eints will remember the old translations for translators to view, but remove them from the git repository, while they are "invalid"; so there won't be any compile warnings after the nightly sync.

### I want to change the meaning of an English string, so that no existing translation makes any sense anymore.

In this case, please change the STR_xxx string identifier of the string; basically: remove the old string, add a new one.
Don't touch the translations, please ignore compile warnings about them.

Eints will discard all memory of the old strings in the nightly sync, and translators can start fresh with a new string.

### I want to add a new string.

Add the new string somewhere in english.txt, where it fits with the neighbouring strings.
Don't touch the translations, even if you can speak some of the languages.

### I want to remove an unused string.

Remove the string from english.txt.
Don't touch the translations, please ignore compile warnings about them.

Eints will remove the translations from the git repository in the nightly sync.

### I want to reorder strings in english.txt without changing them.

Reorder english.txt as you like. Don't touch the translations.

Eints will reorder all translations to match english.txt in the nightly sync.

### I want to add/change '#' comments.

Change comments in english.txt as you like. Don't touch the translations.

Eints will replicate comments into all translations in the nightly sync. Comments are not translated.

### I want to change the STR_xxx string identifier for code style reasons, without changing the English text.

This is the only case, where your PR should also edit translations.
The STR_xxx string identifier is used by Eints as key value to track strings and translations. If you change it, that's the same as deleting a string and adding an unrelated new one.
So, to keep translations, you have to replace the STR_xxx for all translations in the PR as well.

However, you will only be able to keep the translations which are part of the git repository.
Translation history and information about translations being "outdated" will be lost.
So, keep your code style OCD to a minimum :)


## I want to fight a bot and lose.

Here are some things, people sometimes want to do, but which won't work.

### I want to enforce re-translation by clearing current translations.

You have to change the STR_xxx string identifier, that's the only option.

You cannot "clear" translations by removing them via PR; eints will reinstall the previous "syntactically perfect valid" translation.

### I want to revert a broken change, some translator just did via eints.

You have to revert the translations via the WebTranslator interface.
If there are many changes, ask someone with Admin access to eints, so they can manually upload a fixed translation file to eints.

You cannot revert translations changes via PR. Eints merges translations from git and from web by keeping a translation history, and committing the newest translation to git.
If you revert to an old translation in git, eints will simply think git did not yet get the newer translation, and commit it again.
