/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Debug.h>
#include <Entry.h>
#include <Directory.h>
#include <Path.h>
#include <errno.h>
#include "scsi.h"
#include "RipEngine.h"

CDAudioDevice gCDDevice;

const bigtime_t kPulseRate = 500000;

PeriodicWatcher::PeriodicWatcher(void)
{
}

 
BHandler *
PeriodicWatcher::RecipientHandler() const
{
	 return engine;
}

void
PeriodicWatcher::DoPulse()
{
	// control the period here
	if(UpdateState())
		Notify();
}

void 
PeriodicWatcher::UpdateNow()
{
	if(UpdateState())
		Notify();
}

CDContentWatcher::CDContentWatcher(void)
	:	cddbQuery("freedb.freedb.org"),
		discID(-1)
{
}

bool 
CDContentWatcher::GetContent(BString *title, BString *genre, vector<BString> *tracks)
{
	if(discID == -1)
	{
		title->SetTo("");
		tracks->empty();
		tracks->push_back("");
		return true;
	}
	
	bool result = cddbQuery.GetTitles(title, tracks, 1000000);
	if(result)
		*genre=cddbQuery.GetGenre();
	
	return result;
}

bool 
CDContentWatcher::UpdateState()
{
	int32 newDiscID = -1;
	CDState state = gCDDevice.GetState();
	
	// Check the table of contents to see if the new one is different
	// from the old one whenever there is a CD in the drive
	if(state != kNoCD) 
	{
		newDiscID = gCDDevice.GetDiscID();
		
		if (discID == newDiscID)
			return false;
		
		// We have changed CDs, so we are not ready until the CDDB lookup finishes
		cddbQuery.SetToCD(gCDDevice.GetDrivePath());
	}
	else
	{
		if(discID != -1)
		{
			discID = -1;
			return true;
		}
		else
			return false;
	}
	
	if(cddbQuery.Ready())
	{
		discID = newDiscID;
		return true;
	}

	return false;
}

RipEngine::RipEngine(void)
	:	BHandler("RipEngine"),
		fEngineState(kStopped)
{
}

RipEngine::~RipEngine()
{
}

void 
RipEngine::AttachedToLooper(BLooper *looper)
{
	looper->AddHandler(this);
	contentWatcher.AttachedToLooper(this);
}

void 
RipEngine::Stop()
{
	fEngineState = kStopped;
	gCDDevice.Stop();
}

void 
RipEngine::Eject()
{
	gCDDevice.Eject();
	fEngineState = gCDDevice.GetState();
}

void 
RipEngine::GetTrackTime(int16 index, int32 &minutes, int32 &seconds)
{
}


int16
RipEngine::CountTracks(void) const
{
	return gCDDevice.CountTracks();
}

void
RipEngine::DoPulse()
{
	// this is the RipEngine's heartbeat; Since it is a Notifier, it checks if
	// any values changed since the last hearbeat and sends notices to observers

	bigtime_t time = system_time();
	if (time > lastPulse && time < lastPulse + kPulseRate)
		return;
	
	// every pulse rate have all the different state watchers check the
	// curent state and send notifications if anything changed
	
	lastPulse = time;
	contentWatcher.DoPulse();
}

void 
RipEngine::MessageReceived(BMessage *message)
{
	// handle observing
	if (!Notifier::HandleObservingMessages(message)	&& 
		!RipEngineFunctorFactory::DispatchIfFunctionObject(message))
		BHandler::MessageReceived(message);
}
