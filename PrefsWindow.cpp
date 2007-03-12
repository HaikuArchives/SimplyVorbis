#include <Locker.h>
#include <MenuItem.h>
#include <Menu.h>
#include <Message.h>
#include <Messenger.h>
#include <View.h>
#include <Alert.h>
#include <Entry.h>
#include <Path.h>
#include <Box.h>

#ifdef B_ZETA_VERSION_VENTURE // this is Zeta R1.X (Fungerar med R1.1)
#define SYS_ZETA
#endif

#ifdef SYS_ZETA
#include <locale/Locale.h>
#include <locale/Formatter.h>
#else
#define _T(x) x
#endif

#include "MainWindow.h"
#include "PrefsWindow.h"
#include "Preferences.h"

enum
{
	M_DRIVE_CHANGED,
	M_PATH_CHANGED,
	M_EJECT_CHANGED,
	M_NOTIFY_SHOW_CHANGED,
	M_NOTIFY_SOUND_CHANGED,
	M_NOTIFY_SOUND_PATH_CHANGED,
	M_BROWSE_SOUND_PATH,
	M_SOUND_FILE_CHOSEN,
	M_MAKE_PLAYLIST_CHANGED,
	
	M_TRACKNAME_CHANGED,
	M_FOLDERMODE_CHANGED,
	M_FORMAT_CHANGED,
	
	M_BROWSE,
	M_FOLDER_CHOSEN
};

extern CDAudioDevice gCDDrive;

class FolderRefFilter : public BRefFilter
{
public:
	bool Filter(const entry_ref *ref, BNode *node, struct stat *st, const char *filetype);
};

bool FolderRefFilter::Filter(const entry_ref *ref, BNode *node, struct stat *st, const char *filetype)
{
	return node->IsDirectory();
}

