#!/bin/sh
#
# from: https://github.com/acabal/scripts/blob/master/ogg-cover-art
#

usage(){
        fmt <<EOF
DESCRIPTION
        Add cover art to an OGG file.
        Adapted from mussync-tools by biapy: https://github.com/biapy/howto.biapy.com/blob/master/various/mussync-tools

USAGE
        ogg-cover-art [-h|--help] [-d TEXT|--description=TEXT] [COVER-FILENAME] OGG-FILENAME
                Add cover art to OGG-FILENAME.  If COVER-FILENAME is not specified,
                use cover.jpg, cover.jpeg, or cover.png located in the same folder
                as OGG-FILENAME.
EOF
        exit
}
die(){ printf "Error: %s\n" "${1}" 1>&2; exit 1; }
require(){ command -v "${1}" > /dev/null 2>&1 || { suggestion=""; if [ ! -z "$2" ]; then suggestion=" $2"; fi; die "$1 is not installed.${suggestion}"; } }
# End boilerplate

require "vorbiscomment" "Try: apt-get install vorbis-tools"

if [ $# -eq 0 ]; then
        usage
fi

description=''
file1=''
file2=''
files=0

while [ $# -gt 0 ]; do
        case "$1" in
        -h|--help)
                usage
                ;;
        -d)
                if [ $# -eq 1 ]; then
                        die "Missing description's text"
                fi
                description="$2"
                shift 2
                ;;
        --description=*)
                description="${1#*=}"
                shift 1
                ;;
        *)
                case $files in
                0) file1="$1";;
                1) file2="$1";;
                2) die "Too many files specified";;
                esac
                files=$((files+1))
                shift 1
                ;;
        esac
done

case $files in
0)        ;;
1)        outputFile="$file1";;
2)        imagePath="$file1"
        outputFile="$file2";;
esac

unset file1 file2 files

if [ -z "$outputFile" ]; then
        die "No output file specified"
fi

if [ -z "$imagePath" ]; then
        dirName=$(dirname "$outputFile")
        imagePath="${dirName}/cover.jpg"
        if [ ! -f "${imagePath}" ]; then
                imagePath="${dirName}/cover.jpeg"
                if [ ! -f "${imagePath}" ]; then
                        imagePath="${dirName}/cover.png"
                        if [ ! -f "${imagePath}" ]; then
                                die "Couldn't find a cover image in ${dirName}."
                        fi
                fi
        fi
        unset dirName
fi

if [ ! -f "${outputFile}" ]; then
        die "Couldn't find ogg file."
fi

if [ ! -f "${imagePath}" ]; then
        die "Couldn't find cover image."
fi

imageMimeType=$(file -b --mime-type "${imagePath}")

if [ "${imageMimeType}" != "image/jpeg" ] && [ "${imageMimeType}" != "image/png" ]; then
        die "Cover image isn't a jpg or png image."
fi

oggMimeType=$(file -b --mime-type "${outputFile}")

if [ "${oggMimeType}" != "audio/x-vorbis+ogg" ] && [ "${oggMimeType}" != "audio/ogg" ]; then
        die "Input file isn't an ogg file."
fi

# Export existing comments to file
commentsPath="$(mktemp -t "tmp.XXXXXXXXXX")"
vorbiscomment --list --raw "${outputFile}" > "${commentsPath}"

# Remove existing images
sed -i -e '/^metadata_block_picture/d' "${commentsPath}"

# Insert cover image from file

# metadata_block_picture format
# See: https://xiph.org/flac/format.html# metadata_block_picture
imageWithHeader="$(mktemp -t "tmp.XXXXXXXXXX")"

# Reset cache file
: > "${imageWithHeader}"

# Picture type <32>
printf "0: %.8x" 3 | xxd -r -g0 >> "${imageWithHeader}"

# Mime type length <32>
printf "0: %.8x" $(echo -n "${imageMimeType}" | wc -c) | xxd -r -g0 >> "${imageWithHeader}"

# Mime type (n * 8)
echo -n "${imageMimeType}" >> "${imageWithHeader}"

# Description length <32>
printf "0: %.8x" $(echo -n "${description}" | wc -c) | xxd -r -g0 >> "${imageWithHeader}"

# Description (n * 8)
echo -n "${description}" >> "${imageWithHeader}"

# Picture with <32>
printf "0: %.8x" 0 | xxd -r -g0  >> "${imageWithHeader}"

# Picture height <32>
printf "0: %.8x" 0 | xxd -r -g0 >> "${imageWithHeader}"

# Picture color depth <32>
printf "0: %.8x" 0 | xxd -r -g0 >> "${imageWithHeader}"

# Picture color count <32>
printf "0: %.8x" 0 | xxd -r -g0 >> "${imageWithHeader}"

# Image file size <32>
printf "0: %.8x" $(wc -c "${imagePath}" | cut --delimiter=' ' --fields=1) | xxd -r -g0 >> "${imageWithHeader}"

# Image file
cat "${imagePath}" >> "${imageWithHeader}"

echo "metadata_block_picture=$(base64 --wrap=0 < "${imageWithHeader}")" >> "${commentsPath}"

# Update vorbis file comments
vorbiscomment --write --raw --commentfile "${commentsPath}" "${outputFile}"

# Delete temp files
rm "${imageWithHeader}"
rm "${commentsPath}"
