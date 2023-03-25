# Releasing OpenTTD

This guide is for OpenTTD developers/maintainers, to release a new version of OpenTTD.

## Step 0: Branch or Backport

* If this is a beta version release, skip this step.

* If this is an RC1 (first Release Candidate) build, create a new branch `release/nn` where `nn` is the major version number, then apply changes similar to [PR#9573](https://github.com/OpenTTD/OpenTTD/pull/9573). You also need to forwardport the changelog, as in [PR#10113](https://github.com/OpenTTD/OpenTTD/pull/10113).
  * Update CMakeLists.txt
  * Add a new (empty) AI compatibility script in bin/ai/
  * Add the new version to CheckAPIVersion in src/ai/ai_info.cpp + src/game/game_info.cpp
  * Add the new version to src/script/api/ai_changelog.hpp + src/script/api/game_changelog.hpp
  * Update the version of regression in bin/ai/regression/regression_info.nut
  * Add a note to src/saveload/saveload.h about which savegame version is used in the branch.

* If this is a later RC or release build and the release branch already exists, you'll need to backport fixes and language from master to this branch, which were merged after the branch diverged from master. You can use these two helper scripts: https://github.com/OpenTTD/scripts/tree/main/backport

## Step 1: Prepare changelog documentation

1. Update the [changelog](../changelog.txt) with new changes since the last release.
   * Changelog entries are typically PR titles, but can be edited to be more helpful without context.
   * Don't include fixes to things which haven't previously been released (like fixes to features which are in the same changelog).
   * Order the entries by importance: `Feature > Add > Change > Fix`, then numerically by PR number.
2. Create a changelog PR, get approval, and merge.
   * For beta releases, target master, otherwise target the release branch.

## Step 2: Prepare website release announcement

1. Go to https://github.com/OpenTTD/website/new/main/_posts and write a new announcement post. See a [previous example](https://github.com/OpenTTD/website/pull/238) for a template.
2. Create a new branch for this post and open a PR for it.
3. Write announcement text for socials like Forum/Discord/Twitter/Reddit and include it in the PR.
4. Create a Steam news image for that post and include it in the PR.
5. Check the website post (preview link via checks page) and make corrections. We usually just use the GitHub web interface for this and squash the result later.
6. Get this PR approved, but do not merge yet.

## Step 3: Make the actual OpenTTD release

1. Go to https://github.com/OpenTTD/OpenTTD/releases/new and create a new tag matching the release number. For the body of the release, see any older release. "Set as a pre-release" for a beta or RC, set as latest for a real release.
2. Merge website PR.
3. Wait for the OpenTTD release checks to be complete.
4. Check that website links to the new release are working and correct, using the [staging website](https://www-staging.openttd.org/).
5. If this is a full release, ask orudge to update the Microsoft Store and TrueBrain to move the release from the "testing" to "default" branch on Steam.

## Step 4: Tell the world

1. Tag and create a website release to trigger the actions that update the website.
2. After the website is live, make announcements on social media. You may need to coordinate with other developers who can make posts on Twitter, Reddit, Steam, and GOG.
