@echo off
: BDJ4 clean

set bdj4dir="#BDJ4DIR#"
if exist "%bdj4dir%" (
  %bdj4dir%\bin\bdj4 --bdj4cleantmp
)