#ifndef COMMAND_ACTIONS_H
#define COMMAND_ACTIONS_H

#include "command_parser.h"

typedef struct
{
    const char *action;
    command_execution_cb authenticated_cb;
    command_execution_cb non_authenticated_cb;
} command_action;

#define ACTION_FUNC(action) \
    void action(cftp_command_t *command, connection_t *connection)

#define DECL_ACTION_FOR_COMMAND(action, function)  \
    static const char *action##_COMMAND = #action; \
    ACTION_FUNC(function);

#define ADD_COMMAND_WITH_SAME_ACTION(command, function)   \
    {                                                     \
        .action = #command, .authenticated_cb = function, \
        .non_authenticated_cb = function                  \
    }

#define ADD_COMMAND_WITH_DIFF_ACTION(                                   \
    command, authenticated_function, non_authenticated_function)        \
    {                                                                   \
        .action = #command, .authenticated_cb = authenticated_function, \
        .non_authenticated_cb = non_authenticated_function              \
    }

/* Made these macros because I got a bug earlier using strncmp as I was not
 * validating return value to be 0 */
#define IF_MATCHES(input_command, expected_command) \
    if (strncmp(input_command, expected_command, MAX_COMMAND_LENGTH) == 0)
#define IF(x) if (x)
#define ELSE else

/* Comman commands for authenticated and non authenticated */
DECL_ACTION_FOR_COMMAND(SYST, cftp_syst_action)
DECL_ACTION_FOR_COMMAND(QUIT, cftp_quit_action)
DECL_ACTION_FOR_COMMAND(AUTH, cftp_auth_action)
DECL_ACTION_FOR_COMMAND(PBSZ, cftp_pbsz_action)
DECL_ACTION_FOR_COMMAND(PROT, cftp_prot_action)
DECL_ACTION_FOR_COMMAND(NOOP, cftp_noop_action)
DECL_ACTION_FOR_COMMAND(FEAT, cftp_feat_action)
DECL_ACTION_FOR_COMMAND(INVALID, cftp_invalid_action)

/* Authenticated only */
DECL_ACTION_FOR_COMMAND(TYPE, cftp_type_authenticated_action)
DECL_ACTION_FOR_COMMAND(EPSV, cftp_epsv_authenticated_action)
DECL_ACTION_FOR_COMMAND(PASV, cftp_pasv_authenticated_action)
DECL_ACTION_FOR_COMMAND(NLST, cftp_nlst_authenticated_action)
DECL_ACTION_FOR_COMMAND(LIST, cftp_list_authenticated_action)
DECL_ACTION_FOR_COMMAND(SIZE, cftp_size_authenticated_action)
DECL_ACTION_FOR_COMMAND(RETR, cftp_retr_authenticated_action)
DECL_ACTION_FOR_COMMAND(STOR, cftp_stor_authenticated_action)
DECL_ACTION_FOR_COMMAND(MDTM, cftp_mdtm_authenticated_action)
DECL_ACTION_FOR_COMMAND(CWD, cftp_cwd_authenticated_action)
DECL_ACTION_FOR_COMMAND(PWD, cftp_pwd_authenticated_action)
DECL_ACTION_FOR_COMMAND(ABOR, cftp_abor_authenticated_action)
DECL_ACTION_FOR_COMMAND(RMD, cftp_rmd_authenticated_action)
DECL_ACTION_FOR_COMMAND(MKD, cftp_mkd_authenticated_action)
DECL_ACTION_FOR_COMMAND(DELE, cftp_dele_authenticated_action)
DECL_ACTION_FOR_COMMAND(NON_AUTH, cftp_non_authenticated)

/* Non Authenticated only */
DECL_ACTION_FOR_COMMAND(USER, cftp_user_authenticated)
DECL_ACTION_FOR_COMMAND(PASS, cftp_pass_authenticated)
DECL_ACTION_FOR_COMMAND(AUTHENTICATED, cftp_authenticated)

static const command_action command_actions[] = {

    /* Authenticated and Non Authenticated */
    ADD_COMMAND_WITH_SAME_ACTION(SYST, cftp_syst_action),
    ADD_COMMAND_WITH_SAME_ACTION(QUIT, cftp_quit_action),
    ADD_COMMAND_WITH_SAME_ACTION(AUTH, cftp_auth_action),
    ADD_COMMAND_WITH_SAME_ACTION(PBSZ, cftp_pbsz_action),
    ADD_COMMAND_WITH_SAME_ACTION(PROT, cftp_prot_action),
    ADD_COMMAND_WITH_SAME_ACTION(NOOP, cftp_noop_action),
    ADD_COMMAND_WITH_SAME_ACTION(FEAT, cftp_feat_action),

    /* Authenticated only */
    ADD_COMMAND_WITH_DIFF_ACTION(TYPE,
                                 cftp_type_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(EPSV,
                                 cftp_epsv_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(PASV,
                                 cftp_pasv_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(NLST,
                                 cftp_nlst_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(LIST,
                                 cftp_list_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(SIZE,
                                 cftp_size_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(RETR,
                                 cftp_retr_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(STOR,
                                 cftp_stor_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(MDTM,
                                 cftp_mdtm_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(CWD,
                                 cftp_cwd_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(PWD,
                                 cftp_pwd_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(ABOR,
                                 cftp_abor_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(MKD,
                                 cftp_mkd_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(RMD,
                                 cftp_rmd_authenticated_action,
                                 cftp_non_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(DELE,
                                 cftp_dele_authenticated_action,
                                 cftp_non_authenticated),

    /* Non authenticated only */
    ADD_COMMAND_WITH_DIFF_ACTION(USER,
                                 cftp_authenticated,
                                 cftp_user_authenticated),
    ADD_COMMAND_WITH_DIFF_ACTION(PASS,
                                 cftp_authenticated,
                                 cftp_pass_authenticated)

};

#endif
