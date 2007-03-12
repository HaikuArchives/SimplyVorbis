#include <Application.h>
#include <Mime.h>
#include <String.h>
#include <Message.h>
#include <fs_index.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Roster.h>
#include <List.h>
#include "MainWindow.h"
#include "PrefsWindow.h"

class MyApp : public BApplication
{
public:
	MyApp(void);
	void InitFileTypes(void);
	void MessageReceived(BMessage *msg);
};

MyApp::MyApp(void)
 : BApplication(SV_SIGNATURE)
{
	InitFileTypes();
	MainWindow *win = new MainWindow(BRect(100,100,500,400));
	win->Show();
	GetAppInfo(&gAppInfo);
}

void MyApp::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case M_QUERY_DRIVE:
		{
			BMessage *reply = new BMessage(M_DRIVE_IN_USE);
			reply->AddInt8("drive",gCDDrive.GetDrive());
			msg->SendReply(reply);
			break;
		}
		default:
			BApplication::MessageReceived(msg);
	}
}

int main(void)
{
	MyApp app;
	app.Run();
	return 0;
}

void MyApp::InitFileTypes(void)
{
	BMimeType mime;
	bool update = false;
	
	mime.SetType("audio/x-vorbis");
	char tempstr[B_MIME_TYPE_LENGTH];
	
	mime.GetShortDescription(tempstr);
	if(strlen(tempstr) < 1)
		update = true;
	mime.SetShortDescription("Ogg Vorbis Audio File");
	
	mime.GetShortDescription(tempstr);
	if(strlen(tempstr) < 1 || update)
		update = true;
	mime.SetLongDescription("Ogg Vorbis Audio File");
	
	BMessage extmsg;
	mime.GetFileExtensions(&extmsg);
	if(extmsg.IsEmpty() || update)
	{
		update = true;
		extmsg.MakeEmpty();
		BString ext("ogg");
		extmsg.AddString("extensions",ext);
		ext="ogm";
		extmsg.AddString("extensions",ext);
		mime.SetFileExtensions(&extmsg);
	}
	
	BMessage msg;
	mime.GetAttrInfo(&msg);
	if(msg.IsEmpty() || update)
	{
		update = true;
		msg.MakeEmpty();
		msg.AddString("attr:public_name", "Artist");
		msg.AddString("attr:name", "Audio:Artist"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Album");
		msg.AddString("attr:name", "Audio:Album"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Title");
		msg.AddString("attr:name", "Audio:Title"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Track");
		msg.AddString("attr:name", "Audio:Track"); 
		msg.AddInt32("attr:type", B_INT32_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Year");
		msg.AddString("attr:name", "Audio:Year"); 
		msg.AddInt32("attr:type", B_INT32_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Comment");
		msg.AddString("attr:name", "Audio:Comment"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 40); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Genre");
		msg.AddString("attr:name", "Audio:Genre"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Rating");
		msg.AddString("attr:name", "Audio:Rating"); 
		msg.AddInt32("attr:type", B_INT32_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 15); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Playing Time");
		msg.AddString("attr:name", "Audio:Length"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 60); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		msg.AddString("attr:public_name", "Bitrate");
		msg.AddString("attr:name", "Audio:Bitrate"); 
		msg.AddInt32("attr:type", B_STRING_TYPE); 
		msg.AddBool("attr:viewable", true); 
		msg.AddBool("attr:editable", true); 
		msg.AddInt32("attr:width", 30); 
		msg.AddInt32("attr:alignment", B_ALIGN_LEFT); 
		msg.AddBool("attr:extra", false);
		
		mime.SetAttrInfo(&msg);
	}
	
	if(mime.GetPreferredApp(tempstr) != B_OK || strlen(tempstr) < 1)
	{
		// we will use whatever MP3 player the user uses for Ogg Vorbis. If the user
		// doesn't have a preferred one, then we won't worry about it, either.
		BMimeType mp3type("audio/x-mpeg");
		
		char mp3appstr[B_MIME_TYPE_LENGTH];
		mp3type.GetPreferredApp(mp3appstr);
		mime.SetPreferredApp(mp3appstr);
	}
	else
	if(update)
		mime.SetPreferredApp(tempstr);
	
	if(!mime.IsInstalled() || update)
		mime.Install();
	
	// TODO: Check for existence in the index on the volume containing the
	// music folder
	
	BVolume vol;
	BVolumeRoster().GetBootVolume(&vol);
	struct index_info iinfo;
	if(fs_stat_index(vol.Device(), "Audio:Artist", &iinfo) != 0)
	{
		fs_create_index(vol.Device(),"Audio:Artist",B_STRING_TYPE,0);
		fs_create_index(vol.Device(),"Audio:Album",B_STRING_TYPE,0);
		fs_create_index(vol.Device(),"Audio:Genre",B_STRING_TYPE,0);
		fs_create_index(vol.Device(),"Audio:Year",B_INT32_TYPE,0);
	}
}

