#include <Alert.h>
#include <Application.h>
#include <Beep.h>
#include <FindDirectory.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <Messenger.h>
#include <Path.h>
#include <ScrollView.h>
#include <String.h>
#include <TranslationUtils.h>

#include <vector.h>
#include <stdlib.h>
#include <ctype.h>


#ifdef B_ZETA_VERSION_VENTURE // this is Zeta R1.X (Fungerar med R1.1)
#define SYS_ZETA
#endif

#ifdef SYS_ZETA
#include <locale/Locale.h>
#include <locale/Formatter.h>
#else
#define _T(x) x
#endif


#include "AboutWindow.h"
#include "RipSupport.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "PrefsWindow.h"
#include "TypedList.h"

static MainView *sMainView=NULL;
TypedList<bool*> gTrackList(20,true);
CDAudioDevice gCDDrive;
CDDBData gCDData;
media_file_format *gMP3Format=NULL;
media_codec_info *gMP3Codec=NULL;
app_info gAppInfo; 

void QueryBusyDrives(const uint16 &index);

enum
{
	M_GENRE_CHANGED=100,
	M_TRACK_CHANGED,
	M_ARTIST_CHANGED,
	M_ALBUM_CHANGED,
	M_CHOOSE_TRACK,
	M_SHOW_PREFERENCES,
	M_SAVE_CHANGES,
	M_SHOW_ABOUT,
	M_SELECT_DRIVE,
	M_SHOW_LIBRARY,
	M_SELECT_ALL,
	M_SELECT_NEXT,
	M_SELECT_PREVIOUS,
	M_SELECT_INVERT,
	M_CHOOSE_DRIVE,
	M_EDIT_KEY
};

MainView::MainView(const BRect &r)
 : BView(r,"mainview",B_FOLLOW_ALL,B_WILL_DRAW | B_PULSE_NEEDED),
 	fCDQuery("freedb.freedb.org")
{
	sMainView = this;
	fDiscID = -1;
	fDriveNumber = 0;
	fDrivesUsed = new bool[gCDDrive.CountDrives()];
	for(uint8 i=0; i<gCDDrive.CountDrives(); i++)
		fDrivesUsed = false;
	FindOtherInstances();
	
	LoadPreferences(PREFERENCES_PATH);
	
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	BuildGUI();
}

MainView::~MainView(void)
{
	delete [] fDrivesUsed;
}

void MainView::Pulse(void)
{
	WatchCDState();
}

void MainView::AttachedToWindow()
{
	if(fDriveMenu)
		fDriveMenu->SetTargetForItems(this);
	fSelectMenu->SetTargetForItems(this);
	
	fAlbum->SetTarget(this);
	fArtist->SetTarget(this);
	fGenre->SetTarget(this);
	fTrackList->SetTarget(this);
	fTrackEditor->SetTarget(this);
	
	fSelectAll->SetTarget(this);
	WatchCDState();
}

void MainView::Show(void)
{
	fGoButton->MakeDefault(true);
	BView::Show();
}

void MainView::Hide(void)
{
	fGoButton->MakeDefault(false);
	BView::Hide();
}

