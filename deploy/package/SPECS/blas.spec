%define blas_dir %(echo `cd ..;pwd`)
%define _topdir     %(echo `pwd`) 
%define name        blas 
%define release     %{blasrel} 
%define version     %{blasver}
%define buildroot   %{_topdir}/install
%define buildarch   %(uname -p)
%define __os_install_post    %{nil}

BuildRoot:      %{buildroot}
Summary:        BLAS library for MADlib 
License:        Open Source        
Name:           %{name}
Version:        %{version}
Release:        %{release}
Group:          Development/Libraries
Prefix:         /temp
AutoReq:        no
AutoProv:       no
BuildArch:      %{buildarch} 
Provides:       blas-%{version}-%{release}.%{buildarch}.rpm 

%description
BLAS library provides an implementation of BLAS subroutines for the MADlib library.

%prep
rm -rf %{buildroot}

%install
mkdir -p %{buildroot}/temp/lib
cp %{blas_dir}/usr/lib64/libblas* %{buildroot}/temp/lib/

%files
/temp
