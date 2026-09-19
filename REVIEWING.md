# Open Code Review

OpenTTD has been developed and maintained by many people over the years.
We can use your help reviewing contributions, to keep that chain of people going into the future.

## Anyone can review

Code reviews have a number of aspects to consider.
1. Does the code work as described?
2. Is the code well-written, efficient, formatted properly, etc.?
3. Does the feature, change, or fix meet the [goals of the project](./CONTRIBUTING.md#project-goals)?

You can help with any of these, even if you don't feel comfortable with all of them.
For example, you might playtest a Pull Request and leave your feedback on its functionality, allowing others to focus on the code.

All Pull Requests in OpenTTD need reviews, including those by maintainers (core developers with merge rights).
Don't be shy about giving us feedback!

## Giving feedback

Reviews are often a chance to correct mistakes or improve upon a contribution before it is merged.

However, please be respectful of the author's time and leave your feedback in a constructive and actionable way.

If you're reviewing code, leaving inline code suggestions is helpful to show the author what you're talking about.
General thoughts can be left in the main text of the review.

Try to avoid suggesting feature-creep.
We try to keep Pull Requests small for ease of review.

Correcting coding style, spelling, grammar, and naming is good, but lengthy debates about minor details are often unhelpful and quickly take the fun out of contributing.

OpenTTD is a complex bit of software with a fair amount of old code, so it's not always straightforward to read.
If you don't understand why a contributor has done things a certain way, ask!
It could be a mistake on the contributor's part, a learning opportunity for you, or a bit of strange code that needs a comment to make it clear to others in the future.

## Approving a Pull Request

If you are happy with a Pull Request, feel free to leave an Approved review with your thoughts!

For example, you might Approve and leave a comment about what you feel confident reviewing, and anything you're unsure about:
> Works as described. Code looks good to me, although I'm not too familiar with the saveload conversion so someone else should have a look.
> I like how this feature improves upon the existing systems and think it makes sense gameplay-wise.

In order to be merged, a Pull Request must be approved and merged by a maintainer.
(Nobody can approve their own pull request.)

If a non-maintainer approves a Pull Request, it still needs to wait for a maintainer to give the final approval and merge it.
This is not guaranteed, as the maintainer may have additional feedback or disagree with the Pull Request's suitability for inclusion in the project.
However, reviews and approvals from non-maintainers are valuable information to the reviewer showing that others have tested the Pull Request, looked over its code, and like its contents.

## Triage Team

There are a number of people involved in OpenTTD who don't have merge rights, but can manage Pull Requests and Issues to help keep the project organized.
For example, they can assign labels, close resolved issues, and communicate with bug reporters and contributors to help move the project forward.

This role requires decent communication skills, a basic understanding of our development process, and a few merged Pull Requests.
If you are interested in helping out in this way, please ask a maintainer on Discord or IRC.

## Attribution

We would like to thank Alice I. Cecile for [writing about Open Code Review](https://shaping.systems/blog/open-code-review/) and inspiring us to try expanding our process.
This guide was heavily inspired by the [Bevy contributor's guide](https://bevy.org/learn/contribute/helping-out/how-you-can-help/), under the MIT License, thanks also to everyone involved in that project.