PrefsWindow::PrefsWindow(const BRect &rect)
 : 	BWindow(rect,"Preferences",B_TITLED_WINDOW,B_NOT_RESIZABLE | B_NOT_ZOOMABLE |
 			B_ASYNCHRONOUS_CONTROLS)
{
	BView *view = new BView(Bounds(),"bgview",B_FOLLOW_ALL,B_WILL_DRAW);
	view->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(view);
	
	BRect r;
	BMenu *menu;
	
	r.Set(5,5, (Bounds().right / 2) - 5, 25);
	
	int16 drivecount = fCD.CountDrives();
	if( drivecount > 1)
	{
		ResizeBy(0,35);
		menu = new BMenu(_T("CD Drives"));
		menu->SetLabelFromMarked(true);
		menu->SetRadioMode(true);
		
		fDriveList = new BMenuField(r,"drivelist",_T("Default CD Drive: "),menu);
		fDriveList->SetDivider(fDriveList->StringWidth(_T("Default CD Drive: "))+5);
		view->AddChild(fDriveList);
		
		for(int32 i=0; i<fCD.CountDrives(); i++)
		{
			BString string;
			string << i+1;
			
			menu->AddItem(new BMenuItem(string.String(), new BMessage(M_DRIVE_CHANGED)));
		}
		
		if(menu->CountItems()>0)
		{
			prefsLock.Lock();
			int16 drivenum=0;
			if(preferences.FindInt16("drive",&drivenum)==B_OK)
				menu->ItemAt(drivenum)->SetMarked(true);
			else
				menu->ItemAt(0L)->SetMarked(true);
			prefsLock.Unlock();
		}
		
		fDriveList->ResizeToPreferred();
		
		r.left = MAX(fDriveList->Frame().right + 20, Bounds().Width()/2);
		r.left = MIN(r.left, (Bounds().right-5) - view->StringWidth(_T("Make play lists")));
		r.right = Bounds().right - 5;
	}
	
	fPlaylistBox = new BCheckBox(r,"playlistbox",_T("Make play lists"),new BMessage(M_MAKE_PLAYLIST_CHANGED));

	view->AddChild(fPlaylistBox);
	fPlaylistBox->ResizeToPreferred();
	if(drivecount > 1)
		fPlaylistBox->SetResizingMode(B_FOLLOW_RIGHT | B_FOLLOW_TOP);
	r.OffsetBy(0, r.Height() + 15);
	r.left=5;
	
	prefsLock.Lock();
	bool make_playlist;
	if(preferences.FindBool("makeplaylist",&make_playlist)!=B_OK)
		make_playlist=true;
	prefsLock.Unlock();
	
	if(make_playlist)
		fPlaylistBox->SetValue(B_CONTROL_ON);
	
	fBrowseButton = new BButton(BRect(0,0,1,1),"browse",_T("Browse…"),new BMessage(M_BROWSE));
	fBrowseButton->ResizeToPreferred();
	fBrowseButton->MoveTo(Bounds().right - 5 - fBrowseButton->Bounds().Width(), r.top);
	
	r.right = fBrowseButton->Frame().left - 10;
	fMP3Path = new BTextControl(r,"path",_T("Song Folder: "), "", new BMessage(M_PATH_CHANGED));
	fMP3Path->SetDivider(fMP3Path->StringWidth(_T("Song Folder: ")) + 5);
	view->AddChild(fMP3Path);
	view->AddChild(fBrowseButton);
	
	prefsLock.Lock();
	BString mp3path;
	if(preferences.FindString("path",&mp3path)!=B_OK)
		mp3path = "/boot/home/music";
	prefsLock.Unlock();
	fMP3Path->SetText(mp3path.String());
	
	entry_ref ref;
	BEntry entry(mp3path.String());
	BEntry parent;
	entry.GetParent(&parent);
	parent.GetRef(&ref);
	fFilePanel = new BFilePanel(B_OPEN_PANEL,new BMessenger(this),&ref,B_DIRECTORY_NODE,false,
								new BMessage(M_FOLDER_CHOSEN),new FolderRefFilter());
	fFilePanel->Window()->SetTitle(_T("Choose MP3 Folder"));
	
	// Name format
	r.OffsetBy(0, r.Height() + 25);
	r.right = Bounds().right - 5;
	
	float dividersize = MAX(view->StringWidth(_T("Song Name Style"))+10,
					view->StringWidth(_T("Song Organization: "))+10);
	
	int16 nameMode;
	
	prefsLock.Lock();
	if(preferences.FindInt16("namestyle",&nameMode)!=B_OK)
		nameMode = 0;
	prefsLock.Unlock();
	
	menu=new BMenu(_T("Song Name Style"));
	menu->AddItem(new BMenuItem(_T("Artist - Song"), new BMessage(M_TRACKNAME_CHANGED)));
	menu->AddItem(new BMenuItem(_T("Album - Song"), new BMessage(M_TRACKNAME_CHANGED)));
	menu->AddItem(new BMenuItem(_T("Artist - Album - Song"), new BMessage(M_TRACKNAME_CHANGED)));
	menu->SetLabelFromMarked(true);
	menu->SetRadioMode(true);
	menu->ItemAt(nameMode)->SetMarked(true);
	fTrackNameField = new BMenuField(r, "namestyle", _T("Song Name Style: "),menu);
	
	fTrackNameField->SetDivider(dividersize);
	view->AddChild(fTrackNameField);
	
	// Rip format
	
	r.OffsetBy(0,r.Height()+10);
	
	bool usemp3;
	prefsLock.Lock();
	if(preferences.FindBool("usemp3",&usemp3)!=B_OK)
		usemp3 = false;
	prefsLock.Unlock();
	
	menu=new BMenu(_T("Format"));
	menu->AddItem(new BMenuItem(_T("Ogg Vorbis"), new BMessage(M_FORMAT_CHANGED)));
	menu->AddItem(new BMenuItem(_T("MP3"), new BMessage(M_FORMAT_CHANGED)));
	menu->SetLabelFromMarked(true);
	menu->SetRadioMode(true);
	if(usemp3)
		menu->ItemAt(1)->SetMarked(true);
	else
		menu->ItemAt(0)->SetMarked(true);
	
	fFormatField = new BMenuField(r, "format", _T("File Type: "),menu);
	fFormatField->SetDivider(dividersize);
	view->AddChild(fFormatField);
	
	if(!gMP3Format || !gMP3Codec)
	{
		menu->ItemAt(0)->SetMarked(true);
		fFormatField->SetEnabled(false);
		
		prefsLock.Lock();
		preferences.RemoveData("usemp3");
		preferences.AddBool("usemp3",false);
		prefsLock.Unlock();
	}
	
	// Method for organizing the files
	r.OffsetBy(0, r.Height() + 10);
	r.right = Bounds().right - 5;
	
	int16 folderMode;
	
	prefsLock.Lock();
	if(preferences.FindInt16("foldermode",&folderMode)!=B_OK)
		folderMode = 1;
	prefsLock.Unlock();
	
	menu=new BMenu(_T("Organization Style"));
	menu->AddItem(new BMenuItem(_T("Don't Group Songs"), new BMessage(M_FOLDERMODE_CHANGED)));
	menu->AddItem(new BMenuItem(_T("Group Songs by Artist"), new BMessage(M_FOLDERMODE_CHANGED)));
	menu->AddItem(new BMenuItem(_T("Group Songs by Album"), new BMessage(M_FOLDERMODE_CHANGED)));
	menu->AddItem(new BMenuItem(_T("Group Songs by Artist and Album"), new BMessage(M_FOLDERMODE_CHANGED)));
	menu->SetLabelFromMarked(true);
	menu->SetRadioMode(true);
	menu->ItemAt(folderMode)->SetMarked(true);
	
	fGroupField = new BMenuField(r, "groupmode", _T("Song Organization: "),menu);
	fGroupField->SetDivider(dividersize);
	view->AddChild(fGroupField);

	r.OffsetBy(0, r.Height() + 15);
	
	// Notification options
	
	r.bottom=Bounds().bottom-5;
	BBox *notifyBox = new BBox(r,"notifybox",B_FOLLOW_ALL,B_WILL_DRAW);
	view->AddChild(notifyBox);
	notifyBox->SetLabel(_T("When Finished"));
	r=notifyBox->Bounds().InsetByCopy(10,10);
	r.top+=5;
	
	fEjectBox = new BCheckBox(r,"ejectbox",_T("Eject the CD"),new BMessage(M_EJECT_CHANGED));
	notifyBox->AddChild(fEjectBox);
	fEjectBox->ResizeToPreferred();
	
	prefsLock.Lock();
	bool ejectdisc;
	if(preferences.FindBool("eject",&ejectdisc)!=B_OK)
		ejectdisc=false;
	prefsLock.Unlock();
	
	if(ejectdisc)
		fEjectBox->SetValue(B_CONTROL_ON);
	
	r.left = MAX(fEjectBox->Frame().right + 10, notifyBox->Bounds().Width()/2);
	r.left = MIN(r.left, (notifyBox->Bounds().right-10) - view->StringWidth(_T("Switch to SimplyVorbis")));
	fNotifyShow = new BCheckBox(r,"showbox",_T("Switch to SimplyVorbis"),new BMessage(M_NOTIFY_SHOW_CHANGED));
	fNotifyShow->ResizeToPreferred();
	
	if(fNotifyShow->Frame().right > notifyBox->Frame().right)
	{
		// We have an issue: the font settings are mega-cranked up. We'll handle the situation by resizing
		// the window
		ResizeBy(fNotifyShow->Frame().right - notifyBox->Frame().right + 15, 0);
	}
	notifyBox->AddChild(fNotifyShow);
	

	prefsLock.Lock();
	bool notifyshow;
	if(preferences.FindBool("notify_show",&notifyshow)!=B_OK)
		notifyshow=true;
	prefsLock.Unlock();
	
	if(notifyshow)
		fNotifyShow->SetValue(B_CONTROL_ON);
	
	r=fEjectBox->Frame();
	r.right = notifyBox->Bounds().Width() - 10;
	r.OffsetBy(0,r.Height() + 10);
	fNotifySound = new BCheckBox(r,"notifysound",_T("Play a Sound"),new BMessage(M_NOTIFY_SOUND_CHANGED));
	notifyBox->AddChild(fNotifySound);
	fNotifySound->ResizeToPreferred();
	
	prefsLock.Lock();
	bool notifysound;
	if(preferences.FindBool("notify_sound",&notifysound)!=B_OK)
		notifysound=true;
	prefsLock.Unlock();
	
	if(notifysound)
		fNotifySound->SetValue(B_CONTROL_ON);
	
	r.OffsetBy(0,r.Height() + 10);
	fSoundBrowseButton = new BButton(BRect(0,0,1,1),"browse",_T("Browse"),new BMessage(M_BROWSE_SOUND_PATH));
	fSoundBrowseButton->ResizeToPreferred();
	fSoundBrowseButton->MoveTo(notifyBox->Bounds().right - 10 - fSoundBrowseButton->Bounds().Width(), r.top);
	
	r.right = fSoundBrowseButton->Frame().left - 10;
	fSoundFilePath = new BTextControl(r,"soundfilepath",_T("Sound to Play: "), "", new BMessage(M_NOTIFY_SOUND_PATH_CHANGED));
	fSoundFilePath->SetDivider(fSoundFilePath->StringWidth(_T("Sound to Play: ")) + 5);
	
	notifyBox->AddChild(fSoundFilePath);
	notifyBox->AddChild(fSoundBrowseButton);
	
	if(fSoundBrowseButton->Frame().bottom > notifyBox->Bounds().bottom)
	{
		float resizeval = fSoundBrowseButton->Frame().bottom - notifyBox->Bounds().bottom+15;
		ResizeBy(0,resizeval);
	}
	
	prefsLock.Lock();
	BString soundfilepath;
	if(preferences.FindString("notify_sound_path",&soundfilepath)!=B_OK)
		soundfilepath = "";
	prefsLock.Unlock();
	fSoundFilePath->SetText(soundfilepath.String());
	
	if(!notifysound)
	{
		fSoundFilePath->SetEnabled(false);
		fSoundBrowseButton->SetEnabled(false);
	}
	entry.SetTo(soundfilepath.String());
	entry.GetParent(&parent);
	parent.GetRef(&ref);
	fSoundFilePanel = new BFilePanel(B_OPEN_PANEL,new BMessenger(this),&ref,B_FILE_NODE,false,
								new BMessage(M_SOUND_FILE_CHOSEN));
	fSoundFilePanel->Window()->SetTitle(_T("Choose Notify Sound"));
}

