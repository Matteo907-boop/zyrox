#clang++ -O0 -fpass-plugin=./cmake-build-debug/libzyrox.so main.cpp -o ./out/main.so
#echo "running code..."
#./out/main.so
clang -O0 -flto=full -c main.c -o out/main.o
clang -flto=full -fuse-ld=lld -Wl,--load-pass-plugin=../cmake-build-debug/libzyrox.so out/main.o -o out/main.so

echo "running code..."
./out/main.so