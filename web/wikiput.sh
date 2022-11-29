#!/bin/bash

while test ! \( -d src -a -d web -a -d wiki \); do
  cd ..
done
cwd=$(pwd)

sfuser=bll123
project=ballroomdj4
baseurl=https://sourceforge.net/rest/p/${project}/wiki
accessurl=${baseurl}/has_access
useragent=bdj4-wikiput.sh
cookiejar=tmp/cookiejar.txt
bearer=""
tmpfile=tmp/wiki-tmp.txt
compfile=tmp/wiki-comp.txt
filelist=tmp/wiki-files.txt
dt=$(date '+%Y-%m-%d %H:%M:%S')
forceflag=F

. ./VERSION.txt
rlstag=""
case $RELEASELEVEL in
  alpha|beta)
    rlstag=-$RELEASELEVEL
    ;;
  production)
    rlstag=""
    ;;
esac
datetag=""
if [[ $rlstag != "" ]]; then
  datetag=-$BUILDDATE
fi
versstr=${VERSION}${datetag}${rlstag}

function updateimages {
  test -d wikiimg && rm -rf wikiimg
  test -d wikiimg || mkdir wikiimg
  # note trailing slash
  rsync -aS --delete -q wiki-i/ wikiimg

  if [[ ! -d wikiimg ]]; then
    echo "bad copy"
    exit 1
  fi

  imglist=$(find wiki-i -print)
  for ifile in $imglist; do
    ifn=$(echo $ifile | sed 's,wiki-i,wikiimg,')
    if [[ -d $ifile ]]; then
      continue
    fi
    case $ifile in
      *~|*.orig)
        rm -f $ifn
        continue
        ;;
    esac
    data=$(identify $ifile)
    set $data
    sz=$(echo $3 | sed 's,x.*,,')
    if [[ $sz -gt 800 ]]; then
      pct=$(dc -e "4k 800 $sz / 100 * p")
      convert -resize ${pct}% $ifile $ifn
    fi
    touch -r $ifile $ifn
  done
  echo "Enter sf password"
  rsync -v -e ssh -aS --delete \
      wikiimg \
      ${sfuser}@web.sourceforge.net:/home/project-web/${project}/htdocs
  rc=$?
  test -d wikiimg && rm -rf wikiimg
}

function getupddate {
  tfn=$1
  grep 'Updated [0-9]*' $tfn |
  sed \
    -e 's/.*Updated \([0-9 :-]*\); BDJ4 version.*/\1/'
}

function gettitle {
  tfn=$1
  echo $tfn |
      sed \
      -e 's,^wiki/,,' \
      -e 's,/,-,g' \
      -e 's,\.txt$,,'
}

