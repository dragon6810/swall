# Swall

Swall is a (partially) [UCI][uci-link]-compliant chess engine written in C.

## Building

You will need `clang` and `make` installed in order to build. If you'd like to build with a different compiler, set the `CC` variable in `Makefile` to your compiler of choice. 

If you want to also fetch the resources required for the bot to be better (e.g. the opening book), `fetchresource.sh` will download them. Fetching resources and building has been consolidated in the `cleanbuild.sh`.

First build:

    ./cleanbuild.sh

Subsequent builds:

    make

This will place the executable in `bin/swall`.

## Using

To use the resources downloaded in `fetchresource.sh`, you must run swall with `run/` as your cwd.

You can run swall directly from the command line, though I'd recomment using [Cute Chess][cutechess-link]. 

[uci-link]: [https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf]
[cutechess-link]: https://github.com/cutechess/cutechess

## Challenger

Challenger is a python script for benchmarking different versions of swall. Initially, you need to fetch the positions dataset. This can be done by running:

    ./posgen.sh

Then, to run challenger, enter:

    python challenger.py <commit-a> <commit-b>

With commit-a and commit-b being two different commits of the engine to compare. This will run 100 games and tell you the number of wins, draws, and losses for each.