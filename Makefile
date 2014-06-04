

INCLUDE_PATHS:=\

SOURCES:=\
    main.cpp 				\
    console_logger.cpp		\
    threadpool.cpp			\
    workthread.cpp			\
    system.cpp				\

TARGET:=t_test

ARCHITECTURES:=x86_64

DEFINES:=\

WARNINGS:=\

SYMBOLS=-g

FARGUMENTS:= \
#	no-rtti \
	no-exceptions

LIBRARY_PATHS:=\
	/usr/lib/gcc/x86_64-linux-gnu/4.7/ \

LIBRARIES:=\
	stdc++ \
	pthread \

include clang.mk
