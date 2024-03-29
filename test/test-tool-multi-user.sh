#!/bin/sh
#
# Quick and dirty tool tester.
#
# Things to improve:
#  - get rid of user cfg modification
#  - get rid of sleep's
#  - move more/all functionality to native code


GROMIT_MPX="$1"
TOOL="$2"

[ -z "$GROMIT_MPX" ] || [ -z "$TOOL" ] && {
    echo "./test-tool-multi-user.sh <path to gromit-mpx> <tool>"
    exit 1
}

mv ~/.config/gromit-mpx.cfg ~/.config/gromit-mpx.cfg.bak
echo \
'
"test-tool" = '"$TOOL"' (size=5 color="green");
"default" = "test-tool";
' > ~/.config/gromit-mpx.cfg

echo "\033[32mbuilding helper\033[0m"
cc -o send_input send_input.c -lX11 -lXtst

echo "\033[32mlaunching gromit-mpx\033[0m"
$GROMIT_MPX &
PID=$!

sleep 1

echo "\033[32mcreating test devices\033[0m"
xinput create-master one 2>/dev/null
xinput create-master two 2>/dev/null

ID_ONE=$(xinput list --id-only "one pointer" 2>/dev/null)
ID_TWO=$(xinput list --id-only "two pointer" 2>/dev/null)

XTEST_ID_ONE=$(xinput list --id-only "one XTEST pointer" 2>/dev/null)
XTEST_ID_TWO=$(xinput list --id-only "two XTEST pointer" 2>/dev/null)

echo "\033[32mtesting, you should see something in the upper left of the screen; you can also draw manually\033[0m"
$GROMIT_MPX -t
sleep 1
./send_input $XTEST_ID_ONE 100 &
./send_input $XTEST_ID_TWO 300 &
sleep 3
$GROMIT_MPX -t

sleep 1

echo "\033[32mcleaning up\033[0m"
xinput remove-master $ID_ONE 2>/dev/null
xinput remove-master $ID_TWO 2>/dev/null

kill $PID

mv ~/.config/gromit-mpx.cfg.bak ~/.config/gromit-mpx.cfg

echo "\033[32mdone\033[0m"
