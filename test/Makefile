#
#

APP = statusbar_bg

PKGS = gtk+-2.0
OBJS = $(APP).o

CFLAGS = -O2 `pkg-config --cflags $(PKGS)`
LDFLAGS = `pkg-config --libs $(PKGS)`

all: $(APP)

%.o: %.c
	gcc $(CFLAGS) -o $@ -c $<

$(APP): $(OBJS)
	gcc $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(APP) *.o
