#!/bin/bash

# ./especial_char.sh 123 456 789

#$! : Shell最后运行的后台Process的PID
printf "\$$ The complete list is %s\n" "$$"

# '$?' : 最后运行的命令的结束代码（返回值）
printf "\$! vThe complete list is %s\n" "$!"

# '$-' : 使用Set命令设定的Flag一览
printf "\$? The complete list is %s\n" "$?"

# '$*' : 所有参数列表。如"$*"用「"」括起来的情况、以"$1 $2 … $n"的形式输出所有参数。
printf "\$\* The complete list is %s\n" "$*"

# '$@' : 所有参数列表。如"$@"用「"」括起来的情况、以"$1" "$2" … "$n" 的形式输出所有参数。
printf "\$@ The complete list is %s\n" "$@"

# '$#' : 添加到Shell的参数个数
printf "\$# The complete list is %s\n" "$#"

# '$0' :  Shell本身的文件名
printf "\$0 The complete list is %s\n" "$0"

# '$1～$n' : 添加到Shell的各参数值。$1是第1参数、$2是第2参数…。
printf "\$1 The complete list is %s\n" "$1"
printf "\$2 The complete list is %s\n" "$2"











