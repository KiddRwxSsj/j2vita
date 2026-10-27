ifeq ($(JAVACALL_DIR),)
$(error JAVACALL_DIR is not set.)
endif
ifeq ($(VITASDK),)
$(error VITASDK is not set. Example: export VITASDK=/usr/local/vitasdk)
endif

CC=arm-vita-eabi-gcc
AR=arm-vita-eabi-ar
CFLAGS += -I$(VITASDK)/arm-vita-eabi/include -I$(VITASDK)/arm-vita-eabi/include/SDL2 \
          -I$(VITASDK)/arm-vita-eabi/include/freetype2 \
          -D__vita__ -DARM -fno-strict-aliasing -Wno-unused-parameter
include environment.gmk
include $(JAVACALL_DIR)/build/gcc/rules.gmk
pre_target:
post_target:
