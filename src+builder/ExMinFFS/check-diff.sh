#!/bin/bash

opt_copy=no
opt_verbose=no
opt_use_default_destination=yes
unset first_arg
unset second_arg
for arg in $*
do
    if [ "$arg" = "--copy" ]
    then
	opt_copy=yes
    elif [ "$arg" = "-v" ]
    then
	opt_cverbose=yes
    elif [ "$arg" = "-*" ]
    then
	echo "Unknown option [$arg]. Exiting."
	exit 1
    else
	if [ ! -z "$first_arg" ]
	then
	    if [ ! -z "$second_arg" ]
	    then
		echo "Too many args. Exiting."
		exit 1
	    fi
	    second_arg=$arg
	    opt_use_default_destination=no
	else
	    first_arg=$arg
	fi
    fi
done		 

if [ "$opt_use_default_destination" = "yes" ]
then
    default_destination=${PWD%/FreeFileSync/Platforms/MinGW*}
    dst=$default_destination
    src=$first_arg
else
    src=$first_arg
    dst=$second_arg
fi

if [ -z "$first_arg" ]
then
    echo "Need at least one argument. Exiting."
    exit 1
fi


dstdirname=$(/bin/basename $dst)
if [ "$dstdirname" != "src+builder" ]
then
    echo "Destination $dst should be src+builder directory. Existing."
    exit 1
fi

srcdirname=$(/bin/basename $src)
if [ "$srcdirname" != "src+builder" ]
then
    echo "Source $src should be src+builder directory. Existing."
    exit 1
fi

if [ ! -d $src ]
then
    echo "No such directory $src. Existing."
    exit 1
fi

if [ ! -d $dst ]
then
    echo "No such directory $dst. Existing."
    exit 1
fi

echo DEBUG: "$src" "$dst"
if [ "$src" = "$dst" ]
then
    echo "Source directory and destination directory are the same. Existing."
    exit 1
fi

(cd $dst
file_count=0
diff_file_count=0
copy_file_count=0
for i in $(find . \( -name '*.[hc]pp' -o -name '*.[hc]' \) -print | sed -e 's/^..//')
do
    file_count=$((file_count+1))
    if diff -q $src/$i $i >/dev/null 2>&1
    then
	if [ "$opt_cverbose" = "yes" ]
	then
	    echo SAME $i;
	fi
    else 
	diff_file_count=$((diff_file_count+1))
	if [ "$opt_copy" = "yes" ]
	then
	    echo COPY $i
	    cp $src/$i $i
	    copy_file_count=$((copy_file_count+1))
	else
	    echo DIFF $i;
	fi
    fi
done
echo "Total $file_count files checked, $diff_file_count files differ."
)
