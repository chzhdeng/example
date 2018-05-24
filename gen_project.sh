#!/bin/sh

cur_path=`pwd`
root_dir="$cur_path/.."
cd $root_dir

if [ ! -d "./example" ]
then
    git clone git@github.com:chzhdeng/example.git
fi


