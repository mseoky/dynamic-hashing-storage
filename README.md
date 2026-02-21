# Dynamic Hashing Storage

## 프로젝트 개요
이 저장소는 **2025년도 1학기 파일구조(File Structures) 수업 과제** 아카이브입니다.  
과제 목표는 제한된 메모리(4KB 블록 3개) 환경에서 `student`/`professor` 레코드를 **binary 파일**로 저장하고, 삽입/검색/조인을 빠르게 수행하는 저장 구조를 구현하는 것이었습니다.

- 과제 명세: `assignment-3.pdf`
- 최초 제공 스켈레톤: `src/main.cpp`
- 최종 제출 솔루션: `solution.cpp`

## 문제 정의와 설계 방향
과제는 단순 기능 구현보다, 다음 제약을 만족하면서 성능까지 확보하는 것이 핵심이었습니다.

- 레코드는 블록 경계를 넘지 않아야 함
- 매 함수 호출 시 파일 기반 처리(메모리 선적재 금지)
- 사용 가능한 메인 메모리는 사실상 4KB 블록 3개 + 스칼라 변수
- `insert`, `read by rank`, `search by ID`, `join` 인터페이스 고정

이 제약 때문에 정적 배열/선형 스캔 위주의 구현은 데이터가 커질수록 I/O 병목이 커집니다.  
그래서 솔루션에서는 **Extensible Dynamic Hashing(확장 해시)** 를 중심으로 파일 구조를 설계했습니다.

## 구현 요약 (solution.cpp)
### 1) 블록 중심 물리 설계
- 블록 크기: `BLOCK_SIZE = 4096`
- 레코드 구조체를 `#pragma pack(push, 1)`로 패킹해 저장 포맷을 명확히 고정
- 각 버킷 블록 헤더에 `localDepth`, `count`를 저장
- 고정 3개 블록 버퍼만 사용
  - `studentBlock`
  - `professorBlock`
  - `multipurposeBlock` (디렉터리/공용 처리)

핵심 의도는 “레코드 단위”가 아니라 “블록 단위 I/O”로 디스크 접근을 최소화하는 것입니다.

### 2) Extensible Hash 인덱스 3종 운용
이 구현은 해시를 두 테이블에만 쓰지 않고, 조인 최적화를 위해 보조 인덱스를 추가한 점이 특징입니다.

- `student.dat` + `studDir.dat`
  - 키: `Student.ID`
  - 목적: `searchStudent(ID)` 가속
- `professor.dat` + `profDir.dat`
  - 키: `Professor.ID`
  - 목적: 교수 레코드 삽입/탐색
- `advisor.dat` + `advisorDir.dat`
  - 키: `advisorID`, 값: 지도학생 수(`count`)
  - 목적: `join(..., n)` 가속

즉, 학생/교수 원본 저장 + 조인용 집계 인덱스를 분리해 유지합니다.

### 3) 버킷 분할과 디렉터리 확장
`insertStudent`, `insertProfessor`, `insertAdvisor`(내부 업데이트)에서 공통적으로:

- 버킷 여유가 있으면 바로 삽입
- 버킷이 가득 찼고 `localDepth < globalDepth`면 버킷 split
- `localDepth == globalDepth`면 디렉터리 2배 확장 후 split
- 기존 레코드는 새 해시 비트 기준으로 재분배

이 흐름으로 데이터 증가 시에도 평균적으로 짧은 탐색 경로를 유지합니다.

### 4) 함수별 전략
- `insertProfessor` / `insertStudent`
  - 해시 기반 버킷 접근
  - 중복 ID 검사
  - 필요 시 split + 재귀 재시도
- `searchStudent`
  - ID 해시로 대상 버킷 1개만 읽고 탐색
- `readStudent(rank)`
  - 학생 삽입 순서를 `StudentRecord.n`으로 별도 저장
  - rank 인덱스 전용 구조를 두지 않고 버킷 순회로 n 매칭