void MainView::WatchCDState(void)
{
	// First, see if the drive has changed
	if(fDriveNumber != gCDDrive.GetDrive())
	{
		fCDState = kInit;
		fDriveNumber = gCDDrive.GetDrive();
		
		fTrackList->MakeEmpty();
		gTrackList.MakeEmpty();
		fArtist->SetText("");
		fAlbum->SetText("");
		fTrackEditor->SetText("");
		fGenre->SetText("");
	}
	
	// Second, establish whether or not we have a CD in the drive
	CDState playstate = gCDDrive.GetState();
	
		// Yes, we have no bananas!
		
	if(fCDState != playstate)
	{
		switch(playstate)
		{
			case kNoCD:
			{
				// We have just discovered that we have no bananas
				fCDState = kNoCD;
				fTrackList->MakeEmpty();
				gTrackList.MakeEmpty();
				fArtist->SetText("");
				fAlbum->SetText("");
				fTrackEditor->SetText("");
				fGenre->SetText("");
			}
			case kStopped:
			{
				fCDState = kStopped;
				fGoButton->SetEnabled(true);
				if(!fDriveInUse->IsHidden())
					fDriveInUse->Hide();
				break;
			}
			default:
			{
				// This means that the drive is in use, so prevent use of the drive
				fGoButton->SetEnabled(false);
				if(fDriveInUse->IsHidden())
					fDriveInUse->Show();
				fCDState = playstate;
			}
		}
	}
	else
	{
		// No change in the app's play state, so do nothing except exit
		// when there's no CD
		if(fCDState==kNoCD)
			return;
	}
	
//------------------------------------------------------------------------------------------------
	// If we got this far, then there must be a CD in the drive. The next order on the 
	// agenda is to find out which CD it is
	
	int16 trackcount = gCDDrive.CountTracks();
	
	int32 discid = gCDDrive.GetDiscID();
	bool update_track_gui=false;
	
	if(discid != fDiscID)
	{
		update_track_gui = true;
		
		// Apparently the disc has changed since we last looked.
		if(fCDQuery.CurrentDiscID()!=discid)
		{
			fCDQuery.SetToCD(gCDDrive.GetDrivePath());
			if(fLookupInProgress->IsHidden())
				fLookupInProgress->Show();
		}
		
		if(fCDQuery.Ready())
		{
			if(!fLookupInProgress->IsHidden())
				fLookupInProgress->Hide();
			
			fDiscID = discid;
			
			for(int16 i=0; i<trackcount; i++)
				gTrackList.AddItem(new bool(false));
			
			// Note that we only update the CD title for now. We still need a track number
			// in order to update the display for the selected track
			if(fCDQuery.GetData(&gCDData, 1000000))
			{
				BString currentTrackName,genre;
				
				fArtist->SetText(gCDData.Artist());
				fAlbum->SetText(gCDData.Album());
				
				for(int16 i=0; i<trackcount; i++)
				{
					BStringItem *item = new BStringItem(gCDData.TrackAt(i));
					fTrackList->AddItem(item);
				}
				
				// Capitalize has been broken since R4. Grrr.....
				genre = gCDData.Genre();
				char *genreString = genre.LockBuffer(0);
				if(genreString)
				{
					genreString[0] = toupper(genreString[0]);
					fGenre->SetText(genreString);
					genre.UnlockBuffer();
				}
				else
				{
					fGenre->SetText("");
				}
			}
			else
			{
				fArtist->SetText(_T("Unnamed Artist"));
				fAlbum->SetText(_T("Unnamed Album"));

				fGenre->SetText("");
				for(int16 i=0; i<trackcount; i++)
				{
					BString string;
					string << "Track " << i;
					
					BStringItem *item = new BStringItem(string.String());
					fTrackList->AddItem(item);
				}
			}
			fTrackList->Select(0,fTrackList->CountItems()-1);
		}
	}
}

void MainView::UpdateTrackList(void)
{
	// This function shouldn't even be necessary, but I guess for performance
	// reasons, BTextControls don't send update messages with every keystroke,
	// which, unfortunately, is what we want, hence the TextBoxKeyFilter
	int32 selection = fTrackList->CurrentSelection();
	
	BStringItem *stritem = (BStringItem*)fTrackList->ItemAt(selection);
	if(stritem)
	{
		BString newtext(fTrackEditor->Text());
		
		if(newtext.CountChars()>0)
		{
			stritem->SetText(newtext.String());
		}
		else
		{
			newtext << "Track " << selection + 1;
			stritem->SetText(newtext.String());
		}
		fTrackList->InvalidateItem(selection);
		gCDData.RenameTrack(selection,newtext.String());
	}
}


