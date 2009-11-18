%define amcrel _RPM_RELEASE

Summary: Drivers to support enhanced IO for guests of AlacrityVM
Name: alacrityvm-drivers
Version: _RPM_VERSION
License: GPL
Release: %{amcrel}
Group: System/Kernel
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Drivers to support enhanced IO for guests of AlacrityVM

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

%changelog
