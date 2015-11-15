#!/bin/bash

PATH=/cygdrive/c/"Program Files (x86)"/"HTML Help Workshop":$PATH
PATH=/cygdrive/c/"Program Files (x86)"/Graphviz2.38/bin:$PATH
PATH=/cygdrive/c/"Program Files"/doxygen/bin:$PATH
PATH=/cygdrive/c/MinGW-w64/mingw32/bin:$PATH:/cygdrive/c/Projects/Development/inhouse-tools

if [ -f /etc/bash_completion.d/git ]
then	. /etc/bash_completion.d/git
fi

if [ -f /etc/bash_completion.d/svn ]
then	. /etc/bash_completion.d/svn
fi
