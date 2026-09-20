---
scope: glibc ptmalloc2 chunk layout, sizing, and reuse policy on x86-64
measured: 2026-09-21 — host glibc 2.43 (Ubuntu 26.04) · container memdbg glibc 2.39 (Ubuntu 24.04)
authority: 측정값은 위 두 환경에 한정된다. 다른 allocator(musl, jemalloc, Windows)나 32비트에서는 성립하지 않는다
companion: lab/chunk.c — 같은 구조를 프로그램으로 관찰하는 실험
---

# Heap anatomy — glibc chunk layout and allocation policy

## 0. Scope

C 표준이 보장하는 것은 두 가지뿐이다. 요청한 바이트 수만큼 쓸 수 있다는 것,
그리고 반환 주소가 어떤 타입에도 적합하게 정렬되어 있다는 것이다.
이 문서의 나머지 내용은 전부 glibc `ptmalloc2`의 구현 세부이고, 표준이 아니다.

---

## 1. Chunk layout

`malloc`이 돌려주는 주소는 chunk의 시작이 아니라 payload의 시작이다.
청크는 그보다 16바이트 앞에서 시작한다.

```
                 [ in use ]                      [ freed → tcache ]
              +------------------------+      +------------------------+
chunk +0x00   |  prev_size             |      |  prev_size             |
              |  (앞이 살아있으면      |      |  (그대로)              |
              |   앞 청크의 payload)   |      |                        |
              +------------------------+      +------------------------+
chunk +0x08   |  size = 0x21           |      |  size = 0x21  ★그대로  |
              +========================+      +========================+
mem   +0x00   |  payload [0..7]        |      |  next  ★safe-linking   |
              +------------------------+      +------------------------+
mem   +0x08   |  payload [8..15]       |      |  key   ★tcache 구조체  |
              +========================+      +========================+
mem   +0x10   |  다음 청크 prev_size   |      |  다음 청크 prev_size   |
              |  = payload로 빌려 씀   |      |                        |
              |  (usable 24바이트)     |      |                        |
              +------------------------+      +------------------------+
```

★ 표시는 흔한 오해가 몰리는 자리다. 3절과 8절에서 다룬다.

### Field roles

같은 바이트가 상태에 따라 다른 것을 뜻한다.

| Offset | In use | Freed |
|---|---|---|
| `+0x00` `prev_size` | 앞 청크가 사용 중이면 **앞 청크의 payload** | 앞 청크가 해제됐을 때만 앞 청크의 크기 |
| `+0x08` `size` | 이 청크의 전체 크기(16의 배수) \| 하위 3비트 flag | 작은 청크는 **바뀌지 않는다** |
| payload word 0 | 사용자 데이터 | tcache `next` · fastbin `fd` · small/large `fd` |
| payload word 1 | 사용자 데이터 | tcache `key` · fastbin 미사용 · small/large `bk` |
| payload word 2,3 | 사용자 데이터 | largebin `fd_nextsize`, `bk_nextsize` |

`size`의 하위 3비트는 flag다. 읽을 때는 항상 `& ~7`로 지우고 본다.

```
bit 0  PREV_INUSE       앞 청크가 사용 중인가
bit 1  IS_MMAPPED       mmap으로 따로 받은 청크인가
bit 2  NON_MAIN_ARENA   main arena가 아닌가
```

실질 overhead는 8바이트뿐이다. `prev_size`는 대개 앞 청크가 쓰고 있다.

---

## 2. Size formula

```
chunk  = max(32, align16(요청 + 8))
usable = chunk - 8
```

측정값이다. host 2.43과 container 2.39가 동일했다.

```
malloc(     1) usable=    24     ← 1..24 는 전부 32바이트 청크
malloc(    24) usable=    24
malloc(    25) usable=    40     ← 여기서 48바이트 청크로
malloc(    40) usable=    40
malloc(  1032) usable=  1032
malloc(  1033) usable=  1048
```

### Why the minimum is 32

임의의 숫자가 아니라 두 제약의 교집합이다.

```
제약 A  정렬       x86-64에서 반환 주소는 16의 배수여야 한다
                   (long double, SSE 레지스터의 요구)

제약 B  관리 비용  해제된 청크는 자유 목록에 매달려야 하므로
                   자기 안에 연결 포인터를 담을 수 있어야 한다

        prev_size(8) + size(8) + fd(8) + bk(8) = 32  ← MINSIZE
```

살아 있는 블록도 언젠가 해제되므로 그보다 작게 만들 수 없다.

---

## 3. Allocation path

