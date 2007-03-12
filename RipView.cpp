#include <vector.h>
#include <Alert.h>
#include <SupportDefs.h>
#include "RipView.h"
#include "MainWindow.h"
#include "TypedList.h"
#include <Directory.h>
#include <Entry.h>
#include <Roster.h>
#include <List.h>
#include <Node.h>
#include <NodeInfo.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <fs_info.h>
#include <Volume.h>
#include <StringView.h>
#include <ScrollView.h>
#include "Preferences.h"
#include "RipSupport.h"

#ifdef B_ZETA_VERSION_VENTURE // this is Zeta R1.X (Fungerar med R1.1)
#define SYS_ZETA
#endif

#ifdef SYS_ZETA
#include <locale/Locale.h>
#include <locale/Formatter.h>
#else
#define _T(x) x
#endif

enum
{
	M_STOP
};

#define FILE_ESCAPE_CHARACTERS "<> '\"\\|?[]{}():;`,&#/"

extern CDAudioDevice gCDDrive;
static sem_id abort_thread=0;
extern TypedList<bool*> gTrackList;
extern CDDBData gCDData;

RipView::RipView(const BRect &frame, CDAudioDevice *cd)
	: BView(frame,"ripview",B_FOLLOW_ALL,B_WILL_DRAW),
	fRipThread(-1)
{
	abort_thread = create_sem(1,"rip_abort_sem");
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	fStop = new BButton(BRect(0,0,1,1),"stop",_T("Stop"),new BMessage(M_STOP),
						B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	fStop->ResizeToPreferred();
	fStop->MoveTo(Bounds().right - 5 - fStop->Bounds().Width(),
					Bounds().bottom - 5 - fStop->Bounds().Height());
	AddChild(fStop);
	
	BRect r(5,5,Bounds().right - 5,30);
	fProgressBar = new BStatusBar(r,"progressbar");
	fProgressBar->SetResizingMode(B_FOLLOW_TOP | B_FOLLOW_LEFT_RIGHT);
	AddChild(fProgressBar);
	
	r.Set(5,fProgressBar->Frame().bottom+15,Bounds().right - 5, fProgressBar->Frame().bottom+35);
	BStringView *listLabel = new BStringView(r,"listlabel",_T("Songs to be Converted:"));
	AddChild(listLabel);
	
	r.Set(5,listLabel->Frame().bottom+5,Bounds().right - 5 - B_V_SCROLL_BAR_WIDTH, fStop->Frame().top - 10);
	fRipList = new BListView(r,"riplist",B_SINGLE_SELECTION_LIST,B_FOLLOW_ALL);
		
	BScrollView *ripsv = new BScrollView("ripsv",fRipList, B_FOLLOW_ALL,0,false,true);
	AddChild(ripsv);
}

RipView::~RipView(void)
{
	if(fRipThread!=-1)
		kill_thread(fRipThread);
	delete_sem(abort_thread);
}

void RipView::Go(void)
{
	if(fRipThread!=-1)
		Stop();
	
	fRipThread = spawn_thread(RipThread,"rip thread",B_LOW_PRIORITY,this);
	resume_thread(fRipThread);
}

void RipView::Stop(void)
{
	if(fRipThread!=-1)
	{
		acquire_sem(abort_thread);
		release_sem(abort_thread);
	}
}

void RipView::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case B_VALUE_CHANGED:
		{
			fProgressBar->Update(fProgressDelta,fProgressLabel.String());
			break;
		}
		case M_STOP:
		{
			Stop();
			break;
		}
		default:
		{
			BView::MessageReceived(msg);
			break;
		}
	}
}

void RipView::AttachedToWindow(void)
{
	fStop->SetTarget(this);
}

