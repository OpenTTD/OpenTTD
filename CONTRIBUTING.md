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
the preferred channel for [bug reports](#bug-reports), [features requests](#feature-requests)
and [submitting pull requests](#pull-requests), but please respect the following
restrictions:

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

3. **Isolate the problem** &mdash; ideally create a [reduced test
case](https://css-tricks.com/reduced-test-cases/) (< THIS IS NICE BUT NOT ABOUT OPENTTD EH) in a savegame with screenshots.
Try to use few or no NewGRFs, AIs etc if possible.

A good bug report shouldn't leave others needing to chase you up for more
information. Please try to be as detailed as possible in your report. What is
your environment? What steps will reproduce the issue? What browser(s) and OS
experience the problem? Do other browsers show the bug differently? What
would you expect to be the outcome? All these details will help people to fix
any potential bugs.

Example:

> Short and descriptive example bug report title
>
> A summary of the issue and the browser/OS environment in which it occurs. If
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


FEATURE REQUESTS ARE MOST LIKELY TO BE CLOSED, I WILL WRITE HOW AND WHY.

We also use a new policy in regards to closing issues; keeping issues around with "a good idea" or "not really a bug but we should maybe fix it" turns out to have the reversed effect: nobody looks at it anymore. Although we really appreciate feedback and ideas, we will close enhancement requests that we don't expect to fulfill in the next year. Also bugs that are not really bugs but feature requests, are much more likely to be closed with the message: "we are going to pass on this one; thank you though". Many of those ideas etc do have a place on the forums; and if enough people like it, someone will stand up and make it :) We just don't want the Issue Tracker to become a huge sinkhole of endless ideas nobody is going to do anything with.

¿¿ LIST OF KNOWN FEATURES THAT IDEALLY WOULD BE BETTER (RV OVERTAKING ETC) ??

Before opening a feature request, please take a moment to find out whether your idea
fits with the scope and aims of the project. It's up to *you* to make a strong
case to convince the project's developers of the merits of this feature. Please
provide as much detail and context as possible.

IRC IS BEST EH?


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
git pull upstream
```

3. Create a new topic branch (off the main project development branch) to
contain your feature, change, or fix:

```bash
git checkout -b <topic-branch-name>
```

4. Commit your changes in logical chunks. Please adhere to these [git commit
message guidelines](http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html)
or your code is unlikely to be merged into the main project. Use Git's
[interactive rebase](https://help.github.com/articles/interactive-rebase)
feature to tidy up your commits before making them public.

5. Locally merge (or rebase) the upstream development branch into your topic branch:

```bash
git pull [--rebase] upstream v4-dev
```

6. Push your topic branch up to your fork:

```bash
git push origin <topic-branch-name>
```

7. [Open a Pull Request](https://help.github.com/articles/using-pull-requests/)
with a clear title and description against the `v4-dev` branch.

REWRITE FOR GPL

**IMPORTANT**: By submitting a patch, you agree to allow the project owners to
license your work under the terms of the [MIT License](LICENSE) (if it
includes code changes) and under the terms of the
[Creative Commons Attribution 3.0 Unported License](docs/LICENSE)
(if it includes documentation changes).

### Pull request bots

DO WE HAVE BOTS?

[@twbs-rorschach](https://github.com/twbs-rorschach) is a OpenTTD bot that hangs out in our GitHub issue tracker and automatically checks all pull requests for a few simple common mistakes. It's possible that Rorschach might leave a comment on your pull request and then close it. If that happens, simply fix the problem(s) mentioned in the comment (there should be link(s) in the comment explaining the problem(s) in detail) and then either:

* Push the revised version to your pull request's branch and post a comment on the pull request saying that you've fixed the problem(s). One of the OpenTTD Core Team members will then come along and reopen your pull request.
* Or you can just open a new pull request for your revised version.

[@twbs-savage](https://github.com/twbs-savage) is a OpenTTD bot that automatically runs cross-browser tests (via [Sauce](https://saucelabs.com) and Travis CI) on JavaScript pull requests. Savage will leave a comment on pull requests stating whether cross-browser JS tests passed or failed, with a link to the full Travis build details. If your pull request fails, check the Travis log to see which browser + OS combinations failed. Each browser test in the Travis log includes a link to a Sauce page with details about the test. On those details pages, you can watch a screencast of the test run to see exactly which unit tests failed.


## Code guidelines

LINK https://wiki.openttd.org/Coding_style

## License

REWRITE TO GPL

By contributing your code, you agree to license your contribution under the [MIT License](LICENSE).
By contributing to the documentation, you agree to license your contribution under the [Creative Commons Attribution 3.0 Unported License](docs/LICENSE).