힙이 자라는 것은 마지막 수단이다. 앞 단계에서 맞는 것이 나오면 힙은 커지지 않는다.

```
malloc(n)
  │
  ├─ tcache        스레드 전용 캐시. 청크 1040바이트까지, 16바이트 간격
  │                64개 계급, 계급당 7개. 잠금 없이 즉시 반환
  ├─ fastbin       작은 청크의 단일 연결 목록. 병합하지 않음
  ├─ smallbin      크기가 정확히 일치하는 목록
  ├─ unsorted bin  최근 해제분의 대기열. 여기서 분류가 일어남
  ├─ largebin      크기 범위별 목록. 큰 청크를 쪼개서 씀
  ├─ top chunk     힙 맨 끝의 남은 덩어리. 앞쪽을 잘라 씀
  └─ sysmalloc     여기서 처음으로 커널에 요청한다
```

### Reuse policy per list

```
tcache    LIFO        머리에 넣고 머리에서 뺀다. 가장 최근 것
fastbin   LIFO        같은 방식. 단일 연결이라 fd만 쓴다
smallbin  FIFO        머리에 넣고 꼬리에서 뺀다. 가장 오래된 것
largebin  size-first  순서가 아니라 가장 잘 맞는 것을 고르고 남으면 쪼갠다
```

LIFO가 가리키는 순서는 **메모리상의 위치가 아니라 목록에 들어간 시각**이다.

```
할당   a=...ea010  b=...ea030  c=...ea050
해제   a, b, c
재할당 x=...ea050  y=...ea030  z=...ea010     ← c, b, a 순
```

---

## 4. The heap is not a stack

구멍이 뚫리고 다시 메워지며, 이웃끼리 합쳐진다.

```
가운데 구멍 재사용
  [ p 사용중 ][  구멍  ][ r 사용중 ]  →  [ p ][ 새 블록 ][ r ]
  free(q) 후 malloc(48) 이 q와 같은 주소를 돌려줬다

인접 자유 청크 병합
  [ m1 자유 ][ m2 자유 ]  →  [ 4000짜리 하나 ]
  2000+2000 해제 후 malloc(4000) 이 m1과 같은 주소를 돌려줬다

top chunk 에서 잘라 쓰기
  [ 사용중 ][ 사용중 ][ 구멍 ][ 사용중 ][ ====== top chunk ====== ]
                                        ↑ 앞쪽을 잘라 쓰고 경계가 오른쪽으로 밀린다
```

앞쪽에 살아 있는 청크가 하나라도 있으면 뒤쪽 구멍을 커널에 돌려줄 수 없다.
`brk`는 경계를 내리는 방식이라 중간만 오려낼 수가 없다.

---

## 5. Heap growth

요청 크기가 갈림길이다.

```
요청 < mmap_threshold (기본 128 KiB)
    → main arena. brk로 힙 경계를 위로 늘린다
    → 붙어서 자라므로 앞이 막히면 줄일 수 없다

요청 >= mmap_threshold
    → 독립된 mmap 영역을 따로 받는다
    → 페이지 단위로 올림되고, free 하면 munmap 으로 즉시 반환
```

`brk`는 요청한 만큼만 늘어나지 않는다. 첫 `malloc(8)` 한 번에 이만큼 늘었다.

```
brk 증가 = 135168 bytes (0x21000) = 132 KiB
         = M_TOP_PAD 128 KiB + page 4 KiB
```

1 MiB를 요청했을 때 `brk` 증가량은 두 환경 모두 **0**이었다. mmap으로 갔다는 뜻이다.

### Version divergence — the one number that differed

정확히 `malloc(131072)`에서 두 환경이 갈렸다.

```
host  glibc 2.43   usable=135144   → mmap 으로 갔다
cont  glibc 2.39   usable=131080   → heap 에서 잘라 줬다
```

임계값 비교가 `>`인지 `>=`인지가 버전에 따라 다르다.
경계값을 정확한 상수로 외우지 말고 "기본 128 KiB 근처"로 기억한다.

---

## 6. Principles by evidence grade

### Grade 1 — 이 두 환경에서 직접 측정함

| Principle | Evidence |
|---|---|
| 반환 주소는 항상 16의 배수 | 모든 크기에서 `align16=1` |
| 청크는 32바이트보다 작을 수 없다 | `malloc(1)`의 usable이 24 |
| `usable = chunk - 8` | 24, 40, 1032, 1048 |
| `chunk = max(32, align16(요청+8))` | 24→25 경계에서 32→48 |
| tcache는 LIFO | `a,b,c` 해제 후 `c,b,a` 반환 |
| 가운데 구멍도 그대로 재사용 | `free(q)` 후 같은 주소 |
| 인접 자유 청크는 병합 | 2000+2000 해제 후 4000이 들어감 |
| 작은 청크는 해제해도 `size`가 그대로 | 전후 모두 `0x21` |
| 첫 `malloc`이 brk를 132 KiB 늘린다 | `0x21000` |
| 큰 요청은 brk를 늘리지 않는다 | 1 MiB 요청 시 증가 0 |

