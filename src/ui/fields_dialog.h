#ifndef DS1EDIT_FIELDS_DIALOG_H
#define DS1EDIT_FIELDS_DIALOG_H

typedef struct FIELD_S
{
   const char * label;
   char * value;   /* in: the default. out: the entry */
   int cap;
} FIELD_S;

/* Runs when the user chooses OK, with every value already in its field. Return 0 to
   accept, or -1 with the reason in status to keep the dialog up. */
typedef int (* FIELDS_CHECK_CB)(char * status, int cap, void * ctx);

#define FIELDS_DIALOG_MAX 6

/* A modal of text fields: Tab moves, Enter is OK, Esc cancels. A field may be left
   empty. On OK, check runs, and a -1 keeps the dialog up showing its status. On cancel
   every value goes back to its default. Returns 1 for OK, 0 for cancel. A window close
   cancels and stays pending. At most FIELDS_DIALOG_MAX fields. */
int fields_dialog_show(const char * title, FIELD_S * fields, int count,
                       FIELDS_CHECK_CB check, void * ctx);

#endif
