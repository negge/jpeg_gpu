BIN_DIR := bin
SRC_DIR := src
RES_DIR := res
TEST_DIR := test

BINS := bin/jpeg_gpu
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(filter-out \
 $(patsubst $(BIN_DIR)/%,$(SRC_DIR)/%.c,$(BINS)),$(wildcard $(SRC_DIR)/*.c)))
TEST := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/test/%, \
 $(wildcard $(TEST_DIR)/*.c))
GLSL := $(wildcard $(RES_DIR)/*.glsl)


ifneq ($(shell uname),Darwin)
LIBS := `pkg-config --libs glfw3` `pkg-config --libs gl` -ljpeg -lm
else
LIBS := `pkg-config --libs glfw3` -L/usr/local/lib -ljpeg -lm -framework OpenGL
endif

CFLAGS := -std=c89 -pedantic
CFLAGS += -O2 -Wall -Wextra -Wno-parentheses -pedantic -Wno-overlength-strings
#CFLAGS += -g
CFLAGS += `pkg-config --cflags glfw3`
CFLAGS += -DGLJ_ENABLE_LOGGING
#CFLAGS += -DGLJ_ENABLE_VALIDATION

ifeq ($(shell uname),Darwin)
CFLAGS += -I/usr/local/include
endif

guard=@mkdir -p $(@D)

all: $(OBJS) $(BINS) $(TEST)

$(BIN_DIR)/%.o:$(SRC_DIR)/%.c
	$(guard)
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $<

$(BIN_DIR)/%:$(SRC_DIR)/%.c $(OBJS)
	$(guard)
	$(CC) $(INCLUDES) $(CFLAGS) $< $(OBJS) -o $@ $(LIBS)

$(BIN_DIR)/test/%: $(TEST_DIR)/%.c $(OBJS)
	$(guard)
	$(CC) $(INCLUDES) $(CFLAGS) $< $(OBJS) -o $@ $(LIBS)

test: all
	@true $(foreach test,$(TEST), && $(test))

clean:
	rm -rf $(BIN_DIR) src/jpeg_gpu.h

check-syntax:
	$(CC) $(INCLUDES) $(CFLAGS) -o /dev/null -S ${CHK_SOURCES} $(LIBS)

src/jpeg_gpu.h: $(GLSL)
	@echo '/* Generated file, do not commit */' > src/jpeg_gpu.h
	@$(foreach glsl,$(patsubst $(RES_DIR)/%.glsl,%,$(GLSL)), \
		echo 'static const char' \
			$(shell echo $(glsl)[] | tr '[.a-z]' '[_A-Z]') \
			'= {' >> src/jpeg_gpu.h; \
		xxd -i - < $(RES_DIR)/$(glsl).glsl >> src/jpeg_gpu.h; \
		echo ', 0x00 };' >> src/jpeg_gpu.h; \
	)

bin/jpeg_gpu: src/jpeg_gpu.h