### Grade 2 — 구현을 근거로 말하지만 여기서 측정하지 않음

bin별 정책(smallbin FIFO, largebin size-first), safe-linking의 계산 방식,
flag 비트의 의미, tcache 계급 수(64)와 계급당 개수(7)가 여기 속한다.

### Grade 3 — 절대 가정하면 안 되는 것

```
✗ 다음 malloc 이 어떤 주소를 줄지
✗ free 후에도 데이터가 남아 있으리라는 것   → 최소 두 워드는 확실히 덮어써진다
✗ 해제 여부를 헤더만 보고 판정할 수 있다는 것 → 작은 청크는 size 가 그대로다
✗ 할당 순서와 주소 순서가 같다는 것
✗ free() 가 메모리를 OS 에 돌려준다는 것    → 대개 안 돌려준다. RSS 는 그대로다
✗ usable 초과분을 써도 된다는 것            → 요청한 만큼만 쓴다
✗ 다른 allocator, 다른 아키텍처에서도 같다는 것
```

---

## 7. Debugging corollaries

**Heap overflow 는 쓰는 순간에 죽지 않는다.**
32바이트 청크에서 payload 시작 기준 16바이트 뒤는 다음 청크의 `prev_size`라
아직 안전하지만, 24바이트 뒤는 다음 청크의 `size`다. 그래서 범인과 현장이
시간적으로 떨어진다. 한참 뒤 그 이웃을 `malloc`하거나 `free`할 때
`malloc(): invalid size` 같은 메시지로 터진다.

**Use-after-free 가 잘 동작하는 것처럼 보인다.**
tcache가 LIFO라서 해제 직후 같은 계급을 요청하면 그 주소가 그대로 돌아온다.
입력이 조금만 바뀌면 무너진다.

**해제 판정은 한 방향으로만 성립한다.**
`free` 직후 payload 첫 두 워드에 사용자 데이터처럼 보이지 않는 포인터 값이
있다면 해제된 블록이라는 단서가 된다. 반대는 성립하지 않는다.
데이터가 멀쩡해 보여도 살아 있다는 뜻이 아니다.

---

## 8. Reading the heap in gdb

### A byte map

`Rec { int id; char *name; }` 세 개 중 첫 번째와 그 이름 문자열이다.
`malloc`은 언제나 payload의 주소를 돌려준다. 헤더는 그 앞에 그냥 놓여 있다.

```
 주소               raw bytes (little-endian)      무엇인가
 ----------------   -------------------------      ------------------------------
 0x555555559000     00 00 00 00 00 00 00 00        prev_size   ┐
 0x555555559008     21 00 00 00 00 00 00 00        size = 0x21 ┘ chunk #1 헤더
 0x555555559010     00 00 00 00  00 00 00 00       id = 0 + 패딩   ← malloc 반환 ★
 0x555555559018     30 90 55 55 55 55 00 00        name = 0x555555559030
 ----------------                                  chunk #1 끝 (0x20 = 32바이트)
 0x555555559020     00 00 00 00 00 00 00 00        prev_size   ┐
 0x555555559028     21 00 00 00 00 00 00 00        size = 0x21 ┘ chunk #2 헤더
 0x555555559030     61 6c 69 63 65 00 00 00        "alice\0"       ← malloc 반환 ★
 0x555555559038     00 00 00 00 00 00 00 00        남은 payload
 ----------------                                  chunk #2 끝
```

★ 두 자리가 `malloc`이 돌려준 주소다. 둘 다 끝자리가 `0`이다.
`size`의 `0x21`이 첫 바이트에 있는 이유는 little-endian이기 때문이다.

### What `- 8` actually computes

**포인터 산술은 가리키는 타입의 크기로 곱해진다.** 이것을 놓치면 엉뚱한 곳을 본다.

```
r[0]                        = 0x555555559010    payload
(char *)r[0] - 8            = 0x555555559008    size 필드      ← 8 × 1바이트
(unsigned long *)r[0] - 1   = 0x555555559008    같은 자리      ← 1 × 8바이트
r[0] - 8                    = 0x555555558f90    ✗ 128바이트 뒤 ← 8 × sizeof(Rec)
```

