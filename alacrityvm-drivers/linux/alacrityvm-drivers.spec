%define rpmrel _RPM_RELEASE

Summary: Drivers to support enhanced IO for guests of AlacrityVM
Name: alacrityvm-drivers
Version: _RPM_VERSION
License: GPL
Release: %{rpmrel}
Group: System/Kernel
Source: %{name}-%{version}.tar.gz
Patch0: vbus-enet.patch
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: kernel-source kernel-syms module-init-tools
%suse_kernel_module_package ec2 xen xenpae vmi um 

%description
Drivers to support enhanced IO for guests of AlacrityVM

Authors
--------------------------
  Gregory Haskins <ghaskins@novell.com>

%package KMP 

Summary: AlacrityVM guest drivers KMP
Group: System/Kernel 

%description KMP 
Drivers to support enhanced IO for guests of AlacrityVM

Authors
--------------------------
  Gregory Haskins <ghaskins@novell.com>

%prep
%setup

%if 0%{?suse_version} <= 1110
%patch0
%endif

set -- * 
mkdir source 
mv "$@" source/ 
mkdir obj 

%build
export EXTRA_CFLAGS='-DVERSION=\"%version\"' 
for flavor in %flavors_to_build; do 
    rm -rf obj/$flavor 
    cp -r source obj/$flavor 
    make -C /usr/src/linux-obj/%_target_cpu/$flavor modules M=$PWD/obj/$flavor 
done 


%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT  
export INSTALL_MOD_DIR=updates  
for flavor in %flavors_to_build; do 
    make -C /usr/src/linux-obj/%_target_cpu/$flavor modules_install M=$PWD/obj/$flavor 
done

%post
/sbin/depmod -a 

%postun
/sbin/depmod -a 

%changelog
