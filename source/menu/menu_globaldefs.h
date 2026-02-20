// Essential menu globals
void Menu_Init (void);
void Menu_Draw (void);
void Menu_KeyInput (int key);
void Menu_ToggleMenu_f (void);
void Menu_Main_Set (void);
void Menu_Configuration_Set (void);
// Load screens
void Menu_DrawLoadScreen (void);
extern qboolean loading_init;

void Menu_FindKeysForCommand (char *command, int *twokeys);

void Menu_OSK_Draw (void);
void Menu_OSK_Key (int key);