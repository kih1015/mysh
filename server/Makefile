# 컴파일러
CC = gcc

# 컴파일 플래그
CFLAGS = -Wall -Wextra -g

# 타겟 실행 파일 이름
TARGET = server

# 소스 파일
SRCS = mysh.c server.c

# 기본 타겟
all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# 클린업
clean:
	rm -f $(TARGET)
