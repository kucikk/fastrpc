#/bin/bash

cd `dirname \`readlink -f $0\``

ALL_DEPS=`cat debian/control | grep Build-Depends | cut -d: -f1 --complement | sed -e 's/([^\)]*)//g' -e 's/,//g' | xargs`
MIS_DEPS=`dpkg-checkbuilddeps 2>&1 | sed -e 's/^.*Unmet build dependencies: //' -e 's/([^)]*)//g' | xargs`

echo "Checking build dependencies: $ALL_DEPS"
if [ -n "$MIS_DEPS" ]; then
	echo "Installing missing dependencies: $MIS_DEPS"
	apt-get install $MIS_DEPS
fi

