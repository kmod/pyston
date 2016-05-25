# Copyright (c) 2014-2016 Dropbox, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM ubuntu:14.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -yq build-essential automake git cmake ninja-build ccache libncurses5-dev liblzma-dev libreadline-dev libgmp3-dev libmpfr-dev autoconf libtool texlive-extra-utils clang libssl-dev libsqlite3-dev pkg-config libbz2-dev git

# These shallow commands save a decent amount of space, but mess with our go-to-rev tool:
# WORKDIR /root/pyston_deps/llvm-trunk
# RUN git init && git remote add origin https://github.com/llvm-mirror/llvm.git && git fetch origin release_36 --depth=21000 && git reset --hard FETCH_HEAD
# WORKDIR /root/pyston_deps/llvm-trunk/tools/clang
# RUN git init && git remote add origin https://github.com/llvm-mirror/clang.git && git fetch origin release_37 --depth=3000 && git reset --hard FETCH_HEAD
RUN git clone https://github.com/llvm-mirror/llvm.git /root/pyston_deps/llvm-trunk
RUN git clone https://github.com/llvm-mirror/clang.git /root/pyston_deps/llvm-trunk/tools/clang

RUN git config --global user.email "docker@pyston.com" && git config --global user.name "docker"

ADD . /pyston_build
WORKDIR /pyston_build
# git clone https://github.com/dropbox/pyston.git /pyston --depth=1 && \
# git submodule update --init --recursive build_deps && \
RUN true && \
    make llvm_up && \
    make package_nonpgo
# make pyston_release && \
# cp pyston_release pyston

# I wonder if this will work:
ENTRYPOINT ["/pyston/pyston"]

# Create a default virtualenv?  Install cython into it?
