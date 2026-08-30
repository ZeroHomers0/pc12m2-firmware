@echo off
set GHIDRA=D:/code/LPC1765FBD100/ghidra_12.1.3_PUBLIC_20260817/ghidra_12.1.3_PUBLIC
call "%GHIDRA%\support\analyzeHeadless.bat" D:/code/LPC1765FBD100/_tmp_proj/LPC1765FBD100 LPC1765FBD100 -process LPC1765.bin -noanalysis -scriptPath D:/code/LPC1765FBD100/decompiled/tools -postScript DumpRangeDisasm.java
