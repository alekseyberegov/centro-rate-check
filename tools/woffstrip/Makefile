CC = LANG=C g++
LFLAGS += -lz
CFLAGS += -Wall -Werror -g -O2
NAME = woffstrip

HEADERS = $(wildcard *.hpp)
SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,%.o,$(SOURCES))
PREFIX ?= /usr/local

$(NAME): $(OBJECTS)
	$(CC) \
	-o $(@) \
	$(^) \
	$(LFLAGS)

%.o: %.cpp $(HEADERS) Makefile
	$(CC) -c \
	$(CFLAGS) \
	$(<) \
	-o $(@)

.PHONY: install
install: $(NAME)
	@install -v -t "$(DESTDIR)$(PREFIX)/bin" $(^)

.PHONY: clean
clean:
	rm -f $(NAME) $(OBJECTS)
