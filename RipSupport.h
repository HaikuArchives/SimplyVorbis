#ifndef RIPSUPPORT_H
#define RIPSUPPORT_H

#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <scsi.h>
#include <Entry.h>
#include <List.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaFormats.h>
#include <MediaTrack.h>
#include <Message.h>
#include "CDAudioDevice.h"

//#define FAKE_RIPPING

void make_cdda_format(media_format* format);
cdaudio_time GetTimeRipped(void);
void GetCDDAFormats(BList *list);
void GetCDDACodecs(BList *list);
status_t ConvertTrack(const char *device, const char *outfile, uint16 tracknum,
					const media_file_format &format, const media_codec_info &codec,
					BMessenger *updater=NULL, sem_id abort_semaphore=-1);
status_t VorbifyTrack(const char *device, const char *outfilepath, uint16 tracknum, BMessenger *updater=NULL,
						sem_id abort_semaphore=-1);

#endif