void MainView::BuildGUI(void)
{
	BRect r(Bounds());
	
	r.bottom = 20;
	
	BMenuBar *bar = new BMenuBar(r,"menubar");
	AddChild(bar);
	
	fAppMenu = new BMenu("Program");
	fAppMenu->AddItem(new BMenuItem(_T("About…"),new BMessage(M_SHOW_ABOUT)));
	fAppMenu->AddItem(new BMenuItem(_T("Preferences…"),new BMessage(M_SHOW_PREFERENCES),','));
	fAppMenu->AddItem(new BMenuItem(_T("Save Changes to CD Info"),new BMessage(M_SAVE_CHANGES),'S'));
	fAppMenu->AddSeparatorItem();
	
	// We can create a drives menu so that the user can easily select the drive for the
	// current session. We will automagically select a non-busy drive.
	if(gCDDrive.CountDrives()>1)
	{
		fDriveMenu = new BMenu(_T("Use CD Drive"));
		for(int16 i=0; i<gCDDrive.CountDrives(); i++)
		{
		
			BString label(_T("Drive "));
			label << (i+1);
			
			BMessage *drivemsg = new BMessage(M_SELECT_DRIVE);
			drivemsg->AddInt16("drive",i);
			BMenuItem *item = new BMenuItem(label.String(),drivemsg);
			fDriveMenu->AddItem(item);
		}
		fDriveMenu->SetRadioMode(true);
		fAppMenu->AddItem(fDriveMenu);
		
		// Check the default drive first. If not available, iterate through the drives
		// to find one which isn't.
		int16 drive;
		prefsLock.Lock();
		if(preferences.FindInt16("drive",&drive)!=B_OK)
			drive=0;
		prefsLock.Unlock();
		
		gCDDrive.SetDrive(drive);
		CDState drivestate = gCDDrive.GetState();
		if(drivestate!=kNoCD && drivestate!=kStopped)
		{
			bool foundfree=false;
			for(int16 i=0; i<gCDDrive.CountDrives(); i++)
			{
				gCDDrive.SetDrive(i);
				drivestate = gCDDrive.GetState();
				if(drivestate==kNoCD || drivestate==kStopped)
				{
					// Bingo. We have found one.
					foundfree=true;
					fDriveMenu->ItemAt(i)->SetMarked(true);
					break;
				}
			}
			
			// Crap. None free, so go back to the default one.
			if(!foundfree)
				gCDDrive.SetDrive(drive);
		}
		else
		{
			fDriveMenu->ItemAt(drive)->SetMarked(true);
		}
	}
	else
		fDriveMenu=NULL;
	fAppMenu->AddItem(new BMenuItem(_T("Show Music Folder…"),new BMessage(M_SHOW_LIBRARY),'L'));
	bar->AddItem(fAppMenu);	
	fSelectMenu = new BMenu(_T("Selection"));
	fSelectMenu->AddItem(new BMenuItem(_T("All"),new BMessage(M_SELECT_ALL),'A'));
	fSelectMenu->AddSeparatorItem();
	fSelectMenu->AddItem(new BMenuItem(_T("Previous Track"),new BMessage(M_SELECT_PREVIOUS),B_UP_ARROW));
	fSelectMenu->AddItem(new BMenuItem(_T("Next Track"),new BMessage(M_SELECT_NEXT),B_DOWN_ARROW));
	fSelectMenu->AddSeparatorItem();
	fSelectMenu->AddItem(new BMenuItem(_T("Invert"),new BMessage(M_SELECT_INVERT),'I'));

	bar->AddItem(fSelectMenu);
	
	
	r.left = 5;
	r.top = r.bottom+10;
	r.right = (Bounds().right * .65) - 5;
	r.bottom = r.top + 20;

	fArtist = new AutoTextControl(r,"artist",_T("Artist: "),"",
			new BMessage(M_ARTIST_CHANGED),B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	fArtist->SetDivider(fArtist->StringWidth(_T("Album: "))+5);
	AddChild(fArtist);
	
	r.left = (Bounds().right * .65) + 5;
	r.right = Bounds().right - 5;
	
	fGenre = new AutoTextControl(r,"genre",_T("Genre: "), "", new BMessage(M_GENRE_CHANGED),
			B_FOLLOW_TOP | B_FOLLOW_RIGHT);
	fGenre->SetDivider(fGenre->StringWidth(_T("Genre: "))+5);
	
	AddChild(fGenre);
	
	r.OffsetBy(0,r.Height()+10);
	
	if(gCDDrive.CountDrives()>1)
	{
		BString currentdrive = _T("Current Drive: ");
		currentdrive << gCDDrive.GetDrive()+1;
		
		r.left = fGenre->Frame().left;
		fCurrentDrive = new BStringView(r,"currentdrive",currentdrive.String(), B_FOLLOW_RIGHT |
										B_FOLLOW_TOP);
		AddChild(fCurrentDrive);
		
		r.right = fArtist->Frame().right;
		r.left = 5;
	}
	else
	{
		r.left = 5;
	}
	#ifdef SYS_ZETA
	fAlbum = new AutoTextControl(r,"album",_T("Album: "),"", new BMessage(M_ALBUM_CHANGED),
							B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	fAlbum->SetDivider(fAlbum->StringWidth("Album: ")+5);
	#else
	fAlbum = new AutoTextControl(r,"album","Album: ","", new BMessage(M_ALBUM_CHANGED),
							B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	fAlbum->SetDivider(fAlbum->StringWidth("Album: ")+5);

	#endif
	AddChild(fAlbum);
	
	r.right = Bounds().right - 5;
	fGoButton = new BButton(BRect(0,0,1,1),"go",_T("Go"),new BMessage(M_START_ENCODING),
				B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	fGoButton->ResizeToPreferred();
	fGoButton->MoveTo(Bounds().right - fGoButton->Bounds().Width() - 5,
						Bounds().bottom - fGoButton->Bounds().Height() - 5);
	fGoButton->MakeDefault(true);

	fSelectAll = new BButton(BRect(0,0,1,1),"selectall",_T("Select All"),
				new BMessage(M_SELECT_ALL),	B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
	fSelectAll->ResizeToPreferred();
	fSelectAll->MoveTo(5,Bounds().bottom - fSelectAll->Bounds().Height() - 5);
	
	r.bottom = fGoButton->Frame().top - 10;
	r.top = r.bottom - 20;

	fTrackEditor = new AutoTextControl(r,"track",_T("Track: "),"",
			new BMessage(M_TRACK_CHANGED),B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	
	// Set divider to width of 'Album' to align the left side of the boxes
	fTrackEditor->SetDivider(fTrackEditor->StringWidth(_T("Album: "))+5);
	r.bottom = r.top - 10 - B_H_SCROLL_BAR_HEIGHT;
	r.top = fAlbum->Frame().bottom + 10;
	r.right -= B_V_SCROLL_BAR_WIDTH;
	fTrackList = new BListView(r,"tracklist",B_MULTIPLE_SELECTION_LIST,
				B_FOLLOW_ALL);
	fTrackList->SetSelectionMessage(new BMessage(M_CHOOSE_TRACK));
	fTrackList->SetInvocationMessage(new BMessage(M_START_ENCODING));
	BScrollView *scroller = new BScrollView("scroller",fTrackList,
							B_FOLLOW_ALL,0,false,true);
	
	r=fSelectAll->Frame();
	r.right = fGoButton->Frame().left-5;
	r.left = fSelectAll->Frame().right+5;
	
	fDriveInUse = new BStringView(r,"driveinuse",_T("This CD Drive is busy"),
								B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	AddChild(fDriveInUse);
	fDriveInUse->SetHighColor(0,0,255);
	fDriveInUse->SetAlignment(B_ALIGN_CENTER);
	fDriveInUse->Hide();
	
	fLookupInProgress = new BStringView(r,"lookupinprogress",_T("Looking up CD in database"),
										B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	AddChild(fLookupInProgress);
	fLookupInProgress->SetHighColor(86,137,86);
	fLookupInProgress->SetAlignment(B_ALIGN_CENTER);
	fLookupInProgress->Hide();
	
	// Because we do layout from the bottom up, we need to save AddChild for
	// last so that the keyboard navigation order is correct
	
	AddChild(scroller);	
	AddChild(fTrackEditor);
	AddChild(fSelectAll);
	AddChild(fGoButton);
}

void MainView::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case M_SELECT_ALL:
		{
			if(fTrackList->CountItems()>0)
				fTrackList->Select(0,fTrackList->CountItems()-1);
			break;
		}
		case M_SELECT_INVERT:
		{
			for(int32 i=0;i<fTrackList->CountItems(); i++)
			{
				BStringItem *item = (BStringItem*) fTrackList->ItemAt(i);
				if(item->IsSelected())
					fTrackList->Deselect(i);
				else
					fTrackList->Select(i,true);
			}
			
			break;
		}
		case M_SELECT_PREVIOUS:
		{
			if(fTrackList->CountItems()>0)
			{
				int32 index = fTrackList->CurrentSelection();
				if(index>0)
				{
					fTrackList->Select(index-1);
					fTrackList->ScrollToSelection();
				}
				else
				if(index==0)
				{
					fTrackList->Select(0);
					fTrackList->ScrollToSelection();
				}
			}
			break;
		}
		case M_SELECT_NEXT:
		{
			if(fTrackList->CountItems()>0)
			{
				int32 index = fTrackList->CurrentSelection();
				if(index < fTrackList->CountItems()-1)
				{
					fTrackList->Select(index+1);
					fTrackList->ScrollToSelection();
				}
			}
			break;
		}
		case M_ARTIST_CHANGED:
		{
			BString artist = fArtist->Text();
			if(artist.CountChars()<1)
				break;
			
			if(RenameCDDBFile(artist.String(),gCDData.Album()))
			{
				gCDData.SetArtist(fArtist->Text());
				gCDData.Save();
			}
//			else
//				fArtist->SetText(gCDData.Artist());
			
			break;
		}
		case M_ALBUM_CHANGED:
		{
			BString album = fAlbum->Text();
			if(album.CountChars()<1)
				break;
			
			if(RenameCDDBFile(gCDData.Artist(),album.String()))
			{
				gCDData.SetAlbum(fAlbum->Text());
				gCDData.Save();
			}
//			else
//				fAlbum->SetText(gCDData.Album());
			break;
		}
		case M_TRACK_CHANGED:
		{
			UpdateTrackList();
			break;
		}
		case M_GENRE_CHANGED:
		{
			gCDData.SetGenre(fGenre->Text());
			break;
		}
		case M_CHOOSE_TRACK:
		{
			int32 selection = fTrackList->CurrentSelection();
			
			BStringItem *stritem = (BStringItem*)fTrackList->ItemAt(selection);
			if(stritem)
				fTrackEditor->SetText(stritem->Text());
			else
				fTrackEditor->SetText("");
			
			for(uint16 i=0; i<gTrackList.CountItems(); i++)
			{
				BStringItem *stritem = (BStringItem*)fTrackList->ItemAt(i);
				*(gTrackList.ItemAtFast(i)) = stritem->IsSelected();
			}

			fTrackEditor->TextView()->MakeFocus(true);
			fTrackEditor->TextView()->SelectAll();
			break;
		}
		case M_SELECT_DRIVE:
		{
			int16 drive;
			if(msg->FindInt16("drive",&drive)==B_OK)
			{
				gCDDrive.SetDrive(drive);
				BString currentdrive = _T("Current Drive: ");
				currentdrive << gCDDrive.GetDrive()+1;
				fCurrentDrive->SetText(currentdrive.String());
			}
			break;
		}
		default:
		{
			BView::MessageReceived(msg);
			break;		
		}
	}
}

void MainView::FindOtherInstances(void)
{	
	BList list;
	app_info ai;
	
	be_app->GetAppInfo(&ai);
	be_roster->GetAppList(SV_SIGNATURE,&list);
	
	if(list.CountItems()<2)
		return;
	
	uint8 drivecount = gCDDrive.CountDrives();
	if(drivecount<1)
		return;
	
	bool useddrives[drivecount];
	for(uint8 i=0; i<drivecount; i++)
		useddrives[i] = false;
	
	for(int32 i=0; i<list.CountItems();i++)
	{
		team_id team = (team_id)list.ItemAt(i);
		
		if(ai.team == team)
			continue;
		
		// if we got this far, it means that there is another instance running.
		// This means we need to query the app for what drive it's using
		status_t error;
		BMessenger msgr(SV_SIGNATURE,team,&error);
		if(error!=B_OK)
		{
			printf("Couldn't construct messenger to remote SV instance.\n");
			continue;
		}
		BMessage msg(M_QUERY_DRIVE), reply;
		msgr.SendMessage(&msg,&reply, 3000000);
		
		if(reply.what==M_DRIVE_IN_USE)
		{
			int8 drive;
			if(reply.FindInt8("drive",&drive)==B_OK)
			{
				if(drive>=0)
					useddrives[drive]=true;
			}
		}
	}
	
	for(uint8 i=0; i<drivecount; i++)
	{
		if(!useddrives[i])
		{
			gCDDrive.SetDrive(i);
			break;
		}
	}
}

bool MainView::RenameCDDBFile(const char *newartist,const char *newalbum)
{
	if(!newartist || !newalbum)
		return false;
	
	CDDBData data;
	if(!fCDQuery.GetData(&data,1000000))
		return false;
	
	BPath path;
	if(find_directory(B_USER_DIRECTORY, &path, true)!=B_OK)
		return false;
	
	path.Append("cd");
	create_directory(path.Path(), 0755);
	
	BString filename(path.Path());
	filename << "/" << gCDData.Artist() << " - " << gCDData.Album();
	
	if(filename.Compare("Artist")==0)
		filename << "." << gCDData.DiscID();
	
	BEntry entry(filename.String(),B_READ_WRITE);
	
	filename.SetTo(path.Path());
	filename << "/" << newartist << " - " << newalbum;
	
	if(entry.Rename(filename.String())==B_OK)
		return true;
	
	return false;
}


MainWindow::MainWindow(const BRect &frame)
 : BWindow(frame,"SimplyVorbis",B_TITLED_WINDOW,B_ASYNCHRONOUS_CONTROLS |
 			B_NOT_ZOOMABLE | B_PULSE_NEEDED | B_NOT_ANCHORED_ON_ACTIVATE)
{
	LoadPreferences(PREFERENCES_PATH);
	
	fSound=NULL;
	
	float wmin,wmax,hmin,hmax;
	
	GetSizeLimits(&wmin,&wmax,&hmin,&hmax);
	wmin=400;
	hmin=300;
	SetSizeLimits(wmin,wmax,hmin,hmax);
	
	SetPulseRate(10000);
	
	BList formatlist,codeclist;
	GetCDDAFormats(&formatlist);

	media_file_format *format=NULL;
	for (int32 i = 0; (format = (media_file_format*)formatlist.ItemAt(i)); i++) {
		if (strcmp(format->short_name, "mp3") == 0)
			gMP3Format = format;
	}
	
	GetCDDACodecs(&codeclist);

	media_codec_info *codec=NULL;
	for (int32 i = 0; (codec = (media_codec_info*)codeclist.ItemAt(i)); i++) {
		if (strcmp(codec->short_name, "mp3") == 0)
			gMP3Codec = codec;
	}

	prefsLock.Lock();
	int16 drive;
	if(preferences.FindInt16("drive",&drive)==B_OK)
		gCDDrive.SetDrive(drive);
	
	prefsLock.Unlock();
	
	fMainView = new MainView(Bounds());
	AddChild(fMainView);

	fRipView = new RipView(Bounds(),&gCDDrive);
	AddChild(fRipView);
	fRipView->Hide();
}

MainWindow::~MainWindow(void)
{
	delete fSound;
}

bool MainWindow::QuitRequested(void)
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void MainWindow::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case M_EDIT_KEY:
		{
			// we receive this message when the user uses an editing key, such as
			// backspace or delete. The reason we do this is to allow the textbox
			// filter to let the editing key pass through and still save the changes
			// to disk
			int32 command;
			if(msg->FindInt32("command",&command)==B_OK)
				PostMessage(command, fMainView);
			break;
		}
		case M_SAVE_CHANGES:
		{
			BPath path;
			if(find_directory(B_USER_DIRECTORY, &path, true)!=B_OK)
				return;
			
			path.Append("cd");
			create_directory(path.Path(), 0755);
			
			BString filename(path.Path());
			filename << "/" << gCDData.Artist() << " - " << gCDData.Album();
			
			if(filename.Compare("Artist")==0)
				filename << "." << gCDData.DiscID();
			
			gCDData.Save(filename.String());
			break;
		}
		case M_START_ENCODING:
		{
			fMainView->Hide();
			fRipView->Show();
			fRipView->Go();
			break;
		}
		case M_STOP_ENCODING:
		{
			fRipView->Stop();
			fRipView->Hide();
			fMainView->Show();
			
			NotifyUser();
			break;
		}
		case M_SHOW_PREFERENCES:
		{
			// prevent changes to preferences while ripping
			if(fMainView->IsHidden())
				break;
			
			// show only one preferences window, not multiple
			int32 wincount=be_app->CountWindows();
			for(int32 i=0; i<wincount; i++)
			{
				BWindow *win = be_app->WindowAt(i);
				if(strcmp(win->Title(),"Preferences")==0)
					break;
			}
			
			BRect rect(Frame());
			rect.bottom = rect.top + 265;
			rect.right = rect.left + 400;
			PrefsWindow *pwin = new PrefsWindow(rect);
			pwin->Show();
			break;
		}
		case M_SHOW_ABOUT:
		{
			AboutWindow *abwin = new AboutWindow();
			abwin->Show();
			break;
		}
		case M_SHOW_LIBRARY:
		{
			BString librarypath, cmdstring;
			prefsLock.Lock();
			if(preferences.FindString("path",&librarypath) != B_OK)
				librarypath = "/boot/home/music";
			prefsLock.Unlock();
			
			BEntry entry(librarypath.String(),B_READ_ONLY);
			if(!entry.Exists())
				create_directory(librarypath.String(), 0755);
			
			librarypath.CharacterEscape("<> '\"\\|?[]{}():;`,",'\\');
			cmdstring = "/boot/beos/system/Tracker ";
			cmdstring << librarypath << " &";
			system(cmdstring.String());
			break;
		}
		default:
		{
			BWindow::MessageReceived(msg);
			break;
		}
	}
}

void MainWindow::NotifyUser(void)
{
	prefsLock.Lock();
	
	bool ejectcd;
	if(preferences.FindBool("eject",&ejectcd)!=B_OK)
		ejectcd=false;
	
	bool activatewin;
	if(preferences.FindBool("notify_show",&activatewin)!=B_OK)
		activatewin=true;
	
	bool playsound;
	if(preferences.FindBool("notify_sound",&playsound)!=B_OK)
		playsound=false;
	
	BString playpath;
	if(preferences.FindString("notify_sound_path",&playpath)!=B_OK)
		playpath="";
	
	prefsLock.Unlock();
	
	if(activatewin)
	{
		if(IsMinimized())
			Show();
		if(!IsActive())
			Activate();
	}
	
	if(playsound)
	{
		if(playpath.CountChars()<1)
			beep();
		else
		{
			BEntry entry(playpath.String(),B_READ_ONLY);
			if(entry.InitCheck()!=B_OK)
				beep();
			else
			{
				delete fSound;
				
				entry_ref ref;
				entry.GetRef(&ref);
				fSound = new BFileGameSound(&ref,false);
				if(fSound->InitCheck()==B_OK)
					fSound->StartPlaying();
				else
				{
					delete fSound;
					fSound=NULL;
					beep();
				}
			}
		}
	}
	
	if(ejectcd)
		gCDDrive.Eject();
	
}

void QueryBusyDrives(const uint16 &index)
{
	BList teamlist;
	be_roster->GetAppList("application/x-vnd.wgp-SimplyVorbis",&teamlist);
	
	for(int32 i=0; i<teamlist.CountItems(); i++)
	{
		team_id *team = (team_id*)teamlist.ItemAt(i);
		if(*team != gAppInfo.team)
		{
			// not this instance of the app, so ask the team what drive it's using
			BMessenger msgr("application/x-vnd.wgp-SimplyVorbis",*team);
			BMessage msg(M_QUERY_DRIVE);
			msgr.SendMessage(&msg);
		}
	}
}
