#!/usr/bin/env bash

set -e

cd $(dirname $0)/..

if [ $# -ne 1 ] ; then
	echo "Usage: update-rpm version"
	exit
fi

version=$1
tarball=springlobby-${version}.tar.bz2

# first time stuff:
# cd rpm
# osc co home:accAgon SpringLobby

cd rpm/home:accAgon/SpringLobby
osc up
osc rm springlobby-0.0.1.*.tar.bz2
sed -i 's/^\(%define app_version\) .*/\1 '${version}'/' springlobby_tarball.spec
cp /srv/www/springlobby/tarballs/${tarball} .
osc add ${tarball}
osc ci -m "autobuild ${version}"
