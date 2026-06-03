#define _CRT_SECURE_NO_WARNINGS
/* MSVC 컴파일러에서 fopen, strerror 같은 표준 C 함수 경고를 끄기 위한 설정입니다. */

#include <ctype.h>
/* isprint 함수처럼 문자가 화면에 보이는 문자인지 확인하는 함수가 들어 있습니다. */

#include <errno.h>
/* 파일 열기 실패 같은 오류 원인을 errno 값으로 확인할 때 필요합니다. */

#include <stdio.h>
/* FILE, fopen, fgetc, fclose, printf, fprintf 같은 입출력 함수가 들어 있습니다. */

#include <stdlib.h>
/* EXIT_SUCCESS, EXIT_FAILURE 같은 프로그램 종료 상태 상수가 들어 있습니다. */

#include <string.h>
/* strerror 함수처럼 오류 번호를 설명 문자열로 바꾸는 함수가 들어 있습니다. */

#define CHAR_COUNT 256
/* unsigned char는 0부터 255까지 표현하므로, 가능한 문자/바이트 개수는 256개입니다. */

/*
 * 이 프로그램은 1-gram을 "단어"가 아니라 "문자 1개" 기준으로 셉니다.
 *
 * 여기서 말하는 문자는 정확히는 C의 1바이트 문자입니다.
 * 영어 알파벳, 숫자, 기본 기호는 사람이 기대하는 문자 단위로 잘 셉니다.
 * 하지만 UTF-8 한글은 한 글자가 여러 바이트로 저장되므로,
 * 한글 한 글자를 하나로 세지 않고 내부 바이트값 여러 개로 셉니다.
 */

