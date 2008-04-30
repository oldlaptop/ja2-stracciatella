#ifndef __JASCREENS_H_
#define __JASCREENS_H_

#include "Types.h"

#ifdef JA2EDITOR
extern UINT32 EditScreenInit(void);
extern UINT32 EditScreenHandle(void);
void EditScreenShutdown(void);

extern UINT32 LoadSaveScreenInit(void);
extern UINT32 LoadSaveScreenHandle(void);
#endif

extern UINT32 ErrorScreenHandle(void);

extern UINT32 InitScreenHandle(void);

extern UINT32 MainGameScreenInit(void);
extern UINT32 MainGameScreenHandle(void);
void MainGameScreenShutdown(void);


UINT32 PalEditScreenHandle(void);

UINT32 DebugScreenHandle(void);

UINT32 MapScreenInit(void);
UINT32 MapScreenHandle(void);
void MapScreenShutdown(void);

UINT32 LaptopScreenInit(void);
UINT32 LaptopScreenHandle(void);
void LaptopScreenShutdown(void);


UINT32	MessageBoxScreenHandle( void );

UINT32	MainMenuScreenHandle( void );


UINT32	SaveLoadScreenHandle( void );
UINT32	SaveLoadScreenInit( void );

UINT32	OptionsScreenHandle( void );
UINT32	OptionsScreenInit( void );

UINT32	ShopKeeperScreenInit( void );
UINT32	ShopKeeperScreenHandle( void );
void ShopKeeperScreenShutdown(void);

UINT32	SexScreenHandle( void );

UINT32	GameInitOptionsScreenHandle( void );

extern	UINT32	IntroScreenHandle( void );
extern	UINT32	IntroScreenInit( void );


// External functions
void DisplayFrameRate(void);

void HandleTitleScreenAnimation(void);

//External Globals
extern UINT32 guiCurrentScreen;

typedef void (*RENDER_HOOK)( void );

void SetRenderHook( RENDER_HOOK pRenderOverride );
void SetDebugRenderHook( RENDER_HOOK pDebugRenderOverride, INT8 ubPage );

void EnableFPSOverlay(BOOLEAN fEnable);

extern BOOLEAN gfExitDebugScreen;
extern INT8    gCurDebugPage;

#ifdef JA2DEMOADS
void DoDemoIntroduction(void);
UINT32 DemoExitScreenHandle(void);
#endif

#endif
