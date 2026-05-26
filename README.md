# C Word 1-Gram Counter

이 저장소는 C로 구현한 단어 기준 1-gram 빈도 분석 프로그램입니다.
입력 파일을 읽어 단어별 등장 횟수를 세고, 빈도가 높은 순서로 결과를 출력합니다.

## 알고리즘 개요

프로그램은 파일을 바이트 단위로 읽으면서 하나의 단어 토큰을 동적으로 구성합니다.
ASCII 공백, 구두점, 제어 문자를 만나면 현재까지 모은 토큰을 하나의 단어로 확정하고 해시 테이블에 누적합니다.

단어 빈도 저장에는 separate chaining 방식의 해시 테이블을 사용합니다.
각 노드는 다음 정보를 가집니다.

```c
typedef struct WordCount {
    char *word;
    size_t count;
    struct WordCount *next;
} WordCount;
```

새 단어가 들어오면 FNV-1a 기반 해시값으로 bucket을 찾고, 같은 단어가 이미 있으면 `count`를 증가시킵니다.
처음 등장한 단어라면 새 노드를 만들어 bucket 연결 리스트 앞에 삽입합니다.
해시 테이블의 load factor가 약 75%에 도달하면 bucket 수를 두 배로 늘리고 기존 노드를 재배치합니다.

출력 단계에서는 해시 테이블의 노드 포인터를 배열에 모은 뒤 `qsort`로 정렬합니다.
정렬 기준은 다음과 같습니다.

1. 빈도 내림차순
2. 빈도가 같으면 단어 사전순 오름차순

## 토큰화 정책

- ASCII 공백 문자는 단어 구분자로 처리합니다.
- ASCII 구두점과 제어 문자도 단어 구분자로 처리합니다.
- ASCII 알파벳은 소문자로 정규화합니다.
- UTF-8 비ASCII 바이트는 그대로 보존합니다.

이 정책 때문에 `Hello, hello world!`는 `hello` 2회, `world` 1회로 집계됩니다.
한국어처럼 비ASCII 문자를 사용하는 텍스트는 공백과 ASCII 구두점 기준으로 나뉘므로 `안녕, 안녕 세상`은 `안녕` 2회, `세상` 1회로 집계됩니다.

## 빌드

GCC 또는 Clang:

```sh
gcc -std=c11 -Wall -Wextra -O2 -o ngram1 ngram1.c
```

MSVC:

```bat
cl /std:c11 /W4 ngram1.c
```

## 실행

```sh
./ngram1 input.txt
```

Windows PowerShell에서 MSVC로 빌드한 경우:

```powershell
.\ngram1.exe .\input.txt
```

## 예시

입력:

```text
Hello, hello world!
```

출력:

```text
WORD       COUNT
----       -----
hello          2
world          1
```

## 오류 처리

- 입력 파일 인자가 없으면 사용법을 출력하고 non-zero 상태로 종료합니다.
- 파일을 열 수 없으면 오류 메시지를 출력하고 non-zero 상태로 종료합니다.
- 메모리 할당 실패 또는 읽기 실패가 발생하면 오류 메시지를 출력하고 종료합니다.
- 빈 파일은 헤더만 출력합니다.
