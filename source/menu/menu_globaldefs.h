// Essential menu globals
void Menu_Init (void);
void Menu_Draw (void);
void Menu_KeyInput (int key);
void Menu_ToggleMenu_f (void);
void Menu_Main_Set (void);
void Menu_Configuration_Set (void);
image_t Menu_GetConfirmIcon(void);
// Load screens
void Menu_DrawLoadScreen (void);
extern qboolean loading_init;
void LoadingScreen_Begin(const char *map_name);
qboolean LoadingScreen_IsActive(void);
qboolean LoadingScreen_IsWaiting(void);
qboolean LoadingScreen_ShouldWaitForSpawn(void);
qboolean LoadingScreen_Key(int key, qboolean down);
void LoadingScreen_Update(void);
void LoadingScreen_Finish(void);
char *LoadingScreen_ReturnTip(void);
void LoadingScreen_DrawProgressBar(void);
void LoadingScreen_ClearProgress(void);
void LoadingScreen_CompleteProgress(void);
void LoadingScreen_BeginProgressPhase(float start, float end, int total);
void LoadingScreen_AdvanceProgress(void);

void Menu_FindKeysForCommand (char *command, int *twokeys);

void Menu_OSK_Draw (void);
void Menu_OSK_Key (int key);
