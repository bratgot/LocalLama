# Reproducible Linux build environment for LlamaChat, targeting RHEL/Alma/Rocky 9
# (glibc 2.34). CUDA compiles without a GPU — you only need an NVIDIA GPU at
# RUNTIME on the airgapped target, not to build.
#
# Build the image:
#   docker build -t llamachat-build:rocky9 docker/
# Then compile + package (see docker/build-in-container.sh).
# CUDA 12.9 is needed to compile sm_120 (Blackwell), matching the Windows build's
# architecture range (sm_75..sm_120 + PTX).
FROM nvidia/cuda:12.9.1-devel-rockylinux9

# EPEL + CRB provide Qt6 and its build deps on Rocky 9.
RUN dnf -y install dnf-plugins-core epel-release \
 && dnf config-manager --set-enabled crb \
 && dnf -y install \
        gcc-c++ make cmake git \
        qt6-qtbase-devel qt6-qtbase-gui \
        which file findutils tar gzip \
 && dnf clean all

WORKDIR /work
