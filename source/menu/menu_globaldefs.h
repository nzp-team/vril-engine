// Essential menu globals
void Menu_Init (void);
void Menu_Draw (void);
void Menu_KeyInput (int key);
void Menu_ToggleMenu_f (void);
void Menu_Main_Set (void);
// Load screens
void Menu_DrawLoadScreen (void);
extern qboolean loading_init;

void Menu_FindKeysForCommand (char *command, int *twokeys);

// unimplemented
void M_OSK_Draw (void);
void Con_OSK_Key (int key);