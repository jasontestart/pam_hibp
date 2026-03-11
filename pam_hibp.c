#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <hibp.h>

int parse_argc(int argc, const char **argv, 
		char **proxy_url, char **api_url, int *audit_only, long long *threshold) {

    int result = 1;

    /* Parse Arguments from /etc/pam.d/ */
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "proxy=", 6) == 0) {
            *proxy_url = (char *)(argv[i] + 6);
        } else if (strncmp(argv[i], "api=", 4) == 0) {
            *api_url = (char *)(argv[i] + 4);
	} else if (strncmp(argv[i], "threshold=", 10) == 0) {
            *threshold = atoll(argv[i] + 10);
        } else if (strcmp(argv[i], "auditonly") == 0) {
            *audit_only = 1;
        } else {
	    result = 0;
	}
    }

    return result;

}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *password = NULL;
    char *proxy_url = NULL;
    char *api_url = NULL;
    int audit_only = 0;
    long long threshold = 1;

    if (!parse_argc(argc, argv, &proxy_url, &api_url, &audit_only, &threshold)) {
        pam_syslog(pamh, LOG_ERR, "Invalid augument detected in PAM configuration (PAM auth interface).");
        return PAM_SERVICE_ERR;
    }

    /* Get the user's password */
    int retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL);
    if (retval != PAM_SUCCESS || !password) {
        return PAM_AUTH_ERR;
    }

    /* This is where the magic happens. */
    long long count = is_pwned_password((char *)password, proxy_url, api_url);

    /* Authenticate interface is a bit different from password interface */
    if (count >= threshold) {
        if (audit_only) {
            pam_syslog(pamh, LOG_NOTICE, "Pwned password permitted for authentication (audit mode).");
            return PAM_SUCCESS;
        } else {
            pam_syslog(pamh, LOG_WARNING, "Pwned password seen %lld times in breaches.", count);
	}

	/* Inform the user why they are being rejected. */
        pam_info(pamh, "Password rejected due to known compromise.");
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *new_password = NULL;
    char *proxy_url = NULL;
    char *api_url = NULL;
    int audit_only = 0;
    long long threshold = 1;

    /* Check that all our arguments are valid */
    if (!parse_argc(argc, argv, &proxy_url, &api_url, &audit_only, &threshold)) {
        pam_syslog(pamh, LOG_ERR, "Invalid augument detected in PAM configuration (PAM password interface).");
        return PAM_SERVICE_ERR;
    }


    /* Skip the PAM_PRELIM_CHECK */
    if (!(flags & PAM_UPDATE_AUTHTOK)) {
        return PAM_SUCCESS;
    }

    int retval = pam_get_authtok(pamh, PAM_AUTHTOK, &new_password, NULL);
    if (retval != PAM_SUCCESS || !new_password) {
        return PAM_AUTHTOK_ERR;
    }


    /* There is where the magic happens */
    long long count = is_pwned_password((char *)new_password, proxy_url, api_url);

    /* Password interface is a bit different from authenticate interface. */
    if (count >= threshold) {
        if (audit_only) {
	    pam_syslog(pamh, LOG_NOTICE, "User set a password known to be compromised (audit mode).");
	    return PAM_SUCCESS;
	} else {
	    pam_syslog(pamh, LOG_NOTICE, "User attempted to set a pwned password (from %lld breaches).", count);
	}

        /* Inform the user why they are being rejected */
        pam_error(pamh, "Password rejected due to known compromise.");
        return PAM_AUTHTOK_ERR;
    }

    return PAM_SUCCESS;
}

/* pass through for N/A interfaces */

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char *argv[])
{
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char *argv[])
{
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char *argv[])
{
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char *argv[])
{
    return (PAM_SUCCESS);
}
