# RV32IM 어셈블러

단일 RISC-V 어셈블리 소스를 읽어 CPUlator에서 실행할 수 있는 리틀엔디안 raw 바이너리를 만드는 C 프로그램입니다. 적재 시작 주소는 `0x00010000`입니다.

과제의 기준 입력은 [examples/baseline.s](examples/baseline.s)입니다. 산술, 전방 분기, 정지 루프와 초기화된 데이터가 작은 파일 하나에 들어 있어 파싱부터 바이너리 배치까지 확인할 수 있습니다.

## 실행

프로젝트 루트에서 다음 명령을 실행합니다.

```sh
make
./read_asm examples/baseline.s
hexdump -C baseline.bin
make test
```

입력 경로의 마지막 파일명을 기준으로 현재 작업 폴더에 `.bin`을 생성합니다. 성공하면 터미널에 메시지를 출력하지 않습니다. `make test`에 보이는 오류 진단 중 일부는 실패해야 하는 입력을 확인하는 테스트에서 출력됩니다.

| 파일                    | 사용 시점          | 역할                                                    |
| ----------------------- | ------------------ | ------------------------------------------------------- |
| `read_asm`            | 맥 터미널에서 실행 | `make`가 빌드한 어셈블러                              |
| `first.s`             | 최초 출력 점검     | `addi` 세 줄을 입력하는 12바이트 테스트               |
| `examples/baseline.s` | 배치까지 점검      | 라벨, 분기, `.text`, `.data`, `.word`를 함께 확인 |
| `baseline.bin`        | CPUlator에 적재    | 위 소스를 변환한 36바이트의 RISC-V raw 이미지           |

