nasm -f bin -o boot.bin boot.asm
nasm -f bin -o loader.bin loader.asm
nasm -f bin -o kernel.bin kernel.asm

rm boot.img
dd if=/dev/zero of=boot.img bs=512 count=100

dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc


# kernel（5セクタ）
# dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc


qemu-system-x86_64 \
  -drive format=raw,file=boot.img \
  -cpu qemu64 \
  -m 512M \
  -no-reboot \
  -d int,cpu_reset