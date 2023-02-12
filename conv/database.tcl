#!/usr/bin/tclsh

if { $argc != 2 } {
  puts "usage: $argv0 <bdj3dir> <datatopdir>"
  exit 1
}

set bdj3dir [lindex $argv 0]
if { ! [file exists $bdj3dir] || ! [file isdirectory $bdj3dir] } {
  puts "Invalid directory $bdj3dir"
  exit 1
}
if { ! [regexp {/data$} $bdj3dir] } {
  append bdj3dir /data
}
if { ! [file exists $bdj3dir] || ! [file isdirectory $bdj3dir] } {
  puts "Invalid directory $bdj3dir"
  exit 1
}
set datatopdir [lindex $argv 1]
if { ! [file exists $datatopdir/data] || ! [file isdirectory $datatopdir/data] } {
  puts "Invalid directory $datatopdir"
  exit 1
}

# get the default volume from the configuration file so that
# volume adjustments can be done for older database files.
set conffn "[file join $bdj3dir profiles bdj_config.txt]"
set defvol 50
if { [file exists $conffn] } {
  set fh [open $conffn r]
  while { [gets $fh line] >= 0 } {
    if { [regexp {^DEFAULTVOLUME:([0-9]*)} $line all dvol] } {
      set defvol $dvol
      break
    }
  }
  close $fh
}

set hsize 128
set rsize 2048

set infn [file join $bdj3dir musicdb.txt]
if { ! [file exists $infn] } {
  # old version 7/8 name
  set infn [file join $bdj3dir masterlist.txt]
  if { ! [file exists $infn] } {
    puts "   no database"
    exit 1
  }
}

