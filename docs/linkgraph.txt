# Some clarifications about the link graph

`InitializeLinkGraphs` joins all threads, so if the game is abandoned
with some threads still running, they're joined as soon as the next game
(possibly the title game) is started. See also `InitializeGame`.

The MCF (multi-commodity flow) algorithm can be quite CPU-hungry as it's
NP-hard and takes exponential time (though with a very small constant
factor) in the number of nodes.

This is why it is run in a separate thread where possible. However after
some time the thread is joined and if it hasn't finished by then the game
will hang. This problem gets worse if we are running on a platform without
threads. However, as those are usually the ones with less CPU power I
assume the contention for the CPU would make the game hard to play even
with threads or even without cargodist (autosave ...). I might be wrong,
but I won't put any work into this before someone shows me some problem.

You can configure the link graph recalculation time. A link graph
recalculation time of X days means that each link graph job has X days
to run before it is joined. The downside is that the flow stats won't be
updated before the job is finished and thus a high value means less
updates and longer times until changes in capacities are accounted for.
If you play a very large map with a complicated link graph you may want to
raise the time setting to avoid lags. The same holds for systems with slow
CPUs.

Another option to avoid excessive lags is to reduce the accuracy of link
graph calculations. Generally the accuracy is inversely correlated to the
CPU requirements of the MCF algorithm.
