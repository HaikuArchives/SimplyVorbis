#ifndef PREFS_WINDOW_H
#define PREFS_WINDOW_H

#include <Window.h>
#include <FilePanel.h>
#include <Button.h>
#include <MenuField.h>
#include <TextControl.h>
#include <CheckBox.h>

#include "CDAudioDevice.h"

class PrefsWindow : public BWindow
{
public:
	PrefsWindow(const BRect &rect);
	~PrefsWindow(void);
	void MessageReceived(BMessage *msg);
	void AttachedToWindow(void);
	
private:
	
	BFilePanel	*fFilePanel,*fSoundFilePanel;
	BButton		*fBrowseButton;
	BButton		*fSoundBrowseButton;
	BTextControl *fMP3Path;
	BTextControl *fSoundFilePath;
	
	BMenuField	*fTrackNameField;
	BMenuField	*fFormatField;
	BMenuField	*fGroupField;
	
	BMenuField	*fDriveList;
	BCheckBox	*fEjectBox;
	BCheckBox	*fNotifyShow;
	BCheckBox	*fNotifySound;
	BCheckBox	*fPlaylistBox;
	
	CDAudioDevice	fCD;
};

#endif
