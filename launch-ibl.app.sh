#!/bin/sh
bindir=$(pwd)
cd /Users/newpolaris/Projects/ibl2/
export 

if test "x$1" = "x--debugger"; then
	shift
	if test "x" = "xYES"; then
		echo "r  " > $bindir/gdbscript
		echo "bt" >> $bindir/gdbscript
		GDB_COMMAND-NOTFOUND -batch -command=$bindir/gdbscript  /Users/newpolaris/Projects/ibl2/ibl.app 
	else
		"/Users/newpolaris/Projects/ibl2/ibl.app"  
	fi
else
	"/Users/newpolaris/Projects/ibl2/ibl.app"  
fi
