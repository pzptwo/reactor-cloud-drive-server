# 云盘服务端：CloudServer + Reactor 框架 + 各层测试
FRAMEWORK_DIR = ../framework
FRAMEWORK_SRCS = $(FRAMEWORK_DIR)/InetAddress.cpp \
                 $(FRAMEWORK_DIR)/Socket.cpp \
                 $(FRAMEWORK_DIR)/Epoll.cpp \
                 $(FRAMEWORK_DIR)/Channel.cpp \
                 $(FRAMEWORK_DIR)/EventLoop.cpp \
                 $(FRAMEWORK_DIR)/TcpServer.cpp \
                 $(FRAMEWORK_DIR)/Acceptor.cpp \
                 $(FRAMEWORK_DIR)/Connection.cpp \
                 $(FRAMEWORK_DIR)/Buffer.cpp \
                 $(FRAMEWORK_DIR)/ThreadPool.cpp \
                 $(FRAMEWORK_DIR)/Timestamp.cpp

CXXFLAGS = -g -I. -I$(FRAMEWORK_DIR)
LDFLAGS = -lpthread -lmysqlclient

all: cloudserver test_protocol test_db test_fileutil stress_test

cloudserver: main.cpp cloudserver.cpp cloudserver.h protocol.cpp protocol.h db.cpp db.h fileutil.cpp fileutil.h
	g++ $(CXXFLAGS) -o cloudserver main.cpp cloudserver.cpp protocol.cpp db.cpp fileutil.cpp $(FRAMEWORK_SRCS) $(LDFLAGS)

test_protocol: test_protocol.cpp protocol.cpp protocol.h
	g++ $(CXXFLAGS) -o test_protocol test_protocol.cpp protocol.cpp

test_db: test_db.cpp db.cpp db.h
	g++ $(CXXFLAGS) -o test_db test_db.cpp db.cpp -lmysqlclient

test_fileutil: test_fileutil.cpp fileutil.cpp fileutil.h protocol.h
	g++ $(CXXFLAGS) -o test_fileutil test_fileutil.cpp fileutil.cpp

stress_test: stress_test.cpp
	g++ $(CXXFLAGS) -o stress_test stress_test.cpp -pthread

clean:
	rm -f cloudserver test_protocol test_db test_fileutil stress_test cloudDrive.db
	rm -rf test_fileutil_dir test_fileutil_dir_copy
