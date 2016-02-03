BIN_DIR := bin
SRC_DIR := src

OBJS := 
BINS := bin/jpeg_gpu
LIBS := `pkg-config --libs sdl`


CFLAGS := -std=c89 -pedantic
CFLAGS += -O0 -Wall -Wextra -Wno-parentheses -pedantic
CFLAGS += -g
CFLAGS += `pkg-config --cflags sdl`

guard=@mkdir -p $(@D)

all: $(OBJS) $(BINS)

$(BIN_DIR)/%.o:$(SRC_DIR)/%.c
	$(guard)
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $(LIBS) $<

$(BIN_DIR)/%:$(SRC_DIR)/%.c $(OBJS)
	$(guard)
	$(CC) $(INCLUDES) $(CFLAGS) $< $(OBJS) -o $@ $(LIBS)

clean:
	rm -rf $(BIN_DIR)

check-syntax:
	$(CC) $(INCLUDES) $(CFLAGS) -o /dev/null -S ${CHK_SOURCES} $(LIBS)
