TARGET 	= pam_hibp.so
LDFLAGS += -L/usr/local/lib -lhibp -lpam
CFLAGS  += -I/usr/local/include

# Default to Debian
PAM_DIR ?= /usr/lib/`uname -m`-linux-gnu/security

# Rocky Linux
# PAM_DIR = /usr/lib64/security/

$(TARGET): pam_hibp.c
	$(CC) -fPIC -shared -o $@ $^ $(LDFLAGS)

all: $(TARGET)

install: all
	install -m 0644 $(TARGET) $(DESTDIR)$(PAM_DIR)/

uninstall:
	rm -f $(DESTDIR)$(PAM_DIR)/$(TARGET)

clean:
	rm -f $(TARGET)
