g++ -pthread -lpthread -Wall -o Race Race.cc

gcc -pthread -lpthread -lstdc++ -Wall -o Race Race.cc

Requires an argument or throws sig fault
./Race 1

valgrind --leak-check=full --show-leak-kinds=all --trace-children=yes -v ./Race
