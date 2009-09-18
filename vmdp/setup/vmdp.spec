#
# spec file for package vmdp (Version 1.1.0)
#
# Copyright (c) 2007 Novell, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments via http://support.novell.com
#

# usedforbuild    aaa_base acl attr audit-libs autoconf automake bash binutils bzip2 cdrkit-cdrtools-compat coreutils cpio cpp cpp42 cracklib cvs diffutils file filesystem fillup findutils gawk gcc gcc42 gdbm genisoimage gettext gettext-devel glibc glibc-devel glibc-locale grep groff gzip icedax info insserv less libacl libattr libbz2-1 libbz2-devel libcap libdb-4_5 libgcc42 libgomp42 libltdl-3 libmudflap42 libstdc++42 libtool libvolume_id libxcrypt libzio linux-kernel-headers m4 make man mktemp ncurses net-tools netcfg pam pam-modules patch perl perl-base permissions popt readline rpm sed sysvinit tar texinfo timezone util-linux wodim zlib

Name:           vmdp
Version:        1.1.0
Release:        1
Source0:        vmdp-xen-windows.tar.bz2
Source1:        vmdp-xen-rhel.tar.bz2
Source2:        vmdp-doc.tar.bz2
Source3:        
BuildArch:      noarch
Group:          System/Kernel
License:        GPL
AutoReqProv:    no
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  mkisofs
Summary:        SUSE Linux Enterprise Virtual Machine Driver Pack

%description
SUSE Linux Enterprise Virtual Machine Driver Pack contains virtual device
drivers designed to improve network and disk performance in virtual machines.

%package xen-winxp
Summary:        SUSE Drivers for Windows XP on Xen
Version:        1.1.0
License:        Any commercial
Requires:       vmdp
Group:          System/Kernel

%description xen-winxp
SUSE Drivers for Windows XP on Xen are paravirtualized device drivers designed
to enhance network and disk performance in Windows XP virtual machines on Xen.
This package provides 32-bit and 64-bit drivers for Windows XP.

%package xen-win2003
Summary:        SUSE Drivers for Windows Server 2003 on Xen
Version:        1.1.0
License:        Any commercial
Requires:       vmdp
Group:          System/Kernel

%description xen-win2003
SUSE Drivers for Windows Server 2003 on Xen are paravirtualized device drivers
designed to enhance network and disk performance in Windows Server 2003 virtual
machines on Xen.  This package provides 32-bit and 64-bit drivers for Windows
Server 2003.

%package xen-win2000
Summary:        SUSE Drivers for Windows 2000 Server on Xen
Version:        1.1.0
License:        Any commercial
Requires:       vmdp
Group:          System/Kernel

%description xen-win2000
SUSE Drivers for Windows 2000 Server on Xen are paravirtualized device drivers
designed to enhance network and disk performance in Windows 2000 Server virtual
machines on Xen.  This package provides 32-bit drivers for Windows 2000 Server.

%package xen-rhel4
Summary:        SUSE Drivers for Red Hat Enterprise Linux 4 on Xen
Requires:       vmdp
Group:          System/Kernel

%description xen-rhel4
SUSE Drivers for Red Hat Enterprise Linux 4 on Xen are paravirtualized device
drivers designed to enhance network and disk performance in Red Hat Enterprise
Linux 4 virtual machines on Xen.  This package provides 32-bit and 64-bit 
drivers for Red Hat Enterprise Linux 4 U5.

%package xen-rhel5
Summary:        SUSE Drivers for Red Hat Enterprise Linux 5 on Xen
Requires:       vmdp
Group:          System/Kernel

%description xen-rhel5
SUSE Drivers for Red Hat Enterprise Linux 5 on Xen are paravirtualized device
drivers designed to enhance network and disk performance in Red Hat Enterprise
Linux 5 virtual machines on Xen.  This package provides 32-bit and 64-bit 
drivers for Red Hat Enterprise Linux 5.

%prep
%setup -c
%setup -c -D -T -a 1
%setup -c -D -T -a 2

%build

%install
ISO_OPTIONS="-r -J -l -d -allow-multidot -allow-leading-dots -no-bak"
DST=/opt/novell/vm-driver-pack/xen
mkdir -p $RPM_BUILD_ROOT/$DST
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-Win2000-32bit.iso w2k-32/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-Win2003-32bit.iso w2k3-32/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-WinXP-32bit.iso xp-32/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-Win2003-64bit.iso w2k3-64/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-WinXP-64bit.iso w2k3-64/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-WinXP-64bit.iso w2k3-64/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-RHEL4_U5-32bit.iso rhel4-32/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-RHEL4_U5-64bit.iso rhel4-64/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-RHEL5-32bit.iso rhel5-32/
mkisofs $ISO_OPTIONS -o $RPM_BUILD_ROOT/$DST/vmdp-xen-RHEL5-64bit.iso rhel5-64/
install -m 0644 %{SOURCE3} $RPM_BUILD_ROOT/$DST/
install -m 0755 -d $RPM_BUILD_ROOT/usr/share/doc/packages/vmdp
install -m 0644 %{SOURCE2} $RPM_BUILD_ROOT/usr/share/doc/packages/vmdp

%clean
test ! -z "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir /opt/novell
%dir /opt/novell/vm-driver-pack
%dir /opt/novell/vm-driver-pack/xen
%dir /usr/share/doc/packages/vmdp
/opt/novell/vm-driver-pack/xen/README
/usr/share/doc/packages/vmdp/*

%files xen-win2000
%defattr(0644,root,root)
/opt/novell/vm-driver-pack/xen/vmdp-xen-Win2000-32bit.iso

%files xen-win2003
%defattr(0644,root,root)
/opt/novell/vm-driver-pack/xen/vmdp-xen-Win2003-32bit.iso
/opt/novell/vm-driver-pack/xen/vmdp-xen-Win2003-64bit.iso

%files xen-winxp
%defattr(0644,root,root)
/opt/novell/vm-driver-pack/xen/vmdp-xen-WinXP-32bit.iso
/opt/novell/vm-driver-pack/xen/vmdp-xen-WinXP-64bit.iso

%files xen-rhel4
%defattr(0644,root,root)
/opt/novell/vm-driver-pack/xen/vmdp-xen-RHEL4_U5-32bit.iso
/opt/novell/vm-driver-pack/xen/vmdp-xen-RHEL4_U5-64bit.iso

%files xen-rhel5
%defattr(0644,root,root)
/opt/novell/vm-driver-pack/xen/vmdp-xen-RHEL5-32bit.iso
/opt/novell/vm-driver-pack/xen/vmdp-xen-RHEL5-64bit.iso

%changelog
* Fri Jun 28 2007 - jdouglas@novell.com
- Initial package.
