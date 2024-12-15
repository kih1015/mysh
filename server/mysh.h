#ifndef CUSTOM_SHELL_H
#define CUSTOM_SHELL_H

/* 함수 프로토타입 */
void init(void);                         // 초기화 함수
char* execute(char* command);            // 명령어 실행 함수
void send_info();                        // 폴더 내용 정보 전송 함수
void get_realpath(char *usr_path, char *result);

#endif // CUSTOM_SHELL_H