int32 RipView::RipThread(void *data)
{
	acquire_sem(abort_thread);
	
	RipView *view = (RipView*)data;
	
	view->Window()->Lock();
	view->fProgressBar->SetText(_T("Creating songs from CD..."));
	view->Window()->Unlock();
	
	// Get all the preferences that we'll need in one shot to save on piddling
	// around with locking
	prefsLock.Lock();
	
	int16 foldermode;
	if(preferences.FindInt16("foldermode",&foldermode)!=B_OK)
		foldermode=1;
	
	int16 trackname;
	if(preferences.FindInt16("namestyle",&trackname)!=B_OK)
		trackname=0;
	
	int16 bitrate;
	if(preferences.FindInt16("bitrate",&bitrate)!=B_OK)
		bitrate=1;
	
	BString destfolder;
	if(preferences.FindString("path",&destfolder)!=B_OK)
		destfolder="/boot/home/music";
	
	bool use_mp3;
	if(preferences.FindBool("usemp3",&use_mp3)!=B_OK)
		use_mp3=false;
	
	if(use_mp3 && (!gMP3Format || !gMP3Codec))
		use_mp3=false;
	
	bool make_playlist;
	if(preferences.FindBool("makeplaylist",&make_playlist)!=B_OK)
		make_playlist=true;
	
	BString playlistfolder;
	if(preferences.FindString("playlistfolder",&playlistfolder)!=B_OK)
	{
		playlistfolder=destfolder;
		playlistfolder << "/playlists";
	}
	
	prefsLock.Unlock();
	

	bool write_attributes=true;
	
	dev_t destDevice = dev_for_path(destfolder.String());
	BVolume volume(destDevice);
	if(!volume.KnowsAttr())
	{
		write_attributes=false;
		//printf("Volume for %s doesn't support attributes\n",destfolder.String());
	}
	
	// If we are grouping by artist or artist/album, we need to make sure the
	// directory exists
	switch(foldermode)
	{
		case 1:	// Artist
		{
			destfolder << "/" << gCDData.Artist() << "/";
			break;
		}
		case 2: // Album
		{
			destfolder << "/" << gCDData.Album() << "/";
			break;
		}
		case 3: // Artist & Album
		{
			destfolder << "/" << gCDData.Artist() << "/" << gCDData.Album() << "/";
			break;
		}
		default: // no special grouping
		{
			break;
		}
	}
	if(create_directory(destfolder.String(),0777)!=B_OK)
	{
		BEntry dir(destfolder.String());
		if(!dir.Exists())
		{
			BString errormsg;
			#ifdef SYS_ZETA
				errormsg=_T("Uh-oh... SimplyVorbis couldn't create the folder '");
				errormsg << destfolder << _T("RipViewMultiline1");
				
				BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),_T("OK"));
			#else
				errormsg="Uh-oh... SimplyVorbis couldn't create the folder '";
				errormsg << destfolder << "'.\n\nThis may have happened for a number of different reasons, but "
					"most often happens when making music files on a non-BeOS drive, such as one shared "
					"with Windows. Certain characters, such as question marks and slashes cause problems "
					"on these disks. You may want to check the names of the artist, album, and songs for "
					"such characters and put a good substitute in its place or remove the character entirely.";
				
				BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),"OK");
			#endif
			alert->Go();
			
			view->fRipThread = -1;
			view->Window()->PostMessage(M_STOP_ENCODING);
			
			release_sem(abort_thread);
			return 0;
		}
	}
	
	// make the directory only if the user wants to create playlists
	if(make_playlist && create_directory(playlistfolder.String(),0777)!=B_OK)
	{
		BEntry playdir(playlistfolder.String());
		if(!playdir.Exists())
		{
			BString errormsg;
			#ifdef SYS_ZETA			
			errormsg=_T("Uh-oh... SimplyVorbis couldn't create the folder '");
			errormsg << playlistfolder << _T("RIpViewMultiline2");
			
			BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),_T("OK"));
			#else
			errormsg="Uh-oh... SimplyVorbis couldn't create the folder '";
			errormsg << playlistfolder << "' for the playlists.\n\nThis may have happened for a number of different reasons, but "
				"most often happens when making playlists on a non-BeOS drive, such as one shared "
				"with Windows. Certain characters, such as question marks and slashes cause problems "
				"on these disks. You may want to check the names of the artist, album, and songs for "
				"such characters and put a good substitute in its place or remove the character entirely."
				"For the moment, your music will be created, but the playlist will not. Sorry.";
			
			BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),"OK");
			#endif
			alert->Go();
			make_playlist=false;
		}
	}
	
	// *Sigh* FAT32 volumes don't support use of question marks. I wonder what else... :(
	if(!write_attributes)
	{
		destfolder.RemoveAll("?");
		playlistfolder.RemoveAll("?");
	}
	
	BString trackPrefix;
	switch(trackname)
	{
		case 0:	// Artist
		{
			trackPrefix = gCDData.Artist();
			trackPrefix+=" - ";
			break;
		}
		case 1: // Album
		{
			trackPrefix = gCDData.Album();
			trackPrefix+=" - ";
			break;
		}
		case 2: // Artist & Album
		{
			trackPrefix = gCDData.Artist();
			trackPrefix << " - " << gCDData.Album() << " - ";
			break;
		}
		default: // no special grouping
		{
			break;
		}
	}
	
	// Populate the list of tracks to be ripped
	view->Window()->Lock();
	
	for(int32 i=view->fRipList->CountItems(); i>0; i--)
	{
		BStringItem *item = (BStringItem *)view->fRipList->RemoveItem(i);
		delete item;
	}
	
	for(int32 i=0; i<gTrackList.CountItems(); i++)
	{
		if(*(gTrackList.ItemAt(i)))
		{
			if(gCDDrive.IsDataTrack(i))
			{
				*(gTrackList.ItemAt(i)) = false;
				continue;
			}
			
			view->fRipList->AddItem(new BStringItem(gCDData.TrackAt(i)));
		}
	}
	view->Window()->Unlock();
	
	// playlists are a nice thing for quite a few people, apparently. :)
	BFile playlistfile;
	BString playlistfilename=playlistfolder;
	
	if(make_playlist)
	{
		playlistfilename << "/" << gCDData.Artist() << " - " << gCDData.Album() << ".m3u";
		
		// Append to playlists instead of overwriting them
		BEntry playlistentry(playlistfilename.String());
		if(playlistentry.Exists())
		{
			if(playlistfile.SetTo(playlistfilename.String(),B_READ_WRITE)==B_OK)
			{
				// HACK: This works around a bug in R5's BFile::Seek implementation
//				playlistfile.Seek(SEEK_END,0);
				off_t filesize;
				playlistfile.GetSize(&filesize);
				if(filesize>0)
				{
					char data[filesize];
					playlistfile.Read(&data,filesize);
				}
			}
			else
				playlistfile.SetTo(playlistfilename.String(),B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
		}
		else
			playlistfile.SetTo(playlistfilename.String(),B_READ_WRITE | B_CREATE_FILE);
		
		if(playlistfile.InitCheck()!=B_OK)
		{
			BString errormsg;
			#ifdef SYS_ZETA
			errormsg=_T("Uh-oh... SimplyVorbis couldn't create the playlist '");
			errormsg << playlistfilename << _T("RIpViewMultiline3");
			BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),_T("OK"));
			#else
			errormsg="Uh-oh... SimplyVorbis couldn't create the playlist '";
			errormsg << playlistfilename << "'.\n\nThis may have happened for a number of different reasons, but "
				"most often happens when making playlists on a non-BeOS drive, such as one shared "
				"with Windows. Certain characters, such as question marks and slashes cause problems "
				"on these disks. You may want to check the names of the artist, album, and songs for "
				"such characters and put a good substitute in its place or remove the character entirely."
				"For the moment, your music will be created, but the playlist will not. Sorry.";
			BAlert *alert = new BAlert("SimplyVorbis",errormsg.String(),"OK");
			#endif	

			alert->Go();
			make_playlist=false;
		}
	}
	
	BString syscmd;
	BMessenger msgr(view);
	bool copyfailed=false;
	bool showfailalert = 0;
	for(int32 i=0; i<gTrackList.CountItems(); i++)
	{
		if(*(gTrackList.ItemAt(i)))
		{
			view->Window()->Lock();
			BStringItem *oldtrack = (BStringItem*)view->fRipList->RemoveItem(0L);
			view->Window()->Unlock();
			delete oldtrack;
			
			BString filename(trackPrefix);
			filename+=gCDData.TrackAt(i);
			filename.ReplaceAll("/", "-");
			
			// Set things up for the progress bar for ripping this particular track
			view->Window()->Lock();
			
			view->fProgressLabel=_T("Converting '");
			view->fProgressLabel << gCDData.TrackAt(i) << "'";
			view->fProgressBar->SetText(view->fProgressLabel.String());
			
			rgb_color blue={50,150,255,255};
			view->fProgressBar->SetBarColor(blue);
			
			cdaudio_time tracklength;
			gCDDrive.GetTimeForTrack(i+1,tracklength);
			view->fProgressBar->SetMaxValue( (tracklength.minutes * 60) + tracklength.seconds);
			view->fProgressDelta = 100.0 / float( (tracklength.minutes * 60) + tracklength.seconds);
			view->fProgressBar->Reset();
			
			view->Window()->Unlock();
			
			BString ripname(filename);
			ripname += use_mp3 ? ".mp3" : ".ogg";
			cdaudio_time rippedtime;
			
			status_t ripstat=B_OK;
			
			if(use_mp3)
				ripstat=ConvertTrack(gCDDrive.GetDrivePath(),ripname.String(),i+1,*gMP3Format,
									*gMP3Codec,&msgr,abort_thread);
			else
				ripstat=VorbifyTrack(gCDDrive.GetDrivePath(),ripname.String(),i+1,&msgr,abort_thread);
			
			if(ripstat==B_INTERRUPTED)
			{
				//  This will unblock the window
				view->fRipThread = -1;
				view->Window()->PostMessage(M_STOP_ENCODING);
				release_sem(abort_thread);
				BEntry entry(ripname.String());
				if(entry.Exists())
					entry.Remove();
				return -1;
			}
			else
			if(ripstat!=B_OK)
			{
				// Because things aren't really accurate on some CDs on the last audio track, 
				// we bear with it.
				if(!gCDDrive.IsDataTrack(i+1))
				{
					view->Window()->Lock();
					view->fProgressBar->SetText(_T("Couldn't read song from the CD. Sorry!"));
					view->Window()->Unlock();
					BEntry entry(ripname.String());
					if(entry.Exists())
						entry.Remove();
					continue;
				}
				else
				{
					view->Window()->Lock();
					rgb_color darkblue={0,0,127,255};
					view->fProgressBar->Update(view->fProgressBar->MaxValue() - 
												view->fProgressBar->CurrentValue(),
												_T("Finishing early. This is not a bad thing."));
					view->fProgressBar->SetBarColor(darkblue);
					view->Window()->Unlock();
					rippedtime = GetTimeRipped();
				}
			}
			else
			{
				view->Window()->Lock();
				rgb_color darkblue={0,0,127,255};
				view->fProgressBar->SetBarColor(darkblue);
				view->Window()->Unlock();
			}
			
			// This will ensure that the drive isn't running for an unnecesary amount of time
			gCDDrive.Stop();
			
			// Set the mime type
			filename=destfolder;
			filename << trackPrefix << gCDData.TrackAt(i) << (use_mp3 ? ".mp3" : ".ogg");
			
			BNode node(ripname.String());
#ifndef FAKE_RIPPING
			if(node.InitCheck()==B_OK)
#endif
			{
				BNodeInfo nodeinfo(&node);
				if(nodeinfo.InitCheck()==B_OK)
					nodeinfo.SetType( use_mp3 ? "audio/x-mpeg" : "audio/x-vorbis");
				
				if(write_attributes)
				{
					if(strlen(gCDData.Genre())>0)
						node.WriteAttr("Audio:Genre",B_STRING_TYPE,0,gCDData.Genre(),strlen(gCDData.Genre())+1);
					node.WriteAttr("Audio:Comment",B_STRING_TYPE,0,"Created by SimplyVorbis",
									strlen("Created by SimplyVorbis")+1);
					node.WriteAttr("Audio:Title",B_STRING_TYPE,0,gCDData.TrackAt(i),
									strlen(gCDData.TrackAt(i))+1);
					node.WriteAttr("Audio:Album",B_STRING_TYPE,0,gCDData.Album(),
									strlen(gCDData.Album())+1);
					node.WriteAttr("Audio:Artist",B_STRING_TYPE,0,gCDData.Artist(),
									strlen(gCDData.Artist())+1);
					
					int32 tracknum = i+1;
					node.WriteAttr("Audio:Track",B_INT32_TYPE,0,(const void *)&tracknum, sizeof(int32));
					
					node.WriteAttr("Audio:Bitrate",B_STRING_TYPE,0,(const void *)"128", strlen("128")+1);
					
					cdaudio_time tracktime;
					if(gCDDrive.GetTimeForTrack(i+1,tracktime))
					{
						char timestring[20];
						
						// The only time when we will ever get this far when ripstat != B_OK is if
						// we have issues related to misreported track times on an enhanced CD. In this
						// case, we make use of the riptime variable declared above to find out
						// just how much was actually ripped in order to write the proper playing time
						if(ripstat!=B_OK)
							sprintf(timestring,"%.2ld:%.2ld",rippedtime.minutes,rippedtime.seconds);
						else
							sprintf(timestring,"%.2ld:%.2ld",tracktime.minutes,tracktime.seconds);
						node.WriteAttr("Audio:Length",B_STRING_TYPE,0,timestring, strlen(timestring)+1);
					}
				}
				
				// write the file's tags
				BString inString;
				
				syscmd = "tagwriter ";
				inString = gCDData.TrackAt(i);
				inString.CharacterEscape(FILE_ESCAPE_CHARACTERS,'\\');
				syscmd << "-t " << inString;
				
				inString = gCDData.Artist();
				inString.CharacterEscape("<> '\"\\|?[]{}():;`,",'\\');
				syscmd << " -a " << inString;
				
				if(strlen(gCDData.Genre())>0)
				{
					inString = gCDData.Genre();
					inString.CharacterEscape(FILE_ESCAPE_CHARACTERS,'\\');
					syscmd << " -g " << inString;
				}
				
				if(strlen(gCDData.Album())>0)
				{
					inString = gCDData.Album();
					inString.CharacterEscape(FILE_ESCAPE_CHARACTERS,'\\');
					syscmd << " -A " << inString;
				}
				
				syscmd << " -T " << (i+1) << " ";
				syscmd << " -c 'Created by SimplyVorbis' ";
				
				inString = ripname;
				inString.ReplaceAll("/", " - ");
				inString.CharacterEscape(FILE_ESCAPE_CHARACTERS,'\\');
				syscmd+=inString;
				
				//printf("Tag command: %s\n",syscmd.String());
				system(syscmd.String());
				
				// Move the file to the real destination
				BEntry entry(ripname.String());
#ifdef FAKE_RIPPING
				{
					{
#else
				if(entry.Exists())
				{
					BDirectory destination(destfolder.String());
				
					// overwrite an existing file - allow re-ripping a file :)
					if(entry.MoveTo(&destination,NULL,true)!=B_OK)
					{
#endif
						// chances are that if this failed, it's because the destination
						// path is not on the same volume
						view->Window()->Lock();
						BString out(_T("Copying to "));
						out << destfolder;
						view->fProgressBar->SetText(out.String());
						view->Window()->Unlock();
						
						BString cmddest(destfolder);
						cmddest.CharacterEscape("<> '\"\\|[]{}():;`,",'\\');
						
						// *sigh* Certain characters are not allowed for FAT32 names.
						if(!write_attributes)
						{
							cmddest.RemoveAll("?");
							syscmd="cp -fp ";
							syscmd << inString << " " << cmddest;
						}
						else
						{
							syscmd = "copyattr -d ";
							syscmd << inString << " " << cmddest;
						}
						
						//printf("Copy command: %s\n",syscmd.String());
						if(system(syscmd.String())!=0)
							copyfailed=true;
						
						if(!copyfailed)
						{
							entry.Remove();
							syscmd = "settype -t \"audio/x-vorbis\" ";
							syscmd << cmddest << inString;
							system(syscmd.String());
							printf("type command: %s\n", syscmd.String());
						}
						else
						{
							copyfailed=false;
							showfailalert++;
						}
					}
					BString playlistentry(destfolder.String());
					playlistentry << ripname << "\n";
					playlistfile.Write(playlistentry.String(),playlistentry.Length());
				}
			}
		}
		
		gCDDrive.Stop();

#ifndef FAKE_RIPPING		
		// This will show the alert once in the ripping process, as opposed to after every track.
		if(showfailalert == 1)
		{
			copyfailed=false;
			#ifdef SYS_ZETA
			BAlert *alert = new BAlert("SimplyVorbis",_T("RIpViewMultiline4"),_T("OK"));
			#else
			BAlert *alert = new BAlert("SimplyVorbis","SimplyVorbis ran into unexpected issues copying your "
				"music files to the music folder. They have not been lost, however. After clicking "
				"OK, a window will pop up and you can move them to wherever you want.","OK");
			#endif
			alert->Go();
			
			system("/boot/beos/system/Tracker . &");
		}
#endif

	}
	
	if(make_playlist)
	{
		BNodeInfo nodeinfo(&playlistfile);
		if(nodeinfo.InitCheck()==B_OK)
			nodeinfo.SetType("text/x-playlist");
		playlistfile.Unset();
	}
	
	view->Window()->Lock();
	view->fProgressBar->SetText(_T("Finished."));
	view->Window()->Unlock();
	snooze(1000000);
	
	view->fRipThread = -1;
	view->Window()->PostMessage(M_STOP_ENCODING);
	
	release_sem(abort_thread);
	return 0;
}
