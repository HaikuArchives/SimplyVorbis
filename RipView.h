#ifndef RIPVIEW_H
#define RIPVIEW_H

#include <View.h>
#include <Button.h>
#include <OS.h>
#include <ListItem.h>
#include <ListView.h>
#include <StatusBar.h>

#include "CDAudioDevice.h"

class RipView : public BView
{
public:
	RipView(const BRect &frame, CDAudioDevice *cd);
	~RipView(void);
	
	void MessageReceived(BMessage *msg);
	void AttachedToWindow(void);
	
	void Go(void);
	void Stop(void);
	
private:
	static int32 	RipThread(void *data);
	BButton			*fStop;
	thread_id		fRipThread;
	
	BString			fProgressLabel;
	float			fProgressDelta;
	BStatusBar		*fProgressBar;
	BListView		*fRipList;
};

#endif