function gettext {
  tfn=$1

  # sourceforge's markdown has automatic line breaks.
  # these make editing the wiki files very difficult due to the very
  # long lines for each paragraph.
  # this regsub allows more intuitive paragraph handling
  # do not combine lines when:
  #   next line begins with a dash (probable table)
  #   next line begins with a ` (table)
  #   previous line ends with a ` (table)
  #   next line begins with a space (code)
  #   next line begins with a ~ (code)
  #   previous line begins with a ~ (code)
  #   previous line begins with a # (header)
  #   next line begins with a * (list)
  #   next line begins with a ^ (table marker) (will be removed)
  # remove leading ^ (table marker)
  # ? put a <br> before image captions, as the prior line will break them.
  # remove all trailing whitespace
  echo -n 'text=' > ${tmpfile}
  awk -e '
BEGIN {
  appendok = 0;
  text = "";
}
{
  sub (/\r$/, "");
  if ($0 ~ /Updated [0-9 :-]*; BDJ4 version /) {
    # do nothing
    next;
  }
# not known at this time if this is needed
#  if ($0 ~ /<span/) {
#    sub (/<span/, "<br><span");
#  }
  if ($0 ~ /^[^[` *\^~#-]/ && appendok) {
    sub (/\n$/, "", text);
    text = text " ";
  } else {
    appendok = 0;
  }
  if ($0 ~ /[^`~]$/ && $0 !~ /^#/) {
    appendok = 1;
  }
  text = text $0;
  text = text "\n";
}
END {
  print text;
}
    ' \
    ${tfn} >> ${tmpfile}
  echo "" >> ${tmpfile}
  echo "_(Updated ${dt}; BDJ4 version ${versstr})_" >> ${tmpfile}
  sed \
    -e '/Updated [0-9 :-]*; BDJ4 version /d' \
    ${tfn} > ${tfn}.n
  echo "_(Updated ${dt}; BDJ4 version ${versstr})_" >> ${tfn}.n
  mv -f ${tfn}.n ${tfn}
  touch --date "${dt}" ${tfn}
}

function getaccesstoken {
  bearer=$(cat dev/wikibearer.txt)
}

function hasaccess {
  user=$1
  cmd="curl -L --silent -b ${cookiejar} -c ${cookiejar} -X GET \
        -H 'Authorization: Bearer $bearer' \
        -H 'User-Agent: $useragent' \
        --get \
        --data-urlencode 'user=${user}' \
        --data-urlencode 'perm=create' \
        '${accessurl}'"
  data=$(eval $cmd)
  access=F
  case $data in
    *\"result\":\ true*)
      access=T
      ;;
  esac
  echo $access
}

function getlist {
  cmd="curl -L --silent -b ${cookiejar} -c ${cookiejar} -X GET \
        -H 'Authorization: Bearer $bearer' \
        -H 'User-Agent: $useragent' \
        --output ${tmpfile} \
        '${baseurl}'"
  eval $cmd
  sed \
      -e 's,.*\[",,' -e 's,"\].*,,' \
      -e "s/\", \"/\n/g" \
      ${tmpfile} > ${filelist}
  echo "" >> ${filelist}
}

function get {
  tfn="$1"
  title=$(gettitle $tfn)
  cmd="curl -L --silent -b ${cookiejar} -c ${cookiejar} -X GET \
        -H 'Authorization: Bearer $bearer' \
        -H 'User-Agent: $useragent' \
        --output ${tmpfile} \
        '${baseurl}/${title}'"
  eval $cmd
  sed -e 's,.*"text": ",,' \
      -e 's,".*,,' \
      -e "s,\\\\r,,g" \
      -e "s,\\\\n,\n,g" \
      ${tmpfile} > $tfn
  odt=$(getupddate $tfn)
  touch --date "$odt" ${tfn}
}

function put {
  tfn="$1"
  case ${tfn} in
    *~|*.orig)
      return
      ;;
  esac
  if [[ $forceflag == F ]]; then
    odt=$(getupddate $tfn)
    touch --date "$odt" ${compfile}
    if [[ ! ${tfn} -nt ${compfile} ]]; then
      echo "$tfn: no change"
      return
    fi
  fi
  gettext $tfn
  title=$(gettitle $tfn)
  cmd="curl -L --silent -b ${cookiejar} -c ${cookiejar} -X POST \
      -H 'Authorization: Bearer $bearer' \
      -H 'User-Agent: $useragent' \
      --data-urlencode 'pagetitle=$title' \
      --data-binary @${tmpfile} \
      '${baseurl}/${title}'"
  eval $cmd > /dev/null
  echo "$tfn: updated"
}

function delete {
  # delete does not work on sourceforge
  tfn="$1"
  echo "action: delete: ${baseurl}/${title}"
  if [[ -f $tfn ]]; then
    hg rm $tfn
  fi
}

function getall {
  getlist
  for title in $(cat ${filelist}); do
    get "wiki/${title}"
  done
}

function putall {
  flist=$(find wiki -name '*.txt' -print)
  for f in $flist; do
    put $f
  done
}

function dispfilelist {
  getlist
  cat ${filelist}
}

getaccesstoken
case $1 in
  put)
    shift
    case $1 in
      --force)
        forceflag=T
        shift
        ;;
    esac
    for f in "$@"; do
      if [[ -f "wiki/$f" ]]; then
        put "wiki/$f"
      fi
    done
    ;;
  putall)
    shift
    case $1 in
      --force)
        forceflag=T
        shift
        ;;
    esac
    putall
    ;;
  getall)
    ;;
  dispfilelist)
    dispfilelist
    ;;
  get)
    shift
    for f in "$@"; do
      if [[ -f "wiki/$f" ]]; then
        get "wiki/$f"
      fi
    done
    ;;
  putimages)
    updateimages
    ;;
  *)
    echo "Usage: $0 {putall|getall|dispfilelist|get <file>|put <file>|putimages}"
    ;;
esac

rm -f ${tmpfile} ${compfile} ${filelist}
exit 0
