# Contributing to OpenTTd

Looking to contribute something to OpenTTD? **Here's how you can help.**

Please take a moment to review this document in order to make the contribution
process easy and effective for everyone involved.

Following these guidelines helps to communicate that you respect the time of
the developers managing and developing this open source project. In return,
they should reciprocate that respect in addressing your issue or assessing
patches and features.


## Using the issue tracker

The [issue tracker](https://github.com/OpenTTD/OpenTTD/issues) is
the preferred channel for [bug reports](#bug-reports), but please respect the following restrictions:

* Please **do not** use the issue tracker for personal support requests. FORUMS??

* Please **do not** derail or troll issues. Keep the discussion on topic and
respect the opinions of others.

* Please **do not** post comments consisting solely of "+1" or ":thumbsup:".
Use [GitHub's "reactions" feature](https://github.com/blog/2119-add-reactions-to-pull-requests-issues-and-comments)
instead. We reserve the right to delete comments which violate this rule.

* Please **do not** open issues or pull requests regarding add-on content in NewGRF, GameScripts, AIs, etc

## Bug reports

A bug is a _demonstrable problem_ that is caused by the code in the repository.
Good bug reports are extremely helpful, so thanks!

Guidelines for bug reports:

0. Don't report issues with games where you changed NewGRFs.  Don't report issues with Patchpacks.

1. **Use the GitHub issue search** &mdash; check if the issue has already been
reported.

2. **Check if the issue has been fixed** &mdash; try to reproduce it using the
latest `nightly` build of OpenTTD, available from https://www.openttd.org/en/

3. **Isolate the problem** &mdash; ideally create reproduceable steps with an attached savegame and screenshots.
Try to use few or no NewGRFs, AIs etc if possible.

A good bug report shouldn't leave others needing to chase you up for more
information. Please try to be as detailed as possible in your report. What is
your environment? What steps will reproduce the issue? What OS
experience the problem? What
would you expect to be the outcome? All these details will help people to fix
any potential bugs.

Example:

> Short and descriptive example bug report title
>
> A summary of the issue and the OS environment in which it occurs. If
> suitable, include the steps required to reproduce the bug.
>
> 1. This is the first step
> 2. This is the second step
> 3. Further steps, etc.
>
> SAVEGAME
> SCREENSHOTS
>
> Any other information you want to share that is relevant to the issue being
> reported. This might include the lines of code that you have identified as
> causing the bug, and potential solutions (and your opinions on their
> merits).

## Feature requests

Before opening a feature request, please take a moment to find out whether your idea fits with the scope and aims of the project.
It's up to *you* to make a strong case to convince the project's developers of the merits of this feature.
Please provide as much detail and context as possible.
This means don't request for a solution, but describe the problem you see and how/why you think it should be fixed.

FEATURE REQUESTS ARE MOST LIKELY TO BE CLOSED, I WILL WRITE HOW AND WHY.

LEAVE SOME TEXT ABOUT SMALL TRIVIAL STUFF OR SOMETHING; JUST NOTHING HUGE AND BIG

For feature request we have a strict policy.
Keeping issues around with "a good idea" or "not really a bug but we should maybe fix it" turns out to have the reversed effect: nobody looks at it anymore.
Although we really appreciate feedback and ideas, we will close feature requests that we don't expect to fulfill in the next year.
Many of those ideas etc do have a place on the [forums](https://www.tt-forums.net); and if enough people like it, someone will stand up and make it.

¿¿ LIST OF KNOWN FEATURES THAT IDEALLY WOULD BE BETTER (RV OVERTAKING ETC) ??

IRC IS BEST EH?
YES AND NO. FEATURE REQUESTS WITH CLEAR USECASES ARE SUPER WELCOME


## Pull requests

Good pull requests—patches, improvements, new features—are a fantastic
help. They should remain focused in scope and avoid containing unrelated
commits.

**Please ask first** before embarking on any significant pull request (e.g.
implementing features, refactoring code, porting to a different language),
otherwise you risk spending a lot of time working on something that the
project's developers might not want to merge into the project.

Please adhere to the [coding guidelines](#code-guidelines) used throughout the
project (indentation, accurate comments, etc.) and any other requirements
(such as test coverage).

PROCESS NEEDS CHECKED

Adhering to the following process is the best way to get your work
included in the project:

1. [Fork](https://help.github.com/fork-a-repo/) the project, clone your fork,
and configure the remotes:

```bash
git clone https://github.com/<your-username>/OpenTTD.git openttd
git clone https://github.com/OpenTTD/OpenTTD-git-hooks.git openttd_hooks
cd openttd
git remote add upstream https://github.com/OpenTTD/OpenTTD.git
cd .git/hooks
ln -s -t . ../../../openttd_hooks/hooks/*
```

2. If you cloned a while ago, get the latest changes from upstream:

```bash
git fetch upstream
```

3. Create a new topic branch (off the main project development branch) to
contain your feature, change, or fix:

```bash
git checkout upstream/master -b <topic-branch-name>
```

4. Commit your changes in logical chunks. Please adhere to these [git commit
message guidelines](http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html)
or your code is unlikely to be merged into the main project. Use Git's
[interactive rebase](https://help.github.com/articles/interactive-rebase)
feature to tidy up your commits before making them public.

5. Locally merge (or rebase) the upstream development branch into your topic branch:

```bash
git fetch upstream
git rebase upstream/master
```

6. Push your topic branch up to your fork the first time:

```bash
git push --set-upstream origin <topic-branch-name>
```

And any time after that:

```bash
git push
```

7. [Open a Pull Request](https://help.github.com/articles/using-pull-requests/)
with a clear title and description against the `master` branch.

REWRITE FOR GPL

**IMPORTANT**: By submitting a patch, you agree to allow the project owners to
license your work under the terms of the [MIT License](LICENSE) (if it
includes code changes) and under the terms of the
[Creative Commons Attribution 3.0 Unported License](docs/LICENSE)
(if it includes documentation changes).

### Pull request validation

WRITE ABOUT CI HERE

## Code guidelines

LINK https://wiki.openttd.org/Coding_style

## License

REWRITE TO GPL

By contributing your code, you agree to license your contribution under the [MIT License](LICENSE).
By contributing to the documentation, you agree to license your contribution under the [Creative Commons Attribution 3.0 Unported License](docs/LICENSE).

TELL THAT YOU TOOK MOST OF THIS FROM BOOTSTRAP. CREDITS WHERE CREDITS IS DUE
