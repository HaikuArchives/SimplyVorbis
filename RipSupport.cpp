#include "RipSupport.h"
#include "vorbisenc.h"
#include <stdlib.h>

//#define LOG_VORBIS
#ifdef LOG_VORBIS
	#define STRACE(x) printf x
#else
	#define STRACE(x) /* nothing */
#endif

static cdaudio_time sTimeRipped=0;

cdaudio_time GetTimeRipped(void)
{
	return sTimeRipped;
}

void make_cdda_format(media_format* format)
{
	format->type = B_MEDIA_RAW_AUDIO;

	format->u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
	format->u.raw_audio.channel_count = 2;
	format->u.raw_audio.frame_rate = 44100.0f;	// measured in Hertz
	format->u.raw_audio.byte_order = B_MEDIA_LITTLE_ENDIAN;

	format->u.raw_audio.buffer_size = 2358 * 2 * sizeof(int16);
}

void GetCDDAFormats(BList *list)
{
	if(!list)
		return;
	
	int32 token=0;
	
	media_file_format *format = new media_file_format;
	while(get_next_file_format(&token,format)==B_OK)
	{
		if(format->capabilities & media_file_format::B_KNOWS_ENCODED_AUDIO)
		{
			list->AddItem(format);
			format = new media_file_format;
		}
	}
	delete format;
}

void GetCDDACodecs(BList *list)
{
	if(!list)
		return;
	
	media_format informat, outformat;
	make_cdda_format(&informat);
	
	int32 token=0;
	media_codec_info *codecinfo= new media_codec_info;
	while(get_next_encoder(&token,NULL,&informat,&outformat,codecinfo)==B_OK)
	{
		list->AddItem(codecinfo);
		codecinfo= new media_codec_info;
	}
	delete codecinfo;
}

