%define amcrel _RPM_RELEASE

Summary: Drivers to support enhanced IO for guests of AlacrityVM
Name: alacrityvm-drivers
Version: _RPM_VERSION
License: GPL
Release: %{amcrel}
Group: System/Kernel
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: kernel-syms module-init-tools

%description
Drivers to support enhanced IO for guests of AlacrityVM

Authors
--------------------------
  Gregory Haskins <ghaskins@novell.com>

%debug_package
%prep
%setup

%build

for flavor in %flavors_to_build; do  
    mkdir -p obj/$flavor  
    make %{?jobs:-j%jobs} -C  /usr/src/linux-obj/%_target_cpu/$flavor \
	M=$PWD O=$PWD/obj/$flavor modules
done  

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT  
export INSTALL_MOD_DIR=alacrityvm  

for flavor in %flavors_to_build; do  
    make -C /usr/src/linux-obj/%_target_cpu/$flavor modules_install \  
     M=$PWD O=$PWD/obj/$flavor  
done  

%clean
rm -rf obj

%files
%defattr(-,root,root)

%changelog
