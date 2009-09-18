@echo off
REM ************** include\public\xen.h ************************************
sed "s/unsigned long/xen_ulong_t/g" include\xen\public\xen.h > include\xen\public\xen1.tmp

sed "s/xen\.h/win_xen\.h/g" include\xen\public\xen1.tmp > include\xen\public\win_xen.h

del include\xen\public\xen1.tmp

REM ************** include\public\arch-x86\xen.h ***************************
sed "s/unsigned long/xen_ulong_t/g" include\xen\public\arch-x86\xen.h > include\xen\public\arch-x86\xen1.tmp

sed "s/xen-x86_32\.h/win_xen-x86_32\.h/g" include\xen\public\arch-x86\xen1.tmp > include\xen\public\arch-x86\xen2.tmp

sed "s/xen-x86_64\.h/win_xen-x86_64\.h/g" include\xen\public\arch-x86\xen2.tmp > include\xen\public\arch-x86\win_xen.h

del include\xen\public\arch-x86\xen1.tmp
del include\xen\public\arch-x86\xen2.tmp

REM ************** include\public\arch-x86\xen-x86_32.h *******************
sed "s/unsigned long/xen_ulong_t/g" include\xen\public\arch-x86\xen-x86_32.h > include\xen\public\arch-x86\win_xen-x86_32.h

REM ************** include\public\arch-x86\xen-x86_64.h *******************
sed "s/unsigned long/xen_ulong_t/g" include\xen\public\arch-x86\xen-x86_64.h > include\xen\public\arch-x86\win_xen-x86_64.h
@echo on