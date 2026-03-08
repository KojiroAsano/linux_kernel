nasm -f bin boot.asm -o boot.bin
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
qemu-system-i386 -drive format=raw,file=boot.img -boot a