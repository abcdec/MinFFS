#!/bin/bash

PATH=/cygdrive/c/MinGW-w64/mingw32/bin:$PATH:/cygdrive/c/Projects/Development/inhouse-tools

if [ -f /etc/bash_completion.d/git ]
then	. /etc/bash_completion.d/git
fi

if [ -f /etc/bash_completion.d/svn ]
then	. /etc/bash_completion.d/svn
fi