status_t ConvertTrack(const char *device, const char *outfile, uint16 tracknum,
					const media_file_format &format, const media_codec_info &codec,
					BMessenger *updater, sem_id abort_semaphore)
{
	int					id;
	int					i;
	int					length;
	int					start;
	long				req_track = 0;
	long				index;
	scsi_toc			toc;
	scsi_read_cd		read_cd;
	status_t			returnstatus=B_OK;
	
	// Attempt to open the CD device
	id = open(device, 0);
	if(id < 0)
		return B_FILE_ERROR;
	
	BEntry entry(outfile, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
	if(entry.InitCheck()!=B_OK)
		return entry.InitCheck();
	
	req_track = tracknum;
	
	// Read the TOC
	if(ioctl(id, B_SCSI_GET_TOC, &toc))
	{
		close(id);
		return B_IO_ERROR;
	}

	if (req_track > toc.toc_data[3])
	{
		close(id);
		return B_BAD_VALUE;
	}

	// Being that we could get the TOC and the track exists, we have a 
	// pretty good chance of success, so we should set up the the 
	// Media Kit stuff for writing to the encoded audio file.
	entry_ref ref;
	entry.GetRef(&ref);
	entry.Unset();

	BMediaFile mediafile(&ref, &format);
	status_t err = mediafile.InitCheck();
	if (err < B_OK)
		return err;
	
	err = mediafile.CommitHeader();
	if (err < B_OK)
		return err;
	
	media_format informat;
	make_cdda_format(&informat);
	
	BMediaTrack *mtrack = mediafile.CreateTrack(&informat, &codec);
	if (!mtrack)
		return B_ERROR;
	
	// Find the TOC data for the track in the struct
	index = 0;
	while (toc.toc_data[4 + (index * 8) + 2] != req_track)
		index++;
	
	// Obtain the starting and ending frames for the track
	start = (toc.toc_data[4 + (index * 8) + 5] * 60 * 75) +
			(toc.toc_data[4 + (index * 8) + 6] * 75) +
			 toc.toc_data[4 + (index * 8) + 7];
	index++;
	length = ((toc.toc_data[4 + (index * 8) + 5] * 60 * 75) +
			  (toc.toc_data[4 + (index * 8) + 6] * 75) +
			   toc.toc_data[4 + (index * 8) + 7]) - start;
	
	// We're going to read the track 1 second at a time
	uint64 frames = min_c(1 * 75, (int) length);
	
	// I wonder what how 2352 was calculated. 
	read_cd.buffer = (char *)malloc(frames * 2352);
	
	while (length)
	{
		// calculate the start and end frames for the SCSI READ_CD call
		index = start;
		read_cd.start_m = index / (60 * 75);
		index %= (60 * 75);
		read_cd.start_s = index / 75;
		index %= 75;
		read_cd.start_f = index;
		
		index = min_c((int)frames, length);
		read_cd.buffer_length = index * 2352;

		length -= index;
		start += index;
		
		read_cd.length_m = index / (60 * 75);
		index %= (60 * 75);
		read_cd.length_s = index / 75;
		index %= 75;
		read_cd.length_f = index;
		
		// Try to read the disc 5 times before giving up
#ifdef FAKE_RIPPING
		snooze(30000);
#else
		for (i = 0; i < 5; i++)
			if (ioctl(id, B_SCSI_READ_CD, &read_cd) == B_NO_ERROR)
				break;
#endif
		
		// Bail if we had problems
		if (i == 5)
		{
			// handle bad reads because of data track inaccuracy
			if(toc.toc_data[4 + (tracknum * 8) + 1] & 4)
			{
				cdaudio_time starttime, endtime;
				starttime.minutes = toc.toc_data[4 + ((tracknum-1) * 8) + 5];
				starttime.seconds = toc.toc_data[4 + ((tracknum-1) * 8) + 6];
				
				endtime.minutes = read_cd.start_m;
				endtime.seconds = read_cd.start_s;
				sTimeRipped = starttime - endtime;
				returnstatus = B_DEV_UNREADABLE;
				break;
			}
			
			mediafile.CloseFile();
			close(id);
			return B_DEV_UNREADABLE;
		}
		
		// Now we're going to swap the byte order of the data in the buffer.
		// I'm assuming that this is because the data read from CD is big-endian
		uchar* buf = (uchar*)read_cd.buffer;
		short tmp;
		for (i = 0; i < read_cd.buffer_length; i += 2)
		{
			tmp = (short)((buf[1] << 8) | buf[0]);
			*(short *)(&buf[0]) = tmp;
			buf += 2;
		}

		// flush the CDDA buffer to the Codec
		int64 codecBufferSize = informat.u.raw_audio.buffer_size;
		int64 cddaBufferSize = read_cd.buffer_length;
		buf = (uchar*)read_cd.buffer;
		while (cddaBufferSize > 0) {
			int64 sizeLeft = min_c(codecBufferSize, cddaBufferSize);
#ifndef FAKE_RIPPING
			int64 framesForSize = sizeLeft / (2 * sizeof(int16));
			mtrack->WriteFrames(buf, framesForSize);
#endif
			buf += sizeLeft;
			cddaBufferSize -= sizeLeft;
		}
		
		sem_info sinfo;
		if(get_sem_info(abort_semaphore,&sinfo)==B_OK)
		{
			if(sinfo.count<0)
			{
				mediafile.CloseFile();
				close(id);
				return B_INTERRUPTED;
			}
		}
		
		if(updater)
			updater->SendMessage(B_VALUE_CHANGED);
	}
	
//	mtrack->Flush();
	mediafile.CloseFile();

	close(id);

#ifdef FAKE_RIPPING
	entry.SetTo(outfile);
	entry.Remove();
#endif

	if(returnstatus==B_OK)
	{
		cdaudio_time starttime, endtime;
		starttime.minutes = toc.toc_data[4 + (tracknum * 8) + 5];
		starttime.seconds = toc.toc_data[4 + (tracknum * 8) + 6];
		
		endtime.minutes = read_cd.start_m;
		endtime.seconds = read_cd.start_s;
		sTimeRipped = endtime - starttime;
	}
	
	return returnstatus;
}

// This sucker is SimplyVorbis' own homegrown encoder taken from one of the examples in libVorbis and adapted
status_t VorbifyTrack(const char *device, const char *outfilepath, uint16 tracknum, BMessenger *updater,
						sem_id abort_semaphore)
{
	STRACE(("VorbifyTrack(device=%s,outfile=%s,tracknum=%d,updater=%p,abort_sem=%ld\n",device,outfilepath,
			tracknum,updater,abort_semaphore));
	FILE*				file;
	int					fileid;
	int					i;
	int					length;
	int					start;
	long				req_track = 0;
	long				index;
	scsi_toc			toc;
	scsi_read_cd		read_cd;
	status_t			returnstatus=B_OK;
	
	fileid = open(device, 0);
	if(fileid < 0)
		return B_FILE_ERROR;
	
	req_track = tracknum;
	
	// Read the TOC
	if(ioctl(fileid, B_SCSI_GET_TOC, &toc))
	{
		close(fileid);
		return B_IO_ERROR;
	}
	STRACE(("Successfully read SCSI table of contents\n"));
	
	if (req_track > toc.toc_data[3])
	{
		close(fileid);
		return B_BAD_VALUE;
	}
	
	file = fopen(outfilepath,"w");
	if(!file)
	{
		close(fileid);
		return B_FILE_ERROR;
	}
	STRACE(("Successfully opened out file\n"));
		
	// Find the TOC data for the track in the struct
	index = 0;
	while (toc.toc_data[4 + (index * 8) + 2] != req_track)
		index++;
	
	// Obtain the starting and ending frames for the track
	start = (toc.toc_data[4 + (index * 8) + 5] * 60 * 75) +
			(toc.toc_data[4 + (index * 8) + 6] * 75) +
			 toc.toc_data[4 + (index * 8) + 7];
	index++;
	length = ((toc.toc_data[4 + (index * 8) + 5] * 60 * 75) +
			  (toc.toc_data[4 + (index * 8) + 6] * 75) +
			   toc.toc_data[4 + (index * 8) + 7]) - start;
	
	// We're going to read the track 1 second at a time
	uint64 frames = min_c(1 * 75, (int) length);
	
	// I wonder what how 2352 was calculated. 
	read_cd.buffer = (char *)malloc(frames * 2352);
	
	ogg_stream_state streamstate; // weld physical pages into a logical stream of packets
	ogg_page         oggpage;	// one Ogg bitstream page.  Vorbis packets are inside
#ifndef FAKE_RIPPING
	ogg_packet       oggpacket;	// one raw packet of data for decode
#endif	
	vorbis_info      vorbinfo;		// struct that stores all the static vorbis bitstream settings
	vorbis_comment   vorbcomment;	// struct that stores all the user comments
	vorbis_dsp_state vorbdsp;		// central working state for the packet->PCM decoder
	vorbis_block     vorbblock;		// local working space for packet->PCM decode
	
	int endofstream=0,returncode;
	
	vorbis_info_init(&vorbinfo);
	STRACE(("vorbis_info_init\n"));
	
	// encode using .5 VBR quality - about the same as 128k bitrate
	returncode=vorbis_encode_init_vbr(&vorbinfo,2,44100,.5);
	STRACE(("vorbis_encode_init_vbr\n"));
	
	// bail if we have an error from vorbis init
	if(returncode)
		return(1);
	
	// add a comment
	vorbis_comment_init(&vorbcomment);
	STRACE(("vorbis_comment_init\n"));
	vorbis_comment_add_tag(&vorbcomment,"ENCODER","SimplyVorbis");
	STRACE(("vorbis_comment_add_tag\n"));
	
	// set up the analysis state and auxiliary encoding storage
	vorbis_analysis_init(&vorbdsp,&vorbinfo);
	STRACE(("vorbis_analysis_init\n"));
	vorbis_block_init(&vorbdsp,&vorbblock);
	STRACE(("vorbis_block_init\n"));
  
	// set up our packet->stream encoder
	// pick a random serial number; that way we can more likely build chained streams just by concatenation
	srand(time(NULL));
	ogg_stream_init(&streamstate,rand());
	STRACE(("ogg_stream_init\n"));
	
	/*
		Vorbis streams begin with three headers; the initial header (with
		most of the codec setup parameters) which is mandated by the Ogg
		bitstream spec.  The second header holds any comment fields.  The
		third header holds the bitstream codebook.  We merely need to
		make the headers, then pass them to libvorbis one at a time;
		libvorbis handles the additional Ogg bitstream constraints
	*/

	{
		ogg_packet header;
		ogg_packet comment_header;
		ogg_packet codebook_header;
		
		vorbis_analysis_headerout(&vorbdsp,&vorbcomment,&header,&comment_header,&codebook_header);
		ogg_stream_packetin(&streamstate,&header); // automatically placed in its own page
		ogg_stream_packetin(&streamstate,&comment_header);
		ogg_stream_packetin(&streamstate,&codebook_header);
		
		// This ensures the actual audio data will start on a new page, as per the Ogg spec
		while(!endofstream)
		{
			int result=ogg_stream_flush(&streamstate,&oggpage);
			if(result==0)
				break;
			fwrite(oggpage.header,1,oggpage.header_len,file);
			fwrite(oggpage.body,1,oggpage.body_len,file);
		}
	}
	STRACE(("Successfully wrote Ogg header\n"));
	
	while(length)
	{
		STRACE(("Begin read loop\n"));
		
		// calculate the start and end frames for the SCSI READ_CD call
		index = start;
		read_cd.start_m = index / (60 * 75);
		index %= (60 * 75);
		read_cd.start_s = index / 75;
		index %= 75;
		read_cd.start_f = index;
		
		index = min_c((int)frames, length);
		read_cd.buffer_length = index * 2352;

		length -= index;
		start += index;
		
		read_cd.length_m = index / (60 * 75);
		index %= (60 * 75);
		read_cd.length_s = index / 75;
		index %= 75;
		read_cd.length_f = index;
		
		// Try to read the disc 5 times before giving up
#ifdef FAKE_RIPPING
		snooze(50000);
#else
		for (i = 0; i < 5; i++)
		{
			if (ioctl(fileid, B_SCSI_READ_CD, &read_cd) == B_NO_ERROR)
				break;
			else
			{
				STRACE(("SCSI read failed. Trying again\n"));
			}
		}
#endif
		
		// Bail if we had problems
		if(i == 5)
		{
			// handle bad reads because of data track inaccuracy
			if(toc.toc_data[4 + (tracknum * 8) + 1] & 4)
			{
				cdaudio_time starttime, endtime;
				starttime.minutes = toc.toc_data[4 + ((tracknum-1) * 8) + 5];
				starttime.seconds = toc.toc_data[4 + ((tracknum-1) * 8) + 6];
				
				endtime.minutes = read_cd.start_m;
				endtime.seconds = read_cd.start_s;
				sTimeRipped = endtime - starttime;
				returnstatus=B_DEV_UNREADABLE;
				break;
			}
			
			STRACE(("Couldn't read CD. Aborting\n"));
			close(fileid);
			fclose(file);
			return B_DEV_UNREADABLE;
		}
		
		// In the code normally used for ripping, we swap the data from big-endian to little endian, but
		// we really don't need to do that here... at least I don't think we do. :P
		
		// write the CDDA buffer to the disk
		
#ifndef FAKE_RIPPING
		// expose the buffer to submit data
		float **buffer=vorbis_analysis_buffer(&vorbdsp,read_cd.buffer_length);
		STRACE(("vorbis_analysis_buffer\n"));
		
		// uninterleave samples
		for(i=0;i<read_cd.buffer_length/4;i++)
		{
			buffer[0][i]=((read_cd.buffer[i*4+1]<<8)| (0x00ff&(int)read_cd.buffer[i*4]))/32768.f;
			buffer[1][i]=((read_cd.buffer[i*4+3]<<8)| (0x00ff&(int)read_cd.buffer[i*4+2]))/32768.f;
		}
		STRACE(("Finished uninterleaving samples\n"));
		
		// tell the library how much we actually submitted
		vorbis_analysis_wrote(&vorbdsp,i);
		STRACE(("vorbis_analysis_wrote\n"));
	
		// vorbis does some data preanalysis, then divvies up blocks for more involved 
		// (potentially parallel) processing.  Get a single	block for encoding now
		while(vorbis_analysis_blockout(&vorbdsp,&vorbblock)==1)
		{
			STRACE(("vorbis_analysis_blockout loop\n"));
			
			// analysis, assume we want to use bitrate management
			vorbis_analysis(&vorbblock,NULL);
			STRACE(("vorbis_analysis\n"));
			vorbis_bitrate_addblock(&vorbblock);
			STRACE(("vorbis_bitrate_addblock\n"));
			
			while(vorbis_bitrate_flushpacket(&vorbdsp,&oggpacket))
			{
				STRACE(("vorbis_bitrate_flushpacket loop\n"));
				
				// weld the packet into the bitstream
				ogg_stream_packetin(&streamstate,&oggpacket);
				STRACE(("ogg_stream_packetin\n"));
				
				// write out pages (if any)
				while(!endofstream)
				{
					STRACE(("while not end of stream loop\n"));
					
					int result=ogg_stream_pageout(&streamstate,&oggpage);
					STRACE(("ogg_stream_packet\n"));
					if(result==0)
						break;
					fwrite(oggpage.header,1,oggpage.header_len,file);
					STRACE(("fwrite #1\n"));
					fwrite(oggpage.body,1,oggpage.body_len,file);
					STRACE(("fwrite #2\n"));
					
					// this could be set above, but for illustrative purposes, I do
					// it here (to show that vorbis does know where the stream ends)
					
					if(ogg_page_eos(&oggpage))
						endofstream=1;
					STRACE(("if ogg_page_eos\n"));
				}
				
			} // end while(vorbis_bitrate_flushpacket)
			
		} // end while(vorbis_analysis_blockout)	
#endif		
		
		sem_info sinfo;
		if(get_sem_info(abort_semaphore,&sinfo)==B_OK)
		{
			STRACE(("get_sem_info\n"));
			if(sinfo.count<0)
			{
				fclose(file);
				close(fileid);
				return B_INTERRUPTED;
			}
		}
		
		if(updater)
		{
			STRACE(("Update sent\n"));
			updater->SendMessage(B_VALUE_CHANGED);
		}
	}

#ifndef FAKE_RIPPING
	
	STRACE(("Entering final write\n"));
	/*
		end of file.  this can be done implicitly in the mainline,
		but it's easier to see here in non-clever fashion.
		Tell the library we're at end of stream so that it can handle
		the last frame and mark end of stream in the output properly
	*/
	vorbis_analysis_wrote(&vorbdsp,0);
	STRACE(("vorbis_analysis wrote\n"));
		
	// vorbis does some data preanalysis, then divvies up blocks for more involved 
	// (potentially parallel) processing.  Get a single	block for encoding now
	while(vorbis_analysis_blockout(&vorbdsp,&vorbblock)==1)
	{
		STRACE(("final vorbis_analysis_blockout loop\n"));
		// analysis, assume we want to use bitrate management
		vorbis_analysis(&vorbblock,NULL);
		STRACE(("vorbis_analysis\n"));
		vorbis_bitrate_addblock(&vorbblock);
		STRACE(("vorbis_bitrate_addblock\n"));
		
		while(vorbis_bitrate_flushpacket(&vorbdsp,&oggpacket))
		{
			STRACE(("final vorbis_bitrate_flushpacket loop\n"));
			
			// weld the packet into the bitstream
			ogg_stream_packetin(&streamstate,&oggpacket);
			STRACE(("ogg_stream_packetin\n"));
			
			// write out pages (if any)
			while(!endofstream)
			{
				STRACE(("final while not end of stream loop\n"));
				int result=ogg_stream_pageout(&streamstate,&oggpage);
				if(result==0)
					break;
				fwrite(oggpage.header,1,oggpage.header_len,file);
				STRACE(("fwrite #3\n"));
				
				fwrite(oggpage.body,1,oggpage.body_len,file);
				STRACE(("fwrite #4\n"));
				
				// this could be set above, but for illustrative purposes, I do
				// it here (to show that vorbis does know where the stream ends)
				
				if(ogg_page_eos(&oggpage))
					endofstream=1;
				STRACE(("ogg_page_eos\n"));
			}
			
		} // end while(vorbis_bitrate_flushpacket)
		
	} // end while(vorbis_analysis_blockout)
#endif
	
	close(fileid);
	fclose(file);
	STRACE(("files closed\n"));
	
#ifdef FAKE_RIPPING
	BEntry entry(outfilepath);
	entry.Remove();
#endif
	
	// clean up and exit.  vorbis_info_clear() must be called last
	ogg_stream_clear(&streamstate);
	vorbis_block_clear(&vorbblock);
	vorbis_dsp_clear(&vorbdsp);
	vorbis_comment_clear(&vorbcomment);
	vorbis_info_clear(&vorbinfo);
	STRACE(("finished clean up\n"));
	
	// ogg_page and ogg_packet structs always point to storage in
	// libvorbis.  They're never freed or manipulated directly
	
	if(returnstatus==B_OK)
	{
		cdaudio_time starttime, endtime;
		starttime.minutes = toc.toc_data[4 + (tracknum * 8) + 5];
		starttime.seconds = toc.toc_data[4 + (tracknum * 8) + 6];
		
		endtime.minutes = read_cd.start_m;
		endtime.seconds = read_cd.start_s;
		sTimeRipped = endtime - starttime;
	}
	
	return returnstatus;
}
