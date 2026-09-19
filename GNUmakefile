# ─────────────────────────────────────────────────────────────
#  week05 로컬 편의 계층
#
#  GNU make 는 Makefile 보다 GNUmakefile 을 먼저 읽는다. 이 파일은 upstream 의
#  Makefile 을 그대로 include 하고 그 위에 두 가지를 얹는다.
#    1. 4주차와 같은 짧은 이름표  (06_null_deref / r_… / g_…)
#    2. 컨테이너에서 돌리는 타깃  (dcheck / dr_… / dg_…)
#  upstream 파일은 손대지 않는다. git pull upstream master 가 충돌 없이 지나가도록.
#
#  아래 CH 는 손으로 적어 둔 목록이다. wildcard 로 계산하면 zsh 탭 완성이
#  이름을 읽지 못한다. 챌린지가 추가되면 이 목록도 함께 고친다.
# ─────────────────────────────────────────────────────────────
include Makefile

CH := 01_use_after_free 02_stack_buffer_overflow 03_heap_buffer_overflow 04_double_free 05_null_return_deref 06_null_deref 07_stack_use_after_return 08_uninitialized_read 09_strcpy_overflow 10_realloc_dangling 11_global_overflow 12_free_non_heap 13_linked_list_uaf 14_integer_overflow_alloc 15_dangling_in_struct 16_unused_cap_overflow 17_ownership_uaf 18_cleanup_double_free 19_realloc_shrink_overflow 20_vector_stale_pointer

RCH  := r_01_use_after_free r_02_stack_buffer_overflow r_03_heap_buffer_overflow r_04_double_free r_05_null_return_deref r_06_null_deref r_07_stack_use_after_return r_08_uninitialized_read r_09_strcpy_overflow r_10_realloc_dangling r_11_global_overflow r_12_free_non_heap r_13_linked_list_uaf r_14_integer_overflow_alloc r_15_dangling_in_struct r_16_unused_cap_overflow r_17_ownership_uaf r_18_cleanup_double_free r_19_realloc_shrink_overflow r_20_vector_stale_pointer
GCH  := g_01_use_after_free g_02_stack_buffer_overflow g_03_heap_buffer_overflow g_04_double_free g_05_null_return_deref g_06_null_deref g_07_stack_use_after_return g_08_uninitialized_read g_09_strcpy_overflow g_10_realloc_dangling g_11_global_overflow g_12_free_non_heap g_13_linked_list_uaf g_14_integer_overflow_alloc g_15_dangling_in_struct g_16_unused_cap_overflow g_17_ownership_uaf g_18_cleanup_double_free g_19_realloc_shrink_overflow g_20_vector_stale_pointer
DRCH := dr_01_use_after_free dr_02_stack_buffer_overflow dr_03_heap_buffer_overflow dr_04_double_free dr_05_null_return_deref dr_06_null_deref dr_07_stack_use_after_return dr_08_uninitialized_read dr_09_strcpy_overflow dr_10_realloc_dangling dr_11_global_overflow dr_12_free_non_heap dr_13_linked_list_uaf dr_14_integer_overflow_alloc dr_15_dangling_in_struct dr_16_unused_cap_overflow dr_17_ownership_uaf dr_18_cleanup_double_free dr_19_realloc_shrink_overflow dr_20_vector_stale_pointer
DGCH := dg_01_use_after_free dg_02_stack_buffer_overflow dg_03_heap_buffer_overflow dg_04_double_free dg_05_null_return_deref dg_06_null_deref dg_07_stack_use_after_return dg_08_uninitialized_read dg_09_strcpy_overflow dg_10_realloc_dangling dg_11_global_overflow dg_12_free_non_heap dg_13_linked_list_uaf dg_14_integer_overflow_alloc dg_15_dangling_in_struct dg_16_unused_cap_overflow dg_17_ownership_uaf dg_18_cleanup_double_free dg_19_realloc_shrink_overflow dg_20_vector_stale_pointer

# ─── lab: 개념 실험용 (챌린지가 아니다) ───
# challenges/ 는 upstream 이 관리한다. 손으로 만든 실험 파일은 lab/ 에 둔다.
# 타입이 없는 자리(청크 헤더, 배열의 끝 너머)를 x/ 로 확인하는 용도다.
#   make lab_chunk     호스트 빌드 + 실행
#   make g_lab_chunk   호스트 gdb
#   make dg_lab_chunk  컨테이너 gdb  ← glibc 2.39. 청크 관찰은 여기서 한다
LAB    := chunk
LABR   := $(addprefix lab_,$(LAB))
LABG   := $(addprefix g_lab_,$(LAB))
LABDG  := $(addprefix dg_lab_,$(LAB))

# stem 이 더 짧아서 upstream 의 $(BUILD)/% 규칙보다 이쪽이 먼저 잡힌다.
$(BUILD)/lab_%: lab/%.c | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(DBG) $< -o $@

$(LABR): lab_%: $(BUILD)/lab_%
	@./$< || echo "   (종료 코드 $$?)"

$(LABG): g_lab_%: $(BUILD)/lab_%
	gdb ./$<

$(LABDG): dg_lab_%: | build-docker
	$(DOCKER) make g_lab_$*

# ─── ASan/UBSan 모드 ───
#   make asan 02_stack_buffer_overflow      빌드 + 실행 → 새니타이저 리포트
#   make asan g_02_stack_buffer_overflow    빌드 + gdb
#
# asan 을 골(goal) 목록에 끼워 넣으면 뒤따르는 이름표가 build-asan/ 쪽으로 넘어간다.
# 5단계(수정·검증)를 마친 뒤에 쓰는 도구다. 먼저 켜면 ASan 이 크래시한 줄을 바로
# 알려주는 바람에, 크래시를 gdb 로 거꾸로 읽어 원인까지 가는 연습이 사라진다.
#
# 산출물을 build/ 와 나누는 이유: 같은 경로에 쓰면 소스가 그대로일 때 make 가
# "이미 최신"으로 판단해 다시 컴파일하지 않는다. 플래그만 바뀐 것은 알아채지 못한다.
# 컨테이너 타깃(dr_/dg_)에는 적용되지 않는다. 호스트 전용이다.
ASAN_DIR   := build-asan
ASAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all