`r[0]`은 `Rec *`이므로 `- 8`은 `8 × 16 = 128`바이트를 물러난다.
헤더를 보려면 **먼저 캐스팅**해서 걸음 크기를 1바이트나 8바이트로 바꿔야 한다.

### Command per position

```
 0x555555559008  size          x/1xg (char *)r[0] - 8
                               p/x *((unsigned long *)r[0] - 1)
                               p (*((unsigned long *)r[0] - 1)) & ~7      플래그 제거
 0x555555559000  헤더 두 워드   x/2xg (char *)r[0] - 16
 0x555555559010  struct 전체    p *r[0]
 0x555555559010  멤버 하나      p r[0]->id
 0x555555559018  멤버의 주소    p &r[0]->name
 0x555555559030  문자열         x/s r[0]->name       p r[0]->name
 0x555555559030  문자열의 헤더  x/1xg (char *)r[0]->name - 8
 payload 두 워드                x/2xg r[0]           해제됐다면 next, key
```

`x`와 `p`는 볼 수 있는 범위가 같다. 다른 것은 해석 방식이다.
`x/FMT ADDR`은 타입을 무시하고 주소에서부터 형식대로 찍는다.
`p EXPR`은 C 식으로 평가하므로 `& ~7` 같은 연산, 함수 호출, `$N` 이력이 된다.

프로그램 자신도 같은 범위를 읽는다. 디버거의 특권이 아니라 포인터 산술이다.
payload와 헤더는 같은 페이지에 같은 권한으로 나란히 있고, 그 사실이 바로
heap overflow가 allocator를 망가뜨릴 수 있는 이유다.

### hwalk — walk the heap from one pointer

포인터 하나만 있으면 변수로 잡히지 않는 블록까지 전부 훑는다.
`size`가 16의 배수가 아니면 손상으로 보고 멈춘다.

```gdb
define hwalk
  set $p = (unsigned long *)$arg0
  set $i = 0
  while $i < $arg1
    set $sz = *($p - 1) & ~7
    if $sz % 16 != 0 || $sz < 32
      printf "%2d  payload=%p  size=0x%lx  <-- 손상\n", $i, $p, $sz
      loop_break
    end
    printf "%2d  payload=%p  size=0x%-4lx  w0=%016lx  w1=%016lx\n", $i, $p, $sz, $p[0], $p[1]
    set $p = (unsigned long *)((char *)$p + $sz)
    set $i = $i + 1
  end
end
```

`Rec` 세 개를 만들고 가운데 것만 해제한 뒤 `hwalk r[0] 6`을 돌린 결과다.

```
 0  payload=0x555555559010  size=0x20   w0=0000000000000000  w1=0000555555559030
 1  payload=0x555555559030  size=0x20   w0=0000006563696c61  w1=0000000000000000
 2  payload=0x555555559050  size=0x20   w0=0000000555555559  w1=601c561418d6d7c5
 3  payload=0x555555559070  size=0x20   w0=0000000000626f62  w1=0000000000000000
 4  payload=0x555555559090  size=0x20   w0=0000000000000002  w1=00005555555590b0
 5  payload=0x5555555590b0  size=0x20   w0=0000006c6f726163  w1=0000000000000000

 0  Rec[0]   w0 = id,  w1 = name 포인터
 1  "alice"
 2  Rec[1]   ★해제됨. w1 이 tcache key
 3  "bob"
 4  Rec[2]
 5  "carol"
```

### Six uses of the header

1. **헤더를 watch로 걸어 범행 순간을 잡는다.** overflow는 쓰는 순간에 죽지 않으므로
   `bt`에는 범인이 없다. `watch *(unsigned long *)((char *)p + 24)` 또는
   `tk -l /4xg (char *)p - 16`으로 덮어쓰는 순간에 멈춘다.
2. **오염 판정.** `size`는 항상 16의 배수다. `0x4141414141414141`이 보이면
   앞 블록이 넘쳤다.
3. **포인터 없이 힙 순회.** 다음 payload = 현재 payload + `(size & ~7)`.
4. **해제 판정.** 여러 블록의 payload 두 번째 워드가 전부 같은 값이면 tcache key다.
   그 블록들은 전부 해제됐다. 역은 성립하지 않는다.
5. **safe-linking 역산.** `진짜 주소 = 저장된 값 XOR (payload 주소 >> 12)`.
   `p/x *(unsigned long *)p ^ ((unsigned long)p >> 12)`로 tcache 목록을 따라간다.
6. **경계 확인.** `p malloc_usable_size(p)`로 여유분을, `info proc mappings`로
   brk 힙인지 별도 mmap인지 본다.

