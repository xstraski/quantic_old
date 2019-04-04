@echo off
rem -----------------------
rem Lists all TODOs (Win32)
rem -----------------------

set Wildcard=*.cpp *.h

findstr "TODO(" %Wildcard%