int main(int argc, char *argv[])
/* main은 C 프로그램의 시작점입니다. argc는 인자 개수, argv는 인자 문자열 배열입니다. */
{
    /* 입력 파일을 가리킬 파일 포인터 변수입니다. */
    FILE *input_file;

    /* 각 문자/바이트가 몇 번 나왔는지 저장하는 배열입니다. 처음 값은 모두 0입니다. */
    size_t counts[CHAR_COUNT] = {0};

    /* 실제로 등장한 문자 번호만 모아서 정렬할 배열입니다. */
    int order[CHAR_COUNT];

    /* order 배열에 몇 개의 문자 번호가 들어갔는지 저장합니다. */
    int used_count = 0;

    /* fgetc로 파일에서 읽은 문자 하나를 저장합니다. EOF도 담아야 하므로 int를 씁니다. */
    int ch;

    /* 반복문에서 사용할 인덱스 변수입니다. */
    int i;

    /* 이중 반복문에서 사용할 두 번째 인덱스 변수입니다. */
    int j;

    /* 프로그램 실행 인자가 정확히 2개인지 확인합니다. */
    if (argc != 2) {
        /* 인자가 잘못되면 사용법을 오류 출력(stderr)에 보여줍니다. */
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);

        /* 실패 상태로 프로그램을 종료합니다. */
        return EXIT_FAILURE;
    }

    /* argv[1]은 사용자가 입력한 파일 경로입니다. rb는 binary read, 즉 바이트 단위 읽기 모드입니다. */
    input_file = fopen(argv[1], "rb");

    /* fopen이 실패하면 NULL이 들어옵니다. */
    if (input_file == NULL) {
        /* 파일을 열 수 없다는 메시지와 실제 오류 원인을 출력합니다. */
        fprintf(stderr, "error: cannot open '%s': %s\n", argv[1], strerror(errno));

        /* 파일을 못 열었으므로 실패 상태로 종료합니다. */
        return EXIT_FAILURE;
    }

    /* fgetc로 파일에서 문자/바이트를 하나씩 읽고, 파일 끝 EOF를 만나면 반복을 멈춥니다. */
    while ((ch = fgetc(input_file)) != EOF) {
        /* ch를 0부터 255 범위의 unsigned char 값으로 바꿉니다. */
        unsigned char byte = (unsigned char)ch;

        /* 해당 byte 값의 등장 횟수를 1 증가시킵니다. */
        counts[byte]++;
    }

    /* 반복이 끝난 이유가 정상적인 파일 끝인지, 읽기 오류인지 확인합니다. */
    if (ferror(input_file)) {
        /* 읽기 오류가 발생했다는 메시지를 출력합니다. */
        fprintf(stderr, "error: failed to read '%s'\n", argv[1]);

        /* 열어 둔 파일을 닫습니다. */
        fclose(input_file);

        /* 읽기 실패이므로 실패 상태로 종료합니다. */
        return EXIT_FAILURE;
    }

    /* 파일 읽기가 끝났으므로 파일을 닫습니다. */
    fclose(input_file);

    /* 0부터 255까지 모든 문자/바이트 번호를 확인합니다. */
    for (i = 0; i < CHAR_COUNT; i++) {
        /* 한 번이라도 등장한 문자/바이트만 출력 대상에 넣습니다. */
        if (counts[i] > 0) {
            /* 등장한 문자 번호 i를 order 배열에 저장합니다. */
            order[used_count] = i;

            /* order 배열에 저장된 개수를 1 늘립니다. */
            used_count++;
        }
    }

    /* order 배열을 직접 정렬합니다. 기준은 빈도 내림차순, 빈도가 같으면 문자 번호 오름차순입니다. */
    for (i = 0; i < used_count - 1; i++) {
        /* i 뒤쪽에 있는 값들과 비교하기 위한 반복문입니다. */
        for (j = i + 1; j < used_count; j++) {
            /* 현재 앞쪽 후보 문자 번호입니다. */
            int left = order[i];

            /* 현재 뒤쪽 비교 대상 문자 번호입니다. */
            int right = order[j];

            /* right가 더 많이 나왔거나, 빈도가 같으면서 문자 번호가 더 작으면 위치를 바꿉니다. */
            if (counts[right] > counts[left] ||
                (counts[right] == counts[left] && right < left)) {
                /* 두 값을 바꾸기 위해 임시 변수에 order[i]를 저장합니다. */
                int temp = order[i];

                /* 뒤쪽에 있던 더 우선순위 높은 값을 앞쪽으로 옮깁니다. */
                order[i] = order[j];

                /* 임시로 저장해 둔 기존 앞쪽 값을 뒤쪽으로 옮깁니다. */
                order[j] = temp;
            }
        }
    }

    /* 결과 표의 헤더를 출력합니다. CHAR는 문자, COUNT는 등장 횟수입니다. */
    printf("%-12s %10s\n", "CHAR", "COUNT");

    /* 헤더 아래 구분선을 출력합니다. */
    printf("%-12s %10s\n", "----", "-----");

    /* 정렬된 문자 목록을 처음부터 끝까지 출력합니다. */
    for (i = 0; i < used_count; i++) {
        /* order[i]에 저장된 문자 번호를 unsigned char로 바꿉니다. */
        unsigned char byte = (unsigned char)order[i];

        /* 공백 문자는 화면에 잘 보이지 않으므로 space라는 이름으로 출력합니다. */
        if (byte == ' ') {
            /* 공백 문자와 그 등장 횟수를 출력합니다. */
            printf("%-12s %10zu\n", "space", counts[byte]);

        /* 줄바꿈 문자도 화면에 직접 보이지 않으므로 \n으로 출력합니다. */
        } else if (byte == '\n') {
            /* 줄바꿈 문자와 그 등장 횟수를 출력합니다. */
            printf("%-12s %10zu\n", "\\n", counts[byte]);

        /* Windows 텍스트 파일에 나올 수 있는 carriage return 문자를 \r로 출력합니다. */
        } else if (byte == '\r') {
            /* carriage return 문자와 그 등장 횟수를 출력합니다. */
            printf("%-12s %10zu\n", "\\r", counts[byte]);

        /* 탭 문자는 화면에서 폭이 애매하므로 \t로 출력합니다. */
        } else if (byte == '\t') {
            /* 탭 문자와 그 등장 횟수를 출력합니다. */
            printf("%-12s %10zu\n", "\\t", counts[byte]);

        /* 화면에 보이는 일반 문자인지 확인합니다. */
        } else if (isprint(byte)) {
            /* 일반 문자는 작은따옴표 안에 실제 문자로 출력합니다. */
            printf("'%c'          %10zu\n", byte, counts[byte]);

        /* 위 조건에 걸리지 않는 문자는 보이지 않는 제어 문자나 특수 바이트입니다. */
        } else {
            /* 보이지 않는 값은 16진수 형태로 출력합니다. 예: 0x00, 0x1B */
            printf("0x%02X         %10zu\n", byte, counts[byte]);
        }
    }

    /* 모든 작업이 성공했으므로 성공 상태로 프로그램을 종료합니다. */
    return EXIT_SUCCESS;
}
