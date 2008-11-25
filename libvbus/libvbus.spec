%define rpmrel _RPM_RELEASE

BuildRequires: boost-devel gcc-c++

Summary: libvbus - Userspace interface to kernel Virtual-Bus
Name: libvbus
Version: _RPM_VERSION
License: GPL
Release: %{rpmrel}
Requires: boost
Group: System
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Provides userspace access to the configfs/sysfs/procfs interfaces for
the Virtual-Bus subsystem

Authors
--------------------------
  Gregory Haskins <ghaskins@novell.com>

%debug_package
%prep
%setup

%build
make

%install
make install PREFIX=$RPM_BUILD_ROOT

%clean
make clean

%files
%defattr(-,root,root)
%{libdir}/libvbus.so

%changelog
