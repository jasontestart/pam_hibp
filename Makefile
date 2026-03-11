TARGET 	= pam_hibp.so
LDFLAGS += -L/usr/local/lib -lhibp -lpam
CFLAGS  += -I/usr/local/include

PAM_DIR = /lib/x86_64-linux-gnu/security

$(TARGET): pam_hibp.c
	$(CC) -fPIC -shared -o $@ $^ $(LDFLAGS)

all: $(TARGET)

install: all
	install -m 0644 $(TARGET) $(DESTDIR)$(PAM_DIR)/

uninstall:
	rm -f $(DESTDIR)$(PAM_DIR)/$(TARGET)

clean:
	rm -f $(TARGET)
