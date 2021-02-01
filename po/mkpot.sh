#!/bin/sh

xgettext --language=C --from-code=UTF-8 --keyword=_ --keyword=N_ --keyword=C_:1c,2 --keyword=NC_:1c,2 -s --package-name=Gromit-MPX ../src/*.c -o gromit-mpx.pot
