# Adapted from CPython's Dockerfile:

FROM buildpack-deps:jessie

# http://bugs.python.org/issue19846
# > At the moment, setting "LANG=C" on a Linux system *fundamentally breaks Python 3*, and that's not OK.
ENV LANG C.UTF-8


RUN git clone https://github.com/llvm-mirror/llvm.git ~/pyston_deps/llvm-trunk
RUN git clone https://github.com/llvm-mirror/clang.git ~/pyston_deps/llvm-trunk/tools/clang

RUN git clone https://github.com/dropbox/pyston.git ~/pyston

RUN git config --global user.email "pyston-patches@example.com"
RUN git config --global user.name "Pyston Patches"

RUN make -C ~/pyston llvm_up

RUN apt-get update
RUN apt-get install -yq cmake

# RUN sh -c 'set -ex; cd ~/pyston_deps/llvm-trunk; ./configure ; make package'
# RUN make -C ~/pyston_deps/llvm-trunk package
RUN sh -c 'set -ex ;\
    cd ~/pyston_deps; \
    mkdir llvm-trunk-build ;\
    cd llvm-trunk-build ;\
    cmake ../llvm-trunk ;\
    make package -j16 ;\
    '