- `join(pName, sName, n)`
  - 1단계: `advisor.dat`에서 `count == n`인 `advisorID` 후보 탐색 후 최소 교수 ID 선택
  - 2단계: 해당 교수 ID로 교수명 조회
  - 3단계: 학생 버킷 순회로 해당 교수가 지도하는 학생 중 최소 학생 ID 선택

## 성능 최적화 포인트
### 파일 I/O 관점
- 레코드별 파일 접근 대신 블록 단위 읽기/쓰기
- 버킷 헤더(`depth`, `count`)를 함께 저장해 추가 메타데이터 접근 최소화
- split 시 필요한 버킷만 재배치

### 자료구조 관점
- ID 검색은 해시 인덱스로 평균 O(1)에 가까운 접근
- 조인을 위해 `advisorID -> count`를 **삽입 시점에 유지**하여 join 시 재집계를 피함

### 메모리 제약 대응
- 대용량 컨테이너를 메모리에 유지하지 않고, 블록 버퍼 3개 내에서 작업
- 과제의 “함수 호출 때마다 파일 기반 처리” 조건 충족

## Nested Loop Join 대비 효율성
기본 Nested Loop Join(교수 P명, 학생 S명)은 조건 비교가 보통 `O(P*S)`까지 증가합니다.  
이 솔루션은 조인 전용 보조 인덱스(`advisor.dat`)를 유지해 다음처럼 단계를 분해합니다.

- `count == n` 교수 후보 찾기: advisor 버킷 순회 (`O(P)`에 가까움)
- 최소 교수 ID 확정 후 교수명 조회: 해시 버킷 탐색 (평균 O(1))
- 해당 교수의 최소 학생 ID 찾기: 학생 전체 1회 스캔 (`O(S)`)

결과적으로 조인 비용을 실무적으로 **`O(P + S)` 성격**으로 낮춰, 데이터가 커질수록 Nested Loop 대비 확실한 성능 이점을 확보합니다.

## 구현 시 기술적 고민
- 과제 제약상 메모리 인덱스를 크게 둘 수 없어, on-disk 인덱스 구조를 설계해야 했음
- `readStudent(rank)`는 rank 직접 인덱스 없이도 요구 인터페이스를 만족해야 해서, 삽입 시 rank 값을 레코드에 내장하는 방식으로 해결
- 조인에서 "최소 Prof.ID, 최소 Student.ID" tie-break 규칙을 정확히 맞추기 위해 후보 집합 탐색 순서와 비교 조건을 분리

## 디렉터리 구조
```text
.
├─ assignment-3.pdf
├─ solution.cpp
├─ README.md
├─ professor.txt
├─ student.txt
├─ IDQuery.txt
├─ rankQuery.txt
└─ src/
   ├─ main.cpp          # 최초 제공 스켈레톤
   ├─ professor.txt     # 원본 제공 데이터
   ├─ student.txt
   ├─ IDQuery.txt
   └─ rankQuery.txt
```

정리 판단:
- `src/`는 "초기 제공 파일" 보존을 위해 유지
- 루트에 입력 파일 4개를 배치해, 클론 직후 `solution.cpp`를 바로 실행 가능하도록 구성
- 코드 수정 없이 실행 편의성만 개선

## 실행 방법
### 1) 컴파일 (C++11)
```bash
g++ -std=c++11 -O2 solution.cpp -o solution
```

### 2) 실행
```bash
./solution
```

Windows(MinGW) 예시:
```powershell
g++ -std=c++11 -O2 solution.cpp -o solution.exe
.\solution.exe
```

실행 후 `Execution time: ...` 이 출력되면 정상입니다.

## 비고
이 저장소는 수업 과제 아카이브 목적이며, 제출 당시 인터페이스 요구사항과 파일 기반 처리 제약을 중심으로 구현되었습니다.
