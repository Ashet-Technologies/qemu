
configure:
    mkdir -p ./build
    cd ./build && ../configure \
        --target-list=arm-softmmu,i386-softmmu,riscv32-softmmu,x86_64-softmmu \
        --ninja="$(which ninja)" \
        --enable-gtk \
        --enable-vnc \
        --enable-vnc-jpeg \
        --enable-vnc-sasl \
        --enable-vte \
        --prefix="{{justfile_directory()}}/prefix"

build:
    ninja -C ./build
    ninja -C ./build install

clean:
    rm -rf ./build
