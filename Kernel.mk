cp arch/arm/boot/zImage bootimg/
cd bootimg/
./repack-bootimg.pl zImage cm/ \#$(cat $(pwd)/../.version)-$(stat -c %y $(pwd)/../arch/arm/boot/zImage | cut -b1-10).img
echo "Pack boot image done!"
