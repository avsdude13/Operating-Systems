g++ -o CPU CPU.cc -Wall -Debug

./CPU /bin/printsleep /bin/printsleep2 /
bin/printsleep3

valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes -v ./CPU
