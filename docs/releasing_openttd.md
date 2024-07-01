# Releasing OpenTTD

This guide is for OpenTTD developers/maintainers, to release a new version of OpenTTD.

## Step 0: Branch or Backport

* If this is a beta version release, skip this step.

* If this is an RC1 (first Release Candidate) build, create a new branch `release/nn` where `nn` is the major version number, then apply changes similar to [PR#9573](https://github.com/OpenTTD/OpenTTD/pull/9573). You also need to forwardport the changelog, as in [PR#10113](https://github.com/OpenTTD/OpenTTD/pull/10113).
  * Update the version in `CMakeLists.txt` in the master branch, heading for the next major release, e.g. from 14.0 to 15.0.
  * Add a new (empty) AI compatibility script in `bin/ai/`
  * Add the new version to CheckAPIVersion in `src/ai/ai_info.cpp` and `src/game/game_info.cpp`
  * Add the new version to `src/script/api/ai_changelog.hpp` and `src/script/api/game_changelog.hpp`
  * Update the version of regression in `bin/ai/regression/regression_info.nut`
  * Add a note to `src/saveload/saveload.h` about which savegame version is used in the branch.

* If this is a later RC or release build and the release branch already exists, you'll need to backport fixes and language from master to this branch, which were merged after the branch diverged from master. You can use these two helper scripts: https://github.com/OpenTTD/scripts/tree/main/backport

* If this is a maintenance release, update the version in `CMakeLists.txt` in the release branch, e.g. from 14.0 to 14.1.

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
3. Write announcement text for the store pages and socials like TT-Forums / Discord / Twitter / Reddit / Fosstodon / etc., and include it in the PR.
4. Create a Steam news image for that post and include it in the PR.
5. Check the website post ("View Deployment" link) and make corrections. We usually just use the GitHub web interface for this and squash the result later.
6. Get this PR approved, but do not merge yet.

## Step 3: Make the actual OpenTTD release

1. Confirm that the version in `CMakeLists.txt` matches the intended release version.
2. Go to https://github.com/OpenTTD/OpenTTD/releases/new and create a new tag matching the release number. For the body of the release, copy in the changelog. "Set as a pre-release" for a beta or RC.
3. Wait for the OpenTTD release workflow to be complete.
4. If this is a full release:
   * for `Steam`: under Steamworks -> SteamPipe -> Builds, set the "testing" branch live on the "default" branch. This will request 2FA validation.
   * for `GOG`: under Builds, "Publish" the freshly uploaded builds to `Master`, `GOG-use only` and `Testing`.
   * for `Microsoft Store`: ask orudge to publish the new release.

Access to `Steam`, `GOG` and/or `Microsoft Store` requires a developer account on that platform.
You will need access to the shared keystore in order to create such an account.
For help and/or access to either or both, please contact TrueBrain.

## Step 4: Tell the world

1. Merge the website PR. This will publish the release post.
2. Make announcements on social media and store pages. You may need to coordinate with other developers who can make posts on TT-Forums, Twitter, Reddit, Fosstodon, Discord, Steam, GOG, Microsoft Store, etc.
