# pam_hibp

A Pluggable Authentication Module (PAM) for Linux that interacts with the 
[Have I Been Pwned](https://haveibeenpwned.com/) **Pwned Password** API using k-Anonymity.
 It is designed work with other authentication modules 
(e.g., pam_unix) to support both authentication and password management interfaces.

## Dependencies

To build and install this module, you will need to:
1. Build and install the [libhibp](https://github.com/jasontestart/libhibp) library.
2. Install the PAM development libraries. 

### Debian-based installation
```bash
sudo apt update
sudo apt install libpam0g-dev
```

### RHEL-based installation
```bash
sudo dnf install pam-devel
```

## Building & Installing

To build, simply run make:
```bash
make
```

Installation is a little bit tricky, a the location of PAM modules will depend on the Linux distribution,
 and in many cases the machine architecture.
To find the location of the PAM modules in your Linux distribution, try running the following:
```bash
find /usr -name pam_unix.so
```

### Debian-based distributions

To install on a Debian-based OS (tested with Debian, Ubuntu, and Raspberry Pi OS):
```bash
sudo PAM_DIR=/usr/lib/`uname -m`-linux-gnu/security make install
```

### RHEL-based distributions

To install on a RHEL-based OS (tested with Rocky Linux:
```bash
sudo PAM_DIR=/usr/lib64/security make install
```
## Potential Obstacles

There may be several additional steps needed to get this module to work, which will ironcally
have security implications on your system. 

#### LD_LIBRARY_PATH

This module depends on the `libhibp` library, which installs in `/usr/local/lib`. Your system may not 
be configured to search in this directory for system libraries.  You will need to make sure that 
`LD_LIBRARY_PATH` is set appropriately, and this may look different between a user running the `passwd`
command, or this module being in the auth stack for a daemon like `sshd`.

In the daemon scanario, one approach is to set-up the environment, specifically in `systemd`, where to look for the `libhibp` library. You can do this by running the command `sudo systemctl edit sshd` and adding
the following lines near the beginning of the file:

```ini
[Service]
Environment="LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH"
```

#### SELinux network security policy

The default SELinux network security policy may prevent certain programs, such as`sshd`, from trying to connect to
the HIBP API.  To override this, run:
```bash
sudo setsebool -P nis_enabled 1
```

## Configuration

### Stack placement by module interface

#### Password

For the PAM password interface, this module should be placed early in the PAM stack, 
right **before** the module that does the "real work" (typically `pam_unix`). 
The control flag `requisite` is recommended, so that unacceptable passwords are
rejected early.

A simple insertion at the top of the stack should be all that's needed:

**Example password stack on Rocky Linux (e.g., /etc/pam.d/passwd)**
```ini
#%PAM-1.0
# This tool only uses the password stack.
password   requisite    pam_hibp.so
password   substack	system-auth
-password   optional	pam_gnome_keyring.so use_authtok
password   substack	postlogin
```

#### Auth

For the PAM auth interface, this module should be placed at the end of the PAM stack,
with the control flag set to `required`.

**Example auth stack on Ubuntu (e.g., /etc/pam.d/common-auth)**
```ini
# here are the per-package modules (the "Primary" block)
auth    [success=1 default=ignore]  pam_unix.so nullok
# here's the fallback if no module succeeds
auth    requisite           pam_deny.so
# prime the stack with a positive return value if there isn't one already;
# this avoids us returning an error just because nothing sets a success code
# since the modules above will each just jump around
auth    required            pam_permit.so
# and here are more per-package modules (the "Additional" block)
auth    optional            pam_cap.so
# end of pam-auth-update config
auth    required            pam_hibp.so
```

#### Caution

**The use of `sufficient` or `optional` control flags with this module are not recommended**,
as you will likely end-up with an insecure configuration by doing so.

### Module Arguments

With no arguments, the `pam_hibp` module, through `libhibp`, will take the SHA1 hash of the provided
password, and using k-Anonymity, will lookup the hash using the [Pwned Password API](https://haveibeenpwned.com/API/v3#PwnedPasswords) at `https://api.pwnedpasswords.com/range/`.
If the hash is found one or more times in the database, then authentication (or password change) is rejected
 and the action is recorded to syslog. The module's behaviour may be modified with the following arguments:

**auditonly**

If the `auditonly` argument is present, then the detection of a Pwned Password will be written to syslog.
Authentication or password change will not be affected by the module.

**proxy**

You can configure the module to use a proxy server when connecting to the Pwned Password API. Any proxy
supported by `libcurl` is supported, provided the the scheme can be defined with a url prefix. 

Example: `proxy=https://myproxy.internal:3128/`.

See [https://curl.se/libcurl/c/CURLOPT_PROXY.html](https://curl.se/libcurl/c/CURLOPT_PROXY.html).

**api**

There is a mechanism to [download](https://github.com/HaveIBeenPwned/PwnedPasswordsDownloader)
 the entire 85GB+ Pwned Password database and host the API yourself. This module can be configured
to use a different API endpoint, provided it behaves exactly the same as 
`https://api.pwnedpasswords.com/range/`.

Example: `api=https://mypwneddb.internal/range/`.

The `api` and `proxy` arguments may be combined.

**threshold**

You may have a risk tolerance that allows a good quality password (i.e., a sufficiently long passphrase)
that may appear in the pwned password database but for a small number of breaches as you have other 
controls in place (e.g., MFA, network segmentation). You can define a threshold so that the module will
stay silent and/or take no action when a password appears in the database, but the number of breaches is 
less than the defined threshold.

Example: `threshold=1000`: The module will catch the password `abc123`, but not the passphrase `This is a test.`, even though both are in the Pwned Password database.

The `threshold` and `auditonly` arguments may be combined.
