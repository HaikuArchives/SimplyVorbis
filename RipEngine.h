/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

#ifndef _RIPENGINE_H
#define _RIPENGINE_H

#include <Looper.h>
#include <String.h>
#include <View.h>

#include <vector>

#include "CDAudioDevice.h"
#include "Observer.h"
#include "FunctionObjectMessage.h"
#include "CDDBSupport.h"

class RipEngine;

extern CDAudioDevice gCDDevice;

// watcher sits somewhere were it can get pulses and makes sure
// notices get sent if state changes
class PeriodicWatcher : public Notifier 
{
public:
						PeriodicWatcher(void);
	virtual				~PeriodicWatcher() {}

			void		DoPulse();
			void		UpdateNow();
			
			void		AttachedToLooper(RipEngine *engine)
						{ this->engine = engine; }
	virtual BHandler	*RecipientHandler() const;

protected:
	virtual	bool		UpdateState() = 0;
			RipEngine	*engine;
};

class CDContentWatcher : public PeriodicWatcher 
{
public:
				CDContentWatcher(void);
	
	bool		GetContent(BString *title, BString *genre, vector<BString> *tracks);
	
private:
	bool		UpdateState();

	CDDBQuery	cddbQuery;
	int32		discID;
};

class RipEngine : public BHandler 
{
public:
					RipEngine(void);

	virtual 		~RipEngine();

			// observing support
	virtual	void	MessageReceived(BMessage *);
			void	AttachedToLooper(BLooper *);
			void	DoPulse();

			// control calls
			void 	Stop();
			void 	Eject();
			int16	CountTracks(void) const;
			void	GetTrackTime(int16 index, int32 &minutes, int32 &seconds);
			
			CDState	GetState(void) const { return fEngineState; }
			
			void	SetDrive(const int16 &drivenum);
			
	CDContentWatcher *ContentWatcher()
		{ return &contentWatcher; }
	
private:
	CDContentWatcher 	contentWatcher;

	bigtime_t 			lastPulse;
	
	CDState 			fEngineState;
};


// some function object glue
class RipEngineFunctorFactory : public FunctorFactoryCommon 
{
public:
	static BMessage *NewFunctorMessage(void (RipEngine::*func)(),
										RipEngine *target)
	{
		PlainMemberFunctionObject<void (RipEngine::*)(),
			RipEngine> tmp(func, target);
		return NewMessage(&tmp);
	}

	static BMessage *NewFunctorMessage(void (RipEngine::*func)(ulong),
										RipEngine *target, ulong param)
	{
		SingleParamMemberFunctionObject<void (RipEngine::*)(ulong),
			RipEngine, ulong> tmp(func, target, param);
		return NewMessage(&tmp);
	}
};


#endif