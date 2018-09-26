cd kern/conf
./config ASST2
cd ../..
cd kern/compile/ASST2
bmake depend
bmake
bmake install
