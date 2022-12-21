#!/bin/bash
#
# Copyright 2021-2023 Brad Lanam Pleasant Hill CA
#
# requirements: sshpass
#

echo "* Remember to install on both sourceforge and ballroomdj.org"

TMP=tmpweb
TMPIMG=tmpweb/img

test -d $TMP && rm -rf $TMP
mkdir -p $TMPIMG

tserver=web.sourceforge.net
echo -n "Server [$tserver]: "
read server
if [[ $server == "" ]]; then
  server=$tserver
fi

tremuser=bll123
echo -n "User [$tremuser]: "
read remuser
if [[ $remuser == "" ]]; then
  remuser=$tremuser
fi

tport=22
echo -n "Port [$tport]: "
read port
if [[ $port == "" ]]; then
  port=$tport
fi

ttestweb=N
testpath=""
echo -n "Test [$ttestweb]: "
read testweb
if [[ $testweb == "" ]]; then
  testweb=$ttestweb
fi
if [[ $testweb == Y ]]; then
  testpath="/test"
fi

case $server in
  web.sourceforge.net)
    port=22
    project=ballroomdj4
    # ${remuser}@web.sourceforge.net:/home/project-web/${project}/htdocs
    wwwpath=/home/project-web/${project}/htdocs
    ;;
  ballroomdj.org|gentoo.com)
    wwwpath=/var/www/ballroomdj.org
    ;;
esac
ssh="ssh -p $port"
export ssh

echo -n "Remote Password: "
read -s SSHPASS
echo ""
export SSHPASS

echo "## copying files"
if [[ $server == ballroomdj.org ]]; then
  cp -pf bdj4register.php bdj4report.php bdj4support.php $TMP
fi

mkdir -p $TMP${testpath}
cp -pf bdj4.css $TMP${testpath}
cp -pf bdj4.html $TMP${testpath}/index.html
cp -pf ../img/ballroomdj4-base.svg $TMPIMG/ballroomdj4.svg
cp -pf ../img/menu-base.svg $TMPIMG/menu.svg
cp -pf ../img/bdj4_icon.png $TMPIMG/bdj4_icon.png

cd $TMP
sshpass -e rsync -v -e "$ssh" -aS \
    . \
    ${remuser}@${server}:${wwwpath}
cd ..

test -d $TMP && rm -rf $TMP

exit 0
