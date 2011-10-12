%define lapack_dir %(echo `cd ..;pwd`)
%define _topdir     %(echo `pwd`) 
%define name        lapack 
%define release     37 
%define version     3.0
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
The Pgcrypto package provides cryptographic package for the Greenplum Database.

%prep
rm -rf %{buildroot}

%install
mkdir -p %{buildroot}/temp/lib
cp %{lapack_dir}/usr/lib64/liblapack* %{buildroot}/temp/lib/

%files
/temp
