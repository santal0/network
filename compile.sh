rm -rf build
mkdir build
cd build
cmake .. 
make
make check_lab4
cd ..