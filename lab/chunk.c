/* ─────────────────────────────────────────────────────────────────────────
 *  lab/chunk.c — 힙 청크 헤더 관찰 실험
 *
 *  챌린지가 아니다. 버그가 없고 정상 종료한다. 고칠 것도 없다.
 *  혼자 실행하면서 읽으라고 쓴 파일이므로 아래 순서를 그대로 따라가면 된다.
 *
 *
 *  ■ 왜 하는가 — x/ 를 써야 할 이유를 못 찾겠다는 질문의 답
 *
 *    print 는 내가 이름 붙인 것만 읽는다. p->field, arr[3], *node 는 전부
 *    타입 시스템 안에 있는 주소다. 타입이 참인 동안에는 print 가 x/ 보다
 *    모든 면에서 낫다. 그 판단은 맞다.
 *
 *    문제는 메모리 버그가 정의상 "이름 없는 자리"를 건드린 사건이라는 것이다.
 *    이름이 없는 자리는 세 군데뿐이다.
 *
 *      1. 객체의 끝 너머       overflow 의 증거가 놓이는 곳.
 *                              char label[24] 의 안쪽은 print 로 충분하다.
 *                              그런데 증거는 label[24] 부터, 즉 배열이 끝난
 *                              자리에 있다. 카나리, 저장된 프레임 포인터,
 *                              복귀 주소. 컴파일러가 이름을 주지 않는다.
 *      2. 객체가 시작되기 전   malloc 이 자기 살림에 쓰는 헤더. 이 파일의 주제.
 *      3. free 된 뒤           타입은 그대로 남아 있는데 내용은 glibc 것으로
 *                              바뀐 상태. 이 파일의 두 번째 주제.
 *
 *    print 는 내가 "의도한" 메모리를 보여주고, x/ 는 내가 "실제로 건드린"
 *    메모리를 보여준다. 버그란 이 둘이 갈라진 상태를 말한다.
 *    그래서 print 만으로는 원리상 볼 수 없는 것이 있다.
 *
 *
 *  ■ 메모리 배치 (64비트, glibc)
 *
 *      p-16 : prev_size   앞 청크가 free 일 때만 의미가 있다.
 *                         아니면 앞 청크 사용자 데이터의 꼬리가 그냥 보인다.
 *      p-8  : size        청크 전체 크기. 16바이트 정렬이라 하위 3비트가 남고
 *                         거기에 플래그가 들어간다.
 *                           bit0 PREV_INUSE    앞 청크가 사용 중인가
 *                           bit1 IS_MMAPPED    mmap 으로 따로 받아온 큰 덩어리인가
 *                           bit2 NON_MAIN_ARENA 메인 아레나가 아닌가
 *      p+0  : ← malloc 이 돌려준 주소. C 의 타입은 여기서부터 시작한다.
 *
 *    free 되어 tcache 에 들어가면 glibc 가 사용자 데이터의 앞 16바이트를
 *    자기 것으로 덮어쓴다. glibc 2.39 malloc/malloc.c 의 정의는 이렇다.
 *
 *        typedef struct tcache_entry {
 *          struct tcache_entry *next;
 *          uintptr_t key;          // This field exists to detect double frees.
 *        } tcache_entry;
 *
 *      p+0  : next   같은 tcache bin 의 다음 청크.
 *                    2.32 부터 safe-linking 이 걸려서 (주소 >> 12) ^ 포인터 로
 *                    뒤섞여 저장된다. 눈으로는 이상하게 작은 수처럼 보인다.
 *      p+8  : key    프로세스가 시작할 때 getrandom 으로 뽑은 난수 하나
 *                    (static uintptr_t tcache_key). 해제된 모든 청크에 같은
 *                    값이 박힌다. free 는 이 값을 보고 double free 를 잡는다.
 *
 *                        if (__glibc_unlikely (e->key == tcache_key))
 *                          ... if (tmp == e)
 *                                malloc_printerr ("free(): double free detected in tcache 2");
 *
 *                    double free 가 SIGABRT 로 죽는 이유는 커널이 잡아서가
 *                    아니라, glibc 가 내 데이터 자리에 심어 둔 지문을 보고
 *                    스스로 프로그램을 죽이기 때문이다.
 *
 *
 *  ■ 실행
 *
 *      make dg_lab_chunk    컨테이너 gdb  ← 반드시 여기서 본다. glibc 2.39 기준.
 *      make g_lab_chunk     호스트 gdb    호스트는 2.43 이라 세부가 다를 수 있다.
 *      make lab_chunk       gdb 없이 그냥 실행
 *
 *
 *  ■ 순서
 *    (줄 번호가 어긋나면 gdb 안에서 list main 으로 확인하고 고쳐 잡는다.)
 *
 *    ── [0] 준비 ────────────────────────────────────────────────────────
 *
 *        b 221         free(a) 직전
 *        run
 *
 *      출력에서 b - a = 32 를 먼저 확인한다.
 *      sizeof(struct rec) 는 24 인데 다음 블록이 32 바이트 뒤에 잡혔다.
 *      요청 24 + size 필드 8 = 32, 16바이트 정렬이라 그대로 32.
 *      그 8바이트 차이가 바로 헤더다.
 *
 *
 *    ── [1] free 전 — 헤더를 본다 ───────────────────────────────────────
 *
 *        x/6gx (char *)a - 16
 *
 *      여섯 워드가 a 의 청크 전체와 b 의 size 필드까지 덮는다.
 *
 *        a-16   0x0000000000000000   prev_size (지금은 의미 없음)
 *        a-8    0x0000000000000021   ← size. 0x20 = 32, 끝의 1 이 PREV_INUSE
 *        a+0    0x4141414141414141   name[0..7]  "AAAAAAAA"
 *        a+8    0x0000000000000000   name[8..15]
 *        a+16   0x0000000000001111   n
 *        a+24   0x0000000000000021   ← 벌써 b 의 size 필드다
 *
 *      * 볼 것 1: 0x21 을 "32 + PREV_INUSE" 로 읽을 수 있는가.
 *      * 볼 것 2: a+24 가 이미 옆 청크라는 것. 내 24바이트 바로 뒤에
 *                 남의 살림이 붙어 있다. heap overflow 가 왜 바로 안 죽고
 *                 나중에 free 에서 죽는지가 여기서 보인다.
 *                 넘쳐 쓴 바이트는 남의 size 필드를 조용히 갈아버린다.
 *
 *
 *    ── [2] free(a) 직후 — 타입이 거짓말을 시작한다 ─────────────────────
 *
 *        next
 *        print *a
 *        x/6gx (char *)a - 16
 *
 *      * 볼 것 1: print *a 가 여전히 멀쩡한 구조체를 찍는다.
 *                 게다가 n 은 0x1111 그대로 살아 있다. glibc 가 앞 16바이트만
 *                 덮었기 때문이다. 절반이 맞는 값이라 오히려 더 속기 쉽다.
 *                 gdb 는 "이 메모리는 이제 네 것이 아니다" 를 말해 주지 않는다.
 *                 타입이 그대로 남아 있기 때문이다.  ← 이것이 3번 영역의 실물
 *      * 볼 것 2: a+0 이 뒤섞인 next 로 바뀌었다. 지금은 bin 이 비어 있어서
 *                 next 가 NULL 이고, (a >> 12) ^ 0 = a >> 12 가 저장된다.
 *                 다음 두 줄의 값이 같아야 한다.
 *
 *                     p/x (unsigned long)a >> 12
 *                     x/gx a
 *
 *      * 볼 것 3: a+8 에 없던 난수가 생겼다. 이것이 key 다. 다음 단계에서
 *                 이 값을 다시 만나게 된다.
 *      * 볼 것 4: size 필드 0x21 은 그대로다. free 는 헤더를 지우지 않는다.
 *                 그래서 해제된 청크도 크기를 알 수 있다.
 *
 *
 *    ── [3] free(b) 직후 — 지문과 연결 리스트 ───────────────────────────
 *
 *        b 225
 *        c
 *        x/2gx b
 *
 *      * 볼 것 1: b+8 의 값이 [2] 에서 본 a+8 의 값과 같다.
 *                 이 난수가 "이 메모리는 이미 해제됐다" 는 지문이다.
 *                 같은 포인터를 두 번 free 하면 glibc 가 이 값을 보고 멈춘다.
 *      * 볼 것 2: b+0 을 풀면 a 의 주소가 나온다.
 *
 *                     p/x ((unsigned long)b >> 12) ^ *(unsigned long *)b
 *
 *                 tcache bin 의 연결 리스트가 b -> a -> NULL 로 이어져 있다.
 *                 나중에 같은 크기를 malloc 하면 b 가 먼저 돌아온다 (LIFO).
 *
 *
 *    ── [4] 내가 만든 명령으로 보기 (선택) ──────────────────────────────
 *
 *        tk -l *(unsigned long *)((char *)a + 8)
 *
 *      -l 은 행을 주소에 고정하므로 프레임을 떠나도 살아남는다.
 *      free(a) 를 넘어가는 순간 이 행에 * 와 old -> new 가 뜬다.
 *      key 가 심어지는 그 순간을 잡는 것이다.
 *
 *
 *  ■ 다음 단계 — chunk PTR 명령 만들기
 *
 *    위를 눈으로 확인한 다음에 만든다. 먼저 만들면 무엇을 찍어야 할지 모른다.
 *    walk.py / deep.py 의 틀을 그대로 쓰면 되고, 분량은 40줄 남짓이다.
 *
 *    찍을 것:
 *      - p-8 의 size 원본 값과, 하위 3비트를 뗀 실제 크기
 *      - PREV_INUSE / IS_MMAPPED / NON_MAIN_ARENA 세 플래그
 *      - 다음 청크의 주소 (p + 실제 크기) 와 그쪽 size 필드
 *      - p+0, p+8 의 원본 값. p+0 은 safe-link 를 푼 값도 함께
 *
 *    제약 하나: 해제 여부를 확정하려면 libc 의 static 변수 tcache_key 를
 *    읽어야 하는데, libc 디버그 심볼이 없으면 심볼이 잡히지 않는다.
 *    실용적으로는 "확실히 해제된 청크의 p+8" 을 한 번 읽어 기준값으로 삼고
 *    그것과 비교하는 방식이 낫다. 처음 본 key 값을 캐싱해 두면 된다.
 *
 *    ▸ 언제 쓰는가 — 크래시 지점과 원인이 멀리 떨어진 버그에서 쓴다.
 *
 *      heap overflow 는 넘쳐 쓰는 순간에 아무 일도 일어나지 않는다.
 *      옆 청크의 size 필드가 조용히 바뀔 뿐이다. 프로그램은 한참 더 돌다가
 *      나중에 free 에서 죽고, bt 를 찍으면 libc 가 나온다.
 *      chunk 가 있으면 의심 가는 단계마다 불변식을 확인해서 그것이 처음
 *      깨진 순간을 찾을 수 있다. 지연된 크래시가 즉시 검사로 바뀐다.
 *
 *        free(): invalid size / corrupted top size
 *            → size 필드에 말이 안 되는 값. 누가 헤더를 넘겨 썼는가
 *        heap overflow (크래시 전에 미리)
 *            → 내 버퍼 "다음" 청크의 size 가 이미 망가졌는가
 *        free(): double free detected in tcache 2
 *            → p+8 에 지문이 있는가. 이미 해제된 메모리인가
 *        use-after-free
 *            → 같은 판정. 이 포인터가 살아 있는 청크를 가리키는가
 *        free(): invalid pointer
 *            → p-8 이 그럴듯한 size 가 아니다. 애초에 청크가 아닌 것
 *        realloc 이후
 *            → 옛 주소의 청크가 해제 상태로 바뀌었는가
 *
 *      결국 chunk 는 질문 하나에만 답하는 도구다.
 *      "이 포인터가 가리키는 메모리는 지금 내 것인가, glibc 것인가."
 *      소유권과 수명 버그가 전부 이 질문이다.
 * ───────────────────────────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>

struct rec {
    char name[16];   /* p+0x00 ~ p+0x0f */
    long n;          /* p+0x10 ~ p+0x17 */
};                   /* 합 24바이트 → 청크는 32바이트가 된다 */

int main(void)
{
    struct rec *a = malloc(sizeof *a);
    struct rec *b = malloc(sizeof *b);
    if (!a || !b) return 1;

    snprintf(a->name, sizeof a->name, "AAAAAAAA");
    a->n = 0x1111;
    snprintf(b->name, sizeof b->name, "BBBBBBBB");
    b->n = 0x2222;

    printf("sizeof(struct rec) = %zu\n", sizeof(struct rec));
    printf("a = %p\n", (void *)a);
    printf("b = %p   (a 와의 차이 = %ld)\n", (void *)b, (char *)b - (char *)a);

    free(a);            /* [1] 이 줄 직전에서 멈춘다. 넘어간 뒤 다시 본다 */

    free(b);            /* [2] 두 번째 해제. next 가 a 를 가리키게 된다 */

    printf("done\n");   /* [3] 여기서 b 의 앞 16바이트를 본다 */
    return 0;
}
