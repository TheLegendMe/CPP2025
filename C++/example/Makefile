PROJECT_PATH=/home/oym/muduo/A-Tiny-Network-Library
HEADER_PATH=-I ${PROJECT_PATH}/src/base \
			-I ${PROJECT_PATH}/src/net \
			-I ${PROJECT_PATH}/src/net/poller \
			-I ${PROJECT_PATH}/src/logger \
			-I ${PROJECT_PATH}/src/timer \
			-I ${PROJECT_PATH}/src/memory \
			-I ${PROJECT_PATH}/src/mysql \
			-I ${PROJECT_PATH}/src/http \
			-I ${PROJECT_PATH}/example/Login

SRC_FILES = ${PROJECT_PATH}/src/base/*.cc \
            ${PROJECT_PATH}/src/net/*.cc \
            ${PROJECT_PATH}/src/net/poller/*.cc \
            ${PROJECT_PATH}/src/logger/*.cc \
            ${PROJECT_PATH}/src/timer/*.cc \
			${PROJECT_PATH}/src/memory/*.cc \
			${PROJECT_PATH}/src/mysql/*.cc \
			${PROJECT_PATH}/src/http/*.cc \
			${PROJECT_PATH}/example/Login/*.cc

#LIB_PATH=-L${PROJECT_PATH}/lib
LIBS= -lpthread -lmysqlclient
CFLAGS= -g -Wall ${LIB_PATH} ${HEADER_PATH} 

#EchoServer: echoServer.cc
#	g++ echoServer.cc ${CFLAGS}  ${SRC_FILES} ${LIBS} -o EchoServer

Http: http.cc 
	g++ http.cc ${CFLAGS}  ${SRC_FILES} ${LIBS} -o Http
#EchoServerAsync: echoServerAsync.cc
#	g++ echoServerAsync.cc ${CFLAGS} ${SRC_FILES} -o EchoServerAsync

#SortedList:sortedlist.cc
#	g++ sortedlist.cc ${CFLAGS} ${SRC_FILES} -o SortedList
clean: 
	rm -f EchoServer EchoServerAsync SortedList Http 