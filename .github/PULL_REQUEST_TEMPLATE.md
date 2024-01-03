## Motivation / Problem

<!--
Describe here shortly
* For bug fixes:
    * What problem does this solve?
    * If there is already an issue, link the issue, otherwise describe the problem here.
* For features or gameplay changes:
    * What was the motivation to develop this feature?
    * Does this address any problem with the gameplay or interface?
    * Which group of players do you think would enjoy this feature?
-->


## Description

<!--
Describe here shortly
* For bug fixes:
    * How is the problem solved?
* For features or gameplay changes:
    * What does this feature do?
    * How does it improve/solve the situation described under 'motivation'.
-->


## Limitations

<!--
Describe here
* Is the problem solved in all scenarios?
* Is this feature complete? Are there things that could be added in the future?
* Are there things that are intentionally left out?
* Do you know of a bug or corner case that does not work?
-->


## Checklist for review

Some things are not automated, and forgotten often. This list is a reminder for the reviewers.
* The bug fix is important enough to be backported? (label: 'backport requested')
* This PR touches english.txt or translations? Check the [guidelines](https://github.com/OpenTTD/OpenTTD/blob/master/docs/eints.md)
* This PR affects the save game format? (label 'savegame upgrade')
* This PR affects the GS/AI API? (label 'needs review: Script API')
    * ai_changelog.hpp, game_changelog.hpp need updating.
    * The compatibility wrappers (compat_*.nut) need updating.
* This PR affects the NewGRF API? (label 'needs review: NewGRF')
    * newgrf_debug_data.h may need updating.
    * [PR must be added to API tracker](https://wiki.openttd.org/en/Development/NewGRF/Specification%20Status)
