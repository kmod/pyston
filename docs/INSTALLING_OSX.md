*These instructions are a work in progress and highly experimental*

They were tested on OSX 10.9.5

## Dependencies

Install brew:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew doctor
```

```
brew install automake libtool gmp
brew install ccache
```

```
mkdir ~/pyston_deps
git clone https://github.com/dropbox/pyston.git ~/pyston
```

```
cd ~/pyston_deps
git clone http://llvm.org/git/llvm.git llvm-trunk
git clone http://llvm.org/git/clang.git llvm-trunk/tools/clang
git clone http://llvm.org/git/libcxx.git llvm-trunk/projects/libcxx
cd ~/pyston/src
make llvm_up
make llvm_configure
make llvm -j4
```

```
cd ~/pyston_deps
git clone git://github.com/vinzenz/pypa
mkdir pypa-install
cd pypa
./autogen.sh
./autogen.sh # yes have to run twice
./configure --prefix=$HOME/pyston_deps/pypa-install
make -j4
make install
```

```
brew install homebrew/dupes/gdb
```

Update xcode?

