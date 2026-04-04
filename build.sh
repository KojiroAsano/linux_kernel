nasm -f bin boot.asm -o boot.bin
nasm -f bin loader.asm -o loader.bin
nasm -f bin kernel.asm -o kernel.bin

dd if=/dev/zero of=boot.img bs=512 count=200

dd if=boot.bin of=boot.img conv=notrunc
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc

qemu-system-x86_64 -drive format=raw,file=boot.img