all:server client
server:server.o threadpool.o condition.o
	g++ -g -lpthread server.o threadpool.o condition.o -o server
server.o:server.cpp define.h
	g++ -g -c server.cpp
threadpool.o:threadpool.cpp threadpool.h
	g++ -g -c threadpool.cpp
condition.o:condition.cpp condition.h
	g++ -g -c condition.cpp
client:client.o
	g++ -g client.o -o client
client.o:client.cpp define.h
	g++ -g -c client.cpp
clean:all
	rm all
