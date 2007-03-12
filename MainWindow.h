#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <Application.h>
#include <Roster.h>
#include <OS.h>
#include <Window.h>
#include <TextControl.h>
#include <ListItem.h>
#include <ListView.h>
#include <MenuField.h>
#include <Button.h>
#include <MediaDefs.h>
#include <MediaFormats.h>
#include <StringView.h>
#include <vector>
#include <FileGameSound.h>

#include "AutoTextControl.h"
#include "CDAudioDevice.h"
#include "CDDBSupport.h"
#include "RipView.h"

#define SV_SIGNATURE "application/x-vnd.wgp-SimplyVorbis"

#define M_START_ENCODING 'mste'
#define	M_STOP_ENCODING 'mspe'

#define M_QUERY_DRIVE 'mqdr'
#define M_DRIVE_IN_USE 'mdiu'

class MainView : public BView
{
public:
				MainView(const BRect &r);
				~MainView(void);
	
	void		AttachedToWindow();	
	void		MessageReceived(BMessage *msg);
	void 		Pulse(void);
	void		UpdateTrackList(void);
	void		Show(void);
	void		Hide(void);
	
private:
	friend class	TextBoxKeyFilter;
	
	void			WatchCDState(void);
	void			FindOtherInstances(void);
	bool			RenameCDDBFile(const char *newartist,const char *newalbum);
	void			BuildGUI(void);
	BMenu			*fAppMenu;
	BMenu			*fDriveMenu;
	BMenu			*fSelectMenu;
	
	AutoTextControl	*fArtist;
	AutoTextControl	*fAlbum;
	AutoTextControl	*fGenre;
	
	BListView		*fTrackList;
	AutoTextControl	*fTrackEditor;
	BStringView		*fDriveInUse;
	BStringView		*fCurrentDrive;
	BStringView		*fLookupInProgress;
	
	BButton			*fGoButton;
	BButton			*fSelectAll;
	
	int32			fDiscID;
	CDDBQuery		fCDQuery;
	CDState			fCDState;
	int32			fDriveNumber;
	bool			*fDrivesUsed;
};

class MainWindow : public BWindow
{
public:
	MainWindow(const BRect &frame);
	~MainWindow(void);
	
	bool QuitRequested(void);
	void MessageReceived(BMessage *msg);
	void NotifyUser(void);
private:
	BFileGameSound *fSound;
	RipView		*fRipView;
	MainView	*fMainView;
};

extern media_file_format *gMP3Format;
extern media_codec_info *gMP3Codec; 
extern CDAudioDevice gCDDrive;
extern app_info gAppInfo;

#endif
