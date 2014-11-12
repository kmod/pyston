*These instructions are a work in progress and highly experimental*

They were tested on OSX 10.9.5

## Dependencies

Install brew:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew doctor
```

```
mkdir ~/pyston_deps
git clone https://github.com/dropbox/pyston.git ~/pyston
```

```
cd ~/pyston_deps
git clone http://llvm.org/git/llvm.git llvm-trunk
git clone http://llvm.org/git/clang.git llvm-trunk/tools/clang
cd ~/pyston/src
make llvm_up
make llvm_configure
make llvm -j4
```

```
brew install ccache
```

Update xcode?