PrefsWindow::~PrefsWindow(void)
{
	delete fFilePanel;
}

void PrefsWindow::MessageReceived(BMessage *msg)
{
	BMenuItem *item;
	
	switch(msg->what)
	{
		case M_EJECT_CHANGED:
		{
			prefsLock.Lock();
			preferences.RemoveData("eject");
			preferences.AddBool("eject",(fEjectBox->Value() == B_CONTROL_ON));
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_MAKE_PLAYLIST_CHANGED:
		{
			prefsLock.Lock();
			preferences.RemoveData("makeplaylist");
			preferences.AddBool("makeplaylist",(fPlaylistBox->Value() == B_CONTROL_ON));
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_NOTIFY_SHOW_CHANGED:
		{
			prefsLock.Lock();
			preferences.RemoveData("notify_show");
			preferences.AddBool("notify_show",(fNotifyShow->Value() == B_CONTROL_ON));
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_NOTIFY_SOUND_CHANGED:
		{
			prefsLock.Lock();
			preferences.RemoveData("notify_sound");
			preferences.AddBool("notify_sound",(fNotifySound->Value() == B_CONTROL_ON));
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			
			if(fNotifySound->Value() == B_CONTROL_ON)
			{
				fSoundFilePath->SetEnabled(true);
				fSoundBrowseButton->SetEnabled(true);
			}
			else
			{
				fSoundFilePath->SetEnabled(false);
				fSoundBrowseButton->SetEnabled(false);
				if(fSoundFilePanel->IsShowing())
					fSoundFilePanel->Hide();
			}
			break;
		}
		case M_NOTIFY_SOUND_PATH_CHANGED:
		{
			BString path = fSoundFilePath->Text();
			if(path.CountChars()>1)
			{
				BEntry entry(path.String());
				if(!entry.Exists())
				{
					BString errmsg(_T("SimplyVorbis couldn't find the file "));
					errmsg << path << _T("'. Perhaps it might be easier to find it using the 'Browse' button.");
					BAlert *alert = new BAlert("SimplyVorbis",errmsg.String(),_T("OK"));
					alert->Go();
					break;
				}
				
				// do some reformatting before saving
				BPath fixedpath;
				entry.GetPath(&fixedpath);
				fSoundFilePath->SetText(fixedpath.Path());
			}
			
			prefsLock.Lock();
			preferences.RemoveData("notify_sound_path");
			preferences.AddString("notify_sound_path",fSoundFilePath->Text());
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_DRIVE_CHANGED:
		{
			item = fDriveList->Menu()->FindMarked();
			if(item)
			{
				int16 value = (int16)fDriveList->Menu()->IndexOf(item);
				prefsLock.Lock();
				preferences.RemoveData("drive");
				preferences.AddInt16("drive",value);
				prefsLock.Unlock();
				SavePreferences(PREFERENCES_PATH);
				
				gCDDrive.SetDrive(value);
			}
			break;
		}
		case M_PATH_CHANGED:
		{
			BString path = fMP3Path->Text();
			BEntry entry(path.String());
			if(!entry.Exists())
			{
				BString errmsg(_T("SimplyVorbis couldn't find the folder"));
				errmsg << path << _T(". Perhaps it might be easier to find the folder using the 'Browse' button.");
				BAlert *alert = new BAlert("SimplyVorbis",errmsg.String(),_T("OK"));
				alert->Go();
				break;
			}
			
			// do some reformatting before saving
			BPath fixedpath;
			entry.GetPath(&fixedpath);
			fMP3Path->SetText(fixedpath.Path());
			
			prefsLock.Lock();
			preferences.RemoveData("path");
			preferences.AddString("path",fMP3Path->Text());
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_FOLDER_CHOSEN:
		{
			entry_ref ref;
			if(msg->FindRef("refs",&ref)!=B_OK)
				break;
			BPath path(&ref);
			fMP3Path->SetText(path.Path());
			
			prefsLock.Lock();
			preferences.RemoveData("path");
			preferences.AddString("path",fMP3Path->Text());
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
 		case M_SOUND_FILE_CHOSEN:
		{
			entry_ref ref;
			if(msg->FindRef("refs",&ref)!=B_OK)
				break;
			BPath path(&ref);
			fSoundFilePath->SetText(path.Path());
			
			prefsLock.Lock();
			preferences.RemoveData("notify_sound_path");
			preferences.AddString("notify_sound_path",fSoundFilePath->Text());
			prefsLock.Unlock();
			SavePreferences(PREFERENCES_PATH);
			break;
		}
		case M_TRACKNAME_CHANGED:
		{
			item = fTrackNameField->Menu()->FindMarked();
			if(item)
			{
				int16 value = (int16)fTrackNameField->Menu()->IndexOf(item);
				prefsLock.Lock();
				preferences.RemoveData("namestyle");
				preferences.AddInt16("namestyle",value);
				prefsLock.Unlock();
				SavePreferences(PREFERENCES_PATH);
			}
			break;
		}
		case M_FOLDERMODE_CHANGED:
		{
			item = fGroupField->Menu()->FindMarked();
			if(item)
			{
				int16 value = (int16)fGroupField->Menu()->IndexOf(item);
				prefsLock.Lock();
				preferences.RemoveData("foldermode");
				preferences.AddInt16("foldermode",value);
				prefsLock.Unlock();
				SavePreferences(PREFERENCES_PATH);
			}
			break;
		}
		case M_FORMAT_CHANGED:
		{
			item = fFormatField->Menu()->FindMarked();
			if(item)
			{
				bool value = (strcmp(item->Label(),"MP3")==0);
				
				prefsLock.Lock();
				preferences.RemoveData("usemp3");
				preferences.AddBool("usemp3",value);
				prefsLock.Unlock();
				SavePreferences(PREFERENCES_PATH);
			}
			break;
		}
		case M_BROWSE:
		{
			fFilePanel->Show();
			break;
		}
		case M_BROWSE_SOUND_PATH:
		{
			fSoundFilePanel->Show();
			break;
		}
		default:
		{
			BWindow::MessageReceived(msg);
			break;
		}
	}
}