ifeq (,$(filter asan,$(MAKECMDGOALS)))
OUT := $(BUILD)
else
OUT := $(ASAN_DIR)
endif

$(ASAN_DIR):
	@mkdir -p $@

$(ASAN_DIR)/%: challenges/%/bug.c | $(ASAN_DIR)
	$(CC) $(CSTD) $(WARN) $(DBG) $(ASAN_FLAGS) $< -o $@

asan:
	@[ $(words $(MAKECMDGOALS)) -gt 1 ] || { \
	  echo "사용법: make asan <이름>       빌드 + 실행 (리포트 확인)"; \
	  echo "        make asan g_<이름>     빌드 + gdb"; \
	  echo "  예)   make asan 02_stack_buffer_overflow"; }

aclean:
	rm -rf $(ASAN_DIR)

clean: aclean

# ─── 호스트 (gcc 15 / glibc 2.43 / gdb 17 + ~/.gdbinit) ───
# 20개 중 18개는 컨테이너와 같은 신호로 죽는다. 다른 두 개는 아래 dr_/dg_ 로.
#   10_realloc_dangling  호스트에서는 크래시하지 않는다 (glibc 2.43 이 double free 를 못 잡음)
#   17_ownership_uaf     호스트 SIGABRT / 컨테이너 SIGSEGV
ifeq (,$(filter asan,$(MAKECMDGOALS)))
$(CH): %: $(BUILD)/%
	@echo "→ $(BUILD)/$@"
else
# asan 모드에서 이름표만 적으면 바로 실행한다. 리포트를 보는 것이 목적이므로.
$(CH): %: $(ASAN_DIR)/%
	@./$< || echo "   (종료 코드 $$?)"
endif

$(RCH): r_%: $(OUT)/%
	@./$< || echo "   (종료 코드 $$?)"

$(GCH): g_%: $(OUT)/%
	gdb ./$<

# ─── 컨테이너 (ubuntu 24.04 / gcc 13 / glibc 2.39 / gdb 15 — 검증 기준 환경) ───
# --user       : 컨테이너가 쓴 파일이 호스트에서 root 소유로 남지 않게
# build-docker : 컨테이너의 /work/build 위에 덧씌운다. 호스트 build/ 와 섞이지 않고,
#                scripts/check.sh 가 고정한 build 경로도 그대로 동작한다.
# HOME=/tmp    : uid 1000 은 컨테이너 passwd 에 없어 HOME 이 비어 gdb 가 경고한다.
# ~/.gdbinit(ro), ~/.config/gdb(rw: 히스토리가 그 안에 있다) : 호스트 gdb 설정을 그 HOME 에
#                읽기 전용으로 붙인다. 복사가 아니라 같은 파일이다.
# DOCKER_TTY   : 터미널이 아닌 곳(스크립트)에서 부를 때 make dcheck DOCKER_TTY=-i
DOCKER_TTY ?= -it
DOCKER_IMG ?= memdbg
DOCKER := docker run --rm $(DOCKER_TTY) \
  --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
  --user $(shell id -u):$(shell id -g) -e HOME=/tmp \
  -v "$(HOME)/.gdbinit":/tmp/.gdbinit:ro -v "$(HOME)/.config/gdb":/tmp/.config/gdb \
  -v "$(CURDIR)":/work -v "$(CURDIR)/build-docker":/work/build -w /work \
  $(DOCKER_IMG)

build-docker:
	@mkdir -p $@

dimage:                      ## 컨테이너 이미지 빌드 (최초 1회, 1~3분)
	docker build -t $(DOCKER_IMG) .

dcheck: | build-docker       ## 컨테이너에서 20개 전부 실행 → 크래시 요약 (검증 기준)
	$(DOCKER) make check

dshell: | build-docker       ## 컨테이너 셸
	$(DOCKER) bash

$(DRCH): dr_%: | build-docker
	$(DOCKER) make r_$*

$(DGCH): dg_%: | build-docker
	$(DOCKER) make g_$*

dclean:
	rm -rf build-docker

.PHONY: $(CH) $(RCH) $(GCH) $(DRCH) $(DGCH) $(LABR) $(LABG) $(LABDG) asan aclean dimage dcheck dshell dclean help-all

.DEFAULT_GOAL := help-all
help-all: help
	@echo; \
	echo "로컬 편의 (GNUmakefile)"; \
	echo "  make <이름> | r_<이름> | g_<이름>     호스트: 빌드 | 실행 | gdb (탭 완성 됨)"; \
	echo "  make dcheck                          컨테이너: 20개 크래시 요약 (검증 기준)"; \
	echo "  make dr_<이름> | dg_<이름>            컨테이너: 실행 | gdb   (10, 17 은 여기서)"; \
	echo "  make dshell | dimage | dclean        컨테이너 셸 | 이미지 빌드 | build-docker 정리"; \
	echo "  make asan <이름> | asan g_<이름>      ASan+UBSan 으로 빌드해 실행 | gdb (수정 검증 후에)"; \
	echo "  make aclean                          build-asan 정리 (make clean 에도 딸려간다)"; \
	echo "  make lab_chunk | g_lab_chunk         힙 청크 실험: 실행 | gdb  (컨테이너는 dg_lab_chunk)"