### The `each` family

배열이 가리키는 struct들을 한 번에 펼칠 때 쓴다
(문법은 `~/.config/gdb/each.py`의 docstring).

```
(gdb) each d->by_id[0..3]              슬롯에 담긴 주소들
(gdb) each d->by_id[0..3]->*           각 struct 의 모든 멤버
(gdb) tk each d->by_id[0..3]->*        vars 창에 고정해 변화를 추적
```

정수 리터럴을 그냥 역참조하면 gdb는 `int *`로 간주해 앞 4바이트만 읽는다.
`p *(Rec *)0x5555555592a0`처럼 캐스팅한다.

---

## 9. Reproduce

```c
/* gcc -O0 -o probe probe.c && ./probe */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <gnu/libc-version.h>

int main(void) {
    printf("glibc %s\n", gnu_get_libc_version());

    size_t s[] = {1, 24, 25, 40, 1032, 1033, 131000, 131072};
    for (unsigned i = 0; i < 8; i++) {
        void *p = malloc(s[i]);
        printf("  malloc(%6zu) usable=%6zu align16=%d\n",
               s[i], malloc_usable_size(p), ((unsigned long)p & 15) == 0);
    }

    void *a = malloc(24), *b = malloc(24), *c = malloc(24);
    unsigned long *ha = (unsigned long *)a - 1;
    unsigned long *hb = (unsigned long *)b - 1;
    printf("  a.size=0x%lx  gap(a,b)=%ld\n", *ha, (char *)b - (char *)a);

    free(a); free(b); free(c);
    printf("  free 3 -> a.size=0x%lx b.size=0x%lx\n", *ha, *hb);

    void *x = malloc(24), *y = malloc(24), *z = malloc(24);
    printf("  LIFO? x==c:%d y==b:%d z==a:%d\n", x == c, y == b, z == a);

    void *before = sbrk(0);
    void *q = malloc(1 << 20);
    printf("  1MiB 요청 후 brk 증가=%ld\n", (char *)sbrk(0) - (char *)before);
    (void)q;
    return 0;
}
```

컨테이너에서 같은 프로그램을 돌려 비교한다.

```
docker run --rm -v "$PWD":/w -w /w memdbg bash -c "gcc -O0 -o probe probe.c && ./probe"
```

---

## 10. Drill — header-first debugging

버그를 **찾는** 훈련이 아니라, 헤더만 보고 힙의 상태를 **읽는** 훈련이다.
버그의 정체는 미리 듣고 시작한다. 연마 대상은 절차다.

### Targets

heap 오버플로 계열 세 문제. 세 번 반복해 같은 절차를 몸에 붙인다.

```
03_heap_buffer_overflow     heap 할당 15곳
09_strcpy_overflow          heap 할당 3곳
19_realloc_shrink_overflow  heap 할당 6곳
```

`16_unused_cap_overflow`는 `char rec[24]` 스택 배열이라 청크 헤더가 없다.
이 절차의 대상이 아니다.

### Setup

```
(gdb) source lab/hwalk.gdb

hsize  P        한 블록의 size, usable, flags
hwalk  P N      P 에서 N 블록을 걷는다. 체인이 깨지면 멈춘다
hwatch P        P 다음 청크의 size 필드에 watchpoint
```

### The five steps

1. **기준선.** 할당이 끝나고 손상 전인 지점에 breakpoint 를 걸고
   `hwalk` 로 정상 체인을 찍어 둔다. 블록 개수와 크기 무늬를 기억한다.
2. **끊긴 자리.** 의심 구간을 지난 뒤 다시 `hwalk`. 손상 메시지가 나온
   블록의 **바로 앞 블록**이 넘친 범인이다.
3. **범행 순간.** 그 앞 블록의 payload 주소로 `hwatch` 를 걸고 `rerun`.
   헤더를 덮어쓰는 그 줄에서 멈춘다.
4. **용량 대조.** `hsize` 의 usable 과 코드가 쓰려던 바이트 수를 비교한다.
   요청한 크기가 아니라 usable 이 경계다.
5. **대조 확인.** 멈춘 프레임이 미리 들은 버그와 일치하는지 본다.
   어긋나면 1번의 기준선이 이미 손상된 상태였을 가능성을 먼저 의심한다.

### What this drill is not

코드 분석이 아니다. `bug.c` 를 읽어 논리를 추적하는 대신, 메모리만 보고
누가 누구를 침범했는지 특정한다. 두 접근이 같은 답에 도달하는지 확인하는 것이
이 훈련의 채점 기준이다.
