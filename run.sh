#!/usr/bin/env bash

if make; then
    mkdir -p run
    cd run
    ../bin/swall $@
    exit $?
fi

exit 1