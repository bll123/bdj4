#!/bin/bash
#
# Copyright 2021-2023 Brad Lanam Pleasant Hill CA
#
ver=1

if [[ $1 == --version ]]; then
  echo ${ver}
  exit 0
fi

getresponse () {
  echo -n "[Y/n]: " > /dev/tty
  read answer
  case $answer in
    Y|y|yes|Yes|YES|"")
      answer=Y
      ;;
    *)
      answer=N
      ;;
  esac
  echo $answer
}

if [[ $(uname -s) != Darwin ]]; then
  echo "Not running on MacOS"
  exit 1
fi

echo ""
echo "This script uses the 'sudo' command to run various commands"
echo "in a privileged state.  You will be required to enter your"
echo "password."
echo ""
echo "For security reasons, this script should be reviewed to"
echo "determine that your password is not mis-used and no malware"
echo "is installed."
echo ""
echo "Continue? "
gr=$(getresponse)
if [[ $gr != Y ]]; then
  exit 1
fi

echo "Uninstall the BallroomDJ 4 Application? "
gr=$(getresponse)
if [[ $gr == Y ]]; then
  dir="$HOME/Applications/BDJ4.app"
  test -d "$dir" && rm -rf "$dir"
  dir="$HOME/.config/BDJ4"
  test -d "$dir" && rm -rf "$dir"
  # dir="$HOME/.config/gtk-3.0"
  # test -d "$dir" && rm -rf "$dir"
  echo "## BDJ4 application removed."
fi

echo "Uninstall BallroomDJ 4 Data? "
gr=$(getresponse)
if [[ $gr == Y ]]; then
  dir="$HOME/Library/Application Support/BDJ4"
  test -d "$dir" && rm -rf "$dir"
  echo "## BDJ4 data removed."
fi

echo "Uninstall MacPorts? "
gr=$(getresponse)
if [[ $gr == Y ]]; then
  TMP=tmp-umacports.txt

  echo "## getting ports list"
  sudo port list installed > $TMP
  echo "## uninstalling all ports"
  sudo port -N uninstall $(cat $TMP)
  echo "## cleaning all ports"
  sudo port -N clean --all $(cat $TMP)
  rm -f ${TMP} > /dev/null 2>&1
  for d in \
      /opt/local \
      /Applications/DarwinPorts \
      /Applications/MacPorts \
      /Library/LaunchDaemons/org.macports.* \
      /Library/Receipts/DarwinPorts*.pkg \
      /Library/Receipts/MacPorts*.pkg \
      /Library/StartupItems/DarwinPortsStartup \
      /Library/Tcl/darwinports1.0 \
      /Library/Tcl/macports1.0 \
      ~/.macports \
      ; do
    echo "## removing ${d}"
    sudo rm -rf ${d}
  done
fi