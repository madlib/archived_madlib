%define lapack_dir %(echo `cd ..;pwd`)
%define _topdir     %(echo `pwd`) 
%define name        lapack 
%define release     %{lapackrel}
%define version     %{lapackver} 
%define buildroot   %{_topdir}/install
%define buildarch   %(uname -p)
%define __os_install_post    %{nil}

BuildRoot:      %{buildroot}
Summary:        LAPACK library 
License:        Open Source        
Name:           %{name}
Version:        %{version}
Release:        %{release}
Group:          Development/Tools
Prefix:         /temp
AutoReq:        no
AutoProv:       no
BuildArch:      %{buildarch} 
Provides:       lapack-%{version}-%{release}.%{buildarch}.rpm 

%description
The LAPACK package provides LAPACK library for MADlib.

%prep
rm -rf %{buildroot}

%install
mkdir -p %{buildroot}/temp/lib
cp %{lapack_dir}/package/lib/liblapack* %{buildroot}/temp/lib/

%files
/temp