프로그램은 C11 컴파일러와 `make`로 빌드합니다. 입력 경로를 인자로 하나 넘겨 실행합니다. 시작 주소를 설정한 뒤 CPUlator의 **Memory → Load from file**에서 생성된 바이너리를 선택합니다. [CPUlator 메모리 적재 문서](https://cpulator.01xz.net/doc/#loading-and-saving-memory-contents)

CPUlator의 `rv32-de10-lite` 시스템에서 Memory의 파일 형식을 **Raw**, 시작 주소와 레지스터 `pc`를 각각 `0x00010000`으로 설정합니다. `baseline.bin`을 로드하고 네 명령을 Step으로 실행하면 `t2 = 5`, `pc = 0x00010014`가 기대 결과입니다. [CPUlator 메모리 및 레지스터 문서](https://cpulator.01xz.net/doc/#memory-window)

## 현재 구현 범위

- R/I/S/B/U/J 인코딩과 일반 정수, 메모리, 분기, 점프 명령, CSR 명령 6종을 지원합니다. 실제 명령 테이블 56개 중 51개를 처리합니다.
- 라벨과 `.equ` 상수를 등록하고, 분기·점프의 라벨은 대상 주소에서 현재 명령 주소를 뺀 오프셋으로 바꿉니다.
- `.text`, `.data`, `.bss`, `.globl`·`.global`, `.equ`, `.byte`, `.half`, `.word`, `.ascii`, `.asciz`, `.space`, `.align`을 처리합니다.
- 테이블에 등록된 가상 명령어 23종과 피연산자 하나를 쓰는 `jal label`을 지원합니다. `li`는 값에 따라 한 명령 또는 두 명령, `la`는 두 명령으로 확장합니다.
- 남은 범위는 시스템 명령 `ecall`, `ebreak`, `fence`, `mret`, `wfi`와 일반 산술식, 선택 사항인 `.lst` 출력입니다. 재검증에서 예제 2,3,4번은 바이너리를 생성했고, 1번은 `mret`을 기계어 워드로 치환한 임시 복사본에서 변환을 확인했습니다. [예제별 확인 결과](docs/RESULTS.md#배포-예제-진행-상태)

### 모듈 사이의 데이터 흐름

1. `parse_line()`이 입력 줄을 `ParsedLine`으로 나눕니다. 아직 숫자로 읽지 않은 피연산자와 라벨 이름은 원본 줄 버퍼를 가리킵니다.
2. `collect_layout()`이 상수와 섹션 크기를 계산하고 라벨의 절대 주소를 확정합니다. `.data`와 `.bss`의 시작은 각각 이전 섹션 끝을 16바이트 경계로 올린 주소입니다.
3. 출력 단계에서 `parse_instruction()`이 실제 명령의 문자열 피연산자를 정수로 바꿉니다. 가상 명령은 `expand_pseudo()`가 실제 명령으로 바꾸고, `encode_instruction()`이 포맷별 비트 배치로 32비트 워드를 만듭니다.
4. `write_word_le()`가 명령어 워드를 하위 바이트부터 기록합니다. 데이터 지시어도 같은 출력 파일에 배치하고, 성공한 경우에 최종 `.bin` 이름으로 바꿉니다.

| 파일 묶음                                                             | 역할                                                      |
| --------------------------------------------------------------------- | --------------------------------------------------------- |
| `main.c`, `src/layout.*`                                          | 파일 제어, 라벨 주소 수집, 섹션 배치, 최종 파일 크기 결정 |
| `src/parse.*`, `src/operands.h`, `src/values.*`                 | 입력 문법 처리와 포맷별 피연산자 변환                     |
| `src/*_table.h`, `src/format_finder.c`, `src/register_helper.c` | 명령 메타데이터,레지스터,지시어 조회                      |
| `src/pseudo.c`, `src/pseudo_table.h`                              | 가상 명령 크기 계산과 실제 명령으로 확장                  |
| `src/encode.*`, `src/directives.*`, `src/output.*`              | 명령 워드 조립, 데이터 생성, 리틀엔디안 출력              |
| `tests/unit.c`                                                      | 단위 테스트                                               |

## 제출 문서

- [구현 과정 및 오류 수정 기록](docs/PROCESS.md)
- [검증 방법 및 결과](docs/RESULTS.md)
- [AI 사용 범위 및 초기·검증 프롬프트](docs/AI_PROMPTS.md)

구현 과정 문서에는 단계별 설계와 오류를 좁혀 간 방법을, 검증 결과 문서에는 실행 명령과 실제 관찰값을 적었습니다. AI 사용 문서에는 작업 분담과 개발 순서에 따라, Safari의 ChatGPT에서 질문을 보내고 답변을 받은 화면을 담았습니다.

## 파일 구조

프로젝트 루트에서 `tree --dirsfirst --noreport -I 'read_asm|*.bin|실습|RV32IM_구현_가이드.md|RV32IM_축소_구현_계획.md'`로 제출 소스와 문서 구조를 확인했습니다.

```text
.
├── docs
│   ├── evidence
│   │   ├── chatgpt-01-answer.png
│   │   ├── chatgpt-01.png
│   │   ├── chatgpt-02-answer.png
│   │   ├── chatgpt-02.png
│   │   ├── chatgpt-03-answer.png
│   │   ├── chatgpt-03.png
│   │   ├── chatgpt-04-answer.png
│   │   ├── chatgpt-04.png
│   │   ├── chatgpt-05-answer.png
│   │   ├── chatgpt-05.png
│   │   ├── chatgpt-06-answer.png
│   │   ├── chatgpt-06.png
│   │   ├── example1-debug-answer.png
│   │   ├── example1-debug.png
│   │   ├── example4-debug-01.png
│   │   ├── example4-debug-02.png
│   │   ├── example4-debug-03.png
│   │   └── example4-debug-04.png
│   ├── AI_PROMPTS.md
│   ├── PROCESS.md
│   └── RESULTS.md
├── examples
│   ├── expanded
│   │   ├── asm_example1.s
│   │   ├── asm_example2.s
│   │   ├── asm_example3.s
│   │   └── asm_example4.s
│   ├── asm_example1.s
│   ├── asm_example2.s
│   ├── asm_example3.s
│   ├── asm_example4.s
│   └── baseline.s
├── src
│   ├── csr_table.h
│   ├── directive_table.h
│   ├── directives.c
│   ├── directives.h
│   ├── encode.c
│   ├── encode.h
│   ├── format.h
│   ├── format_finder.c
│   ├── instruction_table.h
│   ├── layout.c
│   ├── layout.h
│   ├── operands.h
│   ├── output.c
│   ├── output.h
│   ├── parse.c
│   ├── parse.h
│   ├── pseudo.c
│   ├── pseudo.h
│   ├── pseudo_table.h
│   ├── register_helper.c
│   ├── register_helper.h
│   ├── register_table.h
│   ├── values.c
│   └── values.h
├── tests
│   └── unit.c
├── Makefile
├── README.md
├── first.s
└── main.c
```
