#ifndef FTP_STATUS_CODES_H
#define FTP_STATUS_CODES_H

/* --- 1xx: Positive Preliminary Replies --- */
#define FTP_STATUS_RESTART_MARKER 110     /* Restart marker reply. */
#define FTP_STATUS_SERVICE_READY_SOON 120 /* Service ready in nnn minutes. */
#define FTP_STATUS_DATA_CONNECTION_OPEN \
    125 /* Data connection already open; transfer starting. */
#define FTP_STATUS_FILE_STATUS_OKAY \
    150 /* File status okay; about to open data connection. */

/* --- 2xx: Positive Completion Replies --- */
#define FTP_STATUS_COMMAND_OK 200 /* Command okay. */
#define FTP_STATUS_COMMAND_NOT_IMPLEMENTED \
    202 /* Command not implemented, superfluous. */
#define FTP_STATUS_SYSTEM_STATUS                \
    211 /* System status, or system help reply. \
         */
#define FTP_STATUS_DIRECTORY_STATUS 212 /* Directory status. */
#define FTP_STATUS_FILE_STATUS 213      /* File status. */
#define FTP_STATUS_HELP_MESSAGE 214     /* Help message. */
#define FTP_STATUS_NAME_SYSTEM_TYPE 215 /* NAME system type. */
#define FTP_STATUS_SERVICE_READY 220    /* Service ready for new user. */
#define FTP_STATUS_SERVICE_CLOSING             \
    221 /* Service closing control connection. \
         */
#define FTP_STATUS_DATA_CONNECTION_CLOSING \
    226 /* Closing data connection. Requested file action successful. */
#define FTP_STATUS_ENTERING_PASSIVE_MODE \
    227 /* Entering Passive Mode (h1,h2,h3,h4,p1,p2). */
#define FTP_STATUS_ENTERING_EPSV_MODE \
    229 /* Entering Extended Passive Mode (|||a|)*/
#define FTP_STATUS_USER_LOGGED_IN 230 /* User logged in, proceed. */
#define FTP_STATUS_LOGOUT_SUCCESSFUL \
    231 /* User logged out; service terminated. */
#define FTP_STATUS_FILE_ACTION_OK \
    250 /* Requested file action okay, completed. */
#define FTP_STATUS_PATHNAME_CREATED 257 /* "PATHNAME" created. */

/* --- 3xx: Positive Intermediate Replies --- */
#define FTP_STATUS_USER_NAME_OK 331 /* User name okay, need password. */
#define FTP_STATUS_NEED_ACCOUNT 332 /* Need account for login. */
#define FTP_STATUS_FILE_ACTION_PENDING \
    350 /* Requested file action pending further information. */

/* --- 4xx: Transient Negative Completion Replies --- */
#define FTP_STATUS_SERVICE_NOT_AVAILABLE \
    421 /* Service not available, closing control connection. */
#define FTP_STATUS_CANNOT_OPEN_DATA 425 /* Can't open data connection. */
#define FTP_STATUS_CONNECTION_CLOSED \
    426 /* Connection closed; transfer aborted. */
#define FTP_STATUS_FILE_ACTION_NOT_TAKEN \
    450 /* Requested file action not taken. */
#define FTP_STATUS_ACTION_ABORTED \
    451 /* Requested action aborted. Local error in processing. */
#define FTP_STATUS_INSUFFICIENT_STORAGE \
    452 /* Requested action not taken. Insufficient storage. */

/* --- 5xx: Permanent Negative Completion Replies --- */
#define FTP_STATUS_SYNTAX_ERROR 500 /* Syntax error, command unrecognized. */
#define FTP_STATUS_SYNTAX_ERROR_PARAMS \
    501 /* Syntax error in parameters or arguments. */
#define FTP_STATUS_COMMAND_NOT_IMPLEMENTED_PERM \
    502                                 /* Command not implemented. */
#define FTP_STATUS_BAD_SEQUENCE 503     /* Bad sequence of commands. */
#define FTP_STATUS_UNSUPPORTED_TYPE 504 /* Unsupported supplied parameter */
#define FTP_STATUS_NOT_LOGGED_IN 530    /* Not logged in. */
#define FTP_STATUS_NEED_ACCOUNT_FOR_STOR \
    532 /* Need account for storing files. */
#define FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM \
    550 /* Requested action not taken (e.g., file not found). */
#define FTP_STATUS_PAGE_TYPE_UNKNOWN \
    551 /* Requested action aborted. Page type unknown. */
#define FTP_STATUS_EXCEEDED_STORAGE_ALLOC \
    552 /* Requested file action aborted. Exceeded storage. */
#define FTP_STATUS_FILE_NAME_NOT_ALLOWED \
    553 /* Requested action not taken. File name not allowed. */

/* --- TLS Specific (RFC 4217) --- */
#define FTP_STATUS_AUTH_TLS_OK \
    234 /* AUTH command OK. Expecting TLS negotiation. */

#endif /* FTP_STATUS_CODES_H */
