
builddir := "./.build"
installdir := "./.prefix"

configure:
    mkdir -p {{builddir}}
    cd {{builddir}} && ../configure \
        --target-list=arm-softmmu,i386-softmmu,riscv32-softmmu,x86_64-softmmu \
        --ninja="$(which ninja)" \
        --enable-gtk \
        --enable-vnc \
        --enable-vnc-jpeg \
        --enable-vnc-sasl \
        --enable-vte \
        --enable-slirp \
        --prefix="{{justfile_directory()}}/{{installdir}}"

build: build-arm build-riscv32 build-i386 build-x86_64
    
build-arm:
    ninja -C {{builddir}} qemu-system-arm

build-riscv32:
    ninja -C {{builddir}} qemu-system-riscv32

build-i386:
    ninja -C {{builddir}} qemu-system-i386

build-x86_64:
    ninja -C {{builddir}} qemu-system-x86_64

build-all:
    ninja -C {{builddir}}

install: build-all
    ninja -C {{builddir}} install

clean:
    rm -rf {{builddir}}