set dbfn "$infn"
set fh [open $dbfn r]
gets $fh line
if { [regexp {^# ?VERSION=(\d+)\s*$} $line all vers] } {
  if { $vers < 7 } {
    puts "Database version is too old to convert."
    exit 1
  }
} else {
  puts "Unable to locate database version"
  exit 1
}
close $fh

set fh [open "$infn" r]
gets $fh line
gets $fh line
gets $fh line
gets $fh line
regexp {^#RAMAX=(\d+)$} $line all racount
close $fh

source $dbfn
if { [info exists masterList] } {
  # handle older database versions
  set musicdbList $masterList
  unset masterList
}

set ssdict [dict create]
set sscount 1

set c 0
dict for {fn data} $musicdbList {
  # build the ssdict now so that singletons can be identified
  if { [dict exists $data SAMESONG] } {
    set value [dict get $data SAMESONG]
    if { $value ne {} } {
      if { ! [dict exists $ssdict $value] } {
        dict set ssdict $value samesong $sscount
        dict set ssdict $value count 1
        set value $sscount
        incr sscount
      } else {
        set count [dict get $ssdict $value count]
        incr count
        dict set ssdict $value count $count
      }
    }
  }
  # get the total count
  incr c
}

set fh [open [file join $datatopdir data musicdb.dat] w]
puts $fh "#VERSION=10"
puts $fh "# Do not edit this file."
puts $fh "#RASIZE=2048"
puts $fh "#RACOUNT=$c"

set newrrn 1
dict for {fn data} $musicdbList {
  seek $fh [expr {($newrrn - 1) * $rsize + $hsize}]
  puts $fh "FILE\n..$fn"

  set haveoldvoladj false
  set oldvoladj 0
  set havevoladjperc false
  set voladjdone false

  # sort it now, make it easier
  foreach {tag} [lsort [dict keys $data]] {
    if { $tag eq "AFMODTIME" } { continue }
    if { $tag eq "ALBART" } { continue }
    if { $tag eq "BDJSYNCID" } { continue }
    if { $tag eq "DISPLAYIMG" } { continue }
    if { $tag eq "DURATION_HMS" } { continue }
    if { $tag eq "DURATION_STR" } { continue }
    if { $tag eq "NOMAXPLAYTIME" } { continue }
    if { $tag eq "UPDATEFLAG" } { continue }
    if { $tag eq "VARIOUSARTISTS" } { continue }
    if { $tag eq "WRITETIME" } { continue }
    # not going to do the sort-of-odd sometimes re-org stuff
    if { $tag eq "AUTOORGFLAG" } { continue }
    # FILE is already handled
    if { $tag eq "FILE" } { continue }
    # rrn is not written
    if { $tag eq "rrn" } { continue }

    set value [dict get $data $tag]

    if { $tag eq "TRACKTOTAL" || $tag eq "DISCTOTAL" } {
      if { $value == 0 } {
        # no point in writing not-useful data
        continue;
      }
    }

    if { $tag eq "ADJUSTFLAGS" } {
      # adjustflags is saved as a string of NTS characters.
      # spaces are no longer used.
      regsub -all { } $value {} value
      regsub -all {A} $value {S} value
    }

    if { $tag eq "SAMESONG" } {
      if { $value eq {} } {
        continue
      }
      if { [dict get $ssdict $value count] == 1 } {
        # singleton, clear the samesong setting
        continue;
      } else {
        set value [dict get $ssdict $value samesong]
      }
    }

    # volume handling

    if { $tag eq "VOLUMEADJUSTMENT" } {
      set haveoldvoladj true
      set oldvoladj $value
      continue;
    }
    if { $tag eq "VOLUMEADJUSTPERC" } {
      set havevoladjperc true
      # 10.0 converts the bdj3 value, 1000.0 is the bdj4 double multiplier
      set value [expr {$value / 10.0 * 1000.0}]
      # make sure the volume-adjust-perc has no decimal point in it.
      set value [format {%.0f} $value]
    }

    if { [string compare $tag "VOLUMEADJUST"] > 0 } {
      if { $haveoldvoladj && ! $havevoladjperc } {
        # bdj3 conversion
        set nvap [expr {double ($oldvoladj) / double ($defvol) * 100.0}]
        set nvap [expr {round ($nvap / 10.0)}]
        # bdj4 conversion
        set nvap [expr {$nvap / 10.0 * 1000.0}]
        set nvap [format {%.0f} $nvap]
        puts $fh "VOLUMEADJUSTPERC\n..$nvap"
      }
      set voladjdone true
    }

    if { $tag eq "SONGSTART" || $tag eq "SONGEND" } {
      if { $value ne {} && $value ne "-1" } {
        regexp {(\d+):(\d+)} $value all min sec
        regsub {^0*} $min {} min
        if { $min eq {} } { set min 0 }
        regsub {^0*} $sec {} sec
        if { $sec eq {} } { set sec 0 }
        set value [expr {($min * 60 + $sec)*1000}]
      }
    }

    if { $tag eq "MUSICBRAINZ_TRACKID" } { set tag RECORDING_ID }
    if { $tag eq "DISCNUMBER" } { set tag DISC }
    if { $tag eq "UPDATETIME" } { set tag LASTUPDATED }
    if { $tag eq "DURATION" } {
      set value [expr {int($value * 1000)}]
    }
    puts $fh "$tag\n..$value"
  }

  if { ! $voladjdone && $haveoldvoladj && ! $havevoladjperc } {
    # bdj3 conversion
    set nvap [expr {double ($oldvoladj) / double ($defvol) * 100.0}]
    set nvap [expr {round ($nvap / 10.0)}]
    # bdj4 conversion
    set nvap [expr {$nvap / 10.0 * 1000.0}]
    set nvap [format {%.0f} $nvap]
    puts $fh "VOLUMEADJUSTPERC\n..$nvap"
  }

  incr newrrn
}

# need to make sure the last record is full-size
# write out a null at the end-1
seek $fh [expr {($newrrn - 1) * $rsize + $hsize - 1}]
puts -nonewline $fh "\0"

close $fh
exit 0
