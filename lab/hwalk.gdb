# hwalk.gdb — heap 을 헤더만 보고 걷는다
#
#   (gdb) source lab/hwalk.gdb
#   (gdb) hwalk <payload 주소> <몇 블록>
#   (gdb) hsize <payload 주소>
#   (gdb) hwatch <payload 주소>
#
# 배경과 바이트 지도: lab/heap-anatomy.md

# 한 블록의 size 필드. 플래그를 지운 실제 크기를 $_hsize 에 남긴다.
define hsize
  set $_hsize = *((unsigned long *)$arg0 - 1) & ~7
  printf "payload=%p  chunk=0x%lx (%ld bytes)  usable=%ld  flags=%ld\n", \
    (void *)$arg0, $_hsize, $_hsize, $_hsize - 8, \
    *((unsigned long *)$arg0 - 1) & 7
end

# payload 주소 하나에서 시작해 N 블록을 걷는다.
# size 가 16의 배수가 아니거나 32 미만이면 손상으로 보고 멈춘다.
define hwalk
  set $p = (unsigned long *)$arg0
  set $i = 0
  while $i < $arg1
    set $sz = *($p - 1) & ~7
    if $sz % 16 != 0 || $sz < 32
      printf "%2d  payload=%p  size=0x%lx  <-- 손상. 앞 블록이 넘쳤다\n", $i, $p, $sz
      loop_break
    end
    printf "%2d  payload=%p  size=0x%-5lx  w0=%016lx  w1=%016lx\n", \
      $i, $p, $sz, $p[0], $p[1]
    set $p = (unsigned long *)((char *)$p + $sz)
    set $i = $i + 1
  end
end

# 이 블록 바로 뒤에 있는 다음 청크의 size 필드를 감시한다.
# overflow 가 헤더를 깨뜨리는 그 순간에 멈춘다.
define hwatch
  set $_hw = (char *)$arg0 + (*((unsigned long *)$arg0 - 1) & ~7) - 8
  printf "watch %p  (다음 청크의 size)\n", $_hw
  # eval 로 주소를 리터럴로 굳힌다. $_hw 를 그대로 watch 하면
  # 나중에 $_hw 가 바뀔 때 watchpoint 가 따라 움직인다.
  eval "watch *(unsigned long *)%p", $_hw
end
