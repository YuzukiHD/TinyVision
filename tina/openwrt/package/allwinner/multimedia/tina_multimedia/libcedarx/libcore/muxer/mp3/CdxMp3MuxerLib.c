/*
 * Copyright (c) 2008-2022 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMp3MuxerLib.c
 * Description : Allwinner MP3 Muxer Definition
 * History :
 *   Author  : chengweipeng <chengweipeng@allwinnertech.com>
 *   Date    : 2022/07/20
 *   Comment : 创建初始版本
 */

#include "CdxMp3Muxer.h"
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#define WRITE_FREE_TAG (0)

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

const char *const ff_id3v1_genre_str[ID3v1_GENRE_MAX + 1] = {
	[0] = "Blues",
	[1] = "Classic Rock",
	[2] = "Country",
	[3] = "Dance",
	[4] = "Disco",
	[5] = "Funk",
	[6] = "Grunge",
	[7] = "Hip-Hop",
	[8] = "Jazz",
	[9] = "Metal",
	[10] = "New Age",
	[11] = "Oldies",
	[12] = "Other",
	[13] = "Pop",
	[14] = "R&B",
	[15] = "Rap",
	[16] = "Reggae",
	[17] = "Rock",
	[18] = "Techno",
	[19] = "Industrial",
	[20] = "Alternative",
	[21] = "Ska",
	[22] = "Death Metal",
	[23] = "Pranks",
	[24] = "Soundtrack",
	[25] = "Euro-Techno",
	[26] = "Ambient",
	[27] = "Trip-Hop",
	[28] = "Vocal",
	[29] = "Jazz+Funk",
	[30] = "Fusion",
	[31] = "Trance",
	[32] = "Classical",
	[33] = "Instrumental",
	[34] = "Acid",
	[35] = "House",
	[36] = "Game",
	[37] = "Sound Clip",
	[38] = "Gospel",
	[39] = "Noise",
	[40] = "AlternRock",
	[41] = "Bass",
	[42] = "Soul",
	[43] = "Punk",
	[44] = "Space",
	[45] = "Meditative",
	[46] = "Instrumental Pop",
	[47] = "Instrumental Rock",
	[48] = "Ethnic",
	[49] = "Gothic",
	[50] = "Darkwave",
	[51] = "Techno-Industrial",
	[52] = "Electronic",
	[53] = "Pop-Folk",
	[54] = "Eurodance",
	[55] = "Dream",
	[56] = "Southern Rock",
	[57] = "Comedy",
	[58] = "Cult",
	[59] = "Gangsta",
	[60] = "Top 40",
	[61] = "Christian Rap",
	[62] = "Pop/Funk",
	[63] = "Jungle",
	[64] = "Native American",
	[65] = "Cabaret",
	[66] = "New Wave",
	[67] = "Psychadelic",
	[68] = "Rave",
	[69] = "Showtunes",
	[70] = "Trailer",
	[71] = "Lo-Fi",
	[72] = "Tribal",
	[73] = "Acid Punk",
	[74] = "Acid Jazz",
	[75] = "Polka",
	[76] = "Retro",
	[77] = "Musical",
	[78] = "Rock & Roll",
	[79] = "Hard Rock",
	[80] = "Folk",
	[81] = "Folk-Rock",
	[82] = "National Folk",
	[83] = "Swing",
	[84] = "Fast Fusion",
	[85] = "Bebob",
	[86] = "Latin",
	[87] = "Revival",
	[88] = "Celtic",
	[89] = "Bluegrass",
	[90] = "Avantgarde",
	[91] = "Gothic Rock",
	[92] = "Progressive Rock",
	[93] = "Psychedelic Rock",
	[94] = "Symphonic Rock",
	[95] = "Slow Rock",
	[96] = "Big Band",
	[97] = "Chorus",
	[98] = "Easy Listening",
	[99] = "Acoustic",
	[100] = "Humour",
	[101] = "Speech",
	[102] = "Chanson",
	[103] = "Opera",
	[104] = "Chamber Music",
	[105] = "Sonata",
	[106] = "Symphony",
	[107] = "Booty Bass",
	[108] = "Primus",
	[109] = "Porn Groove",
	[110] = "Satire",
	[111] = "Slow Jam",
	[112] = "Club",
	[113] = "Tango",
	[114] = "Samba",
	[115] = "Folklore",
	[116] = "Ballad",
	[117] = "Power Ballad",
	[118] = "Rhythmic Soul",
	[119] = "Freestyle",
	[120] = "Duet",
	[121] = "Punk Rock",
	[122] = "Drum Solo",
	[123] = "A capella",
	[124] = "Euro-House",
	[125] = "Dance Hall",
	[126] = "Goa",
	[127] = "Drum & Bass",
	[128] = "Club-House",
	[129] = "Hardcore",
	[130] = "Terror",
	[131] = "Indie",
	[132] = "BritPop",
	[133] = "Negerpunk",
	[134] = "Polsk Punk",
	[135] = "Beat",
	[136] = "Christian Gangsta",
	[137] = "Heavy Metal",
	[138] = "Black Metal",
	[139] = "Crossover",
	[140] = "Contemporary Christian",
	[141] = "Christian Rock",
	[142] = "Merengue",
	[143] = "Salsa",
	[144] = "Thrash Metal",
	[145] = "Anime",
	[146] = "JPop",
	[147] = "SynthPop",
};

const char ff_id3v2_tags[][4] = {
	"TALB", "TBPM", "TCOM", "TCON", "TCOP", "TDEN", "TDLY", "TDOR", "TDRC",
	"TDRL", "TDTG", "TENC", "TEXT", "TFLT", "TIPL", "TIT1", "TIT2", "TIT3",
	"TKEY", "TLAN", "TLEN", "TMCL", "TMED", "TMOO", "TOAL", "TOFN", "TOLY",
	"TOPE", "TOWN", "TPE1", "TPE2", "TPE3", "TPE4", "TPOS", "TPRO", "TPUB",
	"TRCK", "TRSN", "TRSO", "TSOA", "TSOP", "TSOT", "TSRC", "TSSE", "TSST",
	{ 0 },
};

/***************************** Write Data to Stream File *****************************/
static cdx_int32 writeBufferStream(ByteIOContext *s, cdx_int8 *buf, cdx_int32 size)
{
#if FS_WRITER
	return s->fsWrite(s, (cdx_uint8 *)buf, size);
#else
	return s->cdxWrite(s, (cdx_uint8 *)buf, size);
#endif

}

static cdx_int32 writeByteStream(ByteIOContext *s, cdx_int32 b)
{
	return writeBufferStream(s, (cdx_int8 *)(&b), 1);
}

static cdx_int32 writeTagStream(ByteIOContext *s, const char *tag)
{
	cdx_int32 ret = 0;
	while (*tag) {
		ret = writeByteStream(s, *tag++);
		if (ret < 0) {
			break;
		}
	}
	return ret;
}

static cdx_int32 writeBe16Stream(ByteIOContext *s, cdx_uint32 val)
{
	cdx_int32 ret;
	if ((ret = writeByteStream(s, val >> 8)) < 0) {
		return ret;
	}
	ret = writeByteStream(s, val);
	return ret;
}

static cdx_int32 writeBe24Stream(ByteIOContext *s, cdx_uint32 val)
{
	cdx_int32 ret;
	if ((ret = writeBe16Stream(s, val >> 8)) < 0) {
		return ret;
	}
	ret = writeByteStream(s, val);
	return ret;
}

static cdx_int32 writeBe32Stream(ByteIOContext *s, cdx_uint32 val)
{
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
	val = (val >> 16) | (val << 16);
	return writeBufferStream(s, (cdx_int8 *)&val, 4);
}

static cdx_int32 writeLe32Stream(ByteIOContext *s, cdx_uint32 val)
{
	return writeBufferStream(s, (cdx_int8 *)&val, 4);
}

/***************************** Write Data to Stream File *****************************/

/* simple formats */

static void id3v2_put_size(Mp3MuxContext *s, int size)
{
#if FS_WRITER
	ByteIOContext *pb = s->fs_writer_info.mp_fs_writer;
#else
	ByteIOContext *pb = s->stream_writer;
#endif

	writeByteStream(pb, size >> 21 & 0x7f);
	writeByteStream(pb, size >> 14 & 0x7f);
	writeByteStream(pb, size >> 7  & 0x7f);
	writeByteStream(pb, size       & 0x7f);
}

static void id3v2_put_ttag(Mp3MuxContext *s, char *buf, int len,
			   unsigned int tag)
{
#if FS_WRITER
	ByteIOContext *pb = s->fs_writer_info.mp_fs_writer;
#else
	ByteIOContext *pb = s->stream_writer;
#endif

	writeBe32Stream(pb, tag);
	id3v2_put_size(s, len + 1);
	writeBe16Stream(pb, 0);
	writeByteStream(pb, 3); /* UTF-8 */
	writeBufferStream(pb, (cdx_int8 *)buf, len);
}

static AVMetadataTag *
av_metadata_get(AVMetadata *m, const char *key, const AVMetadataTag *prev, int flags)
{
	cdx_int32 j, k;
	const char *str;

	if (!m)
		return NULL;

	if (prev)
		j = prev - m->elems + 1;
	else
		j = 0;

	for (; j < m->count; j++) {
		str = m->elems[j].key;
		if (flags & CDX_AV_METADATA_MATCH_CASE)
			for (k = 0; key[k] == str[k] && key[k]; k++);
		else
			for (k = 0; toupper(key[k]) == toupper(str[k]) && key[k]; k++);
		if (key[k])
			continue;
		if (str[k] && !(flags & CDX_AV_METADATA_IGNORE_SUFFIX))
			continue;
		return &m->elems[j];
	}
	return NULL;
}

static int id3v1_set_string(Mp3MuxContext *s, const char *key,
			    unsigned char *buf, int buf_size)
{
	AVMetadataTag *tag;
	if ((tag = av_metadata_get(s->metadata, key, NULL, 0)))
		strncpy((char *)buf, tag->value, buf_size);
	return !!tag;
}

static int id3v1_create_tag(Mp3MuxContext *s, unsigned char *buf)
{
	AVMetadataTag *tag;
	int i, count = 0;

	memset(buf, 0, ID3v1_TAG_SIZE); /* fail safe */
	buf[0] = 'T';
	buf[1] = 'A';
	buf[2] = 'G';
	/* set mp3 author */
	buf[33] = 'A';
	buf[34] = 'l';
	buf[35] = 'l';
	buf[36] = 'w';
	buf[37] = 'i';
	buf[38] = 'n';
	buf[39] = 'n';
	buf[40] = 'e';
	buf[41] = 'r';
	count += id3v1_set_string(s, "title",   buf +  3, 30);
	count += id3v1_set_string(s, "author",  buf + 33, 30);
	count += id3v1_set_string(s, "album",   buf + 63, 30);
	count += id3v1_set_string(s, "date",    buf + 93,  4);
	count += id3v1_set_string(s, "comment", buf + 97, 30);
	if ((tag = av_metadata_get(s->metadata, "track", NULL, 0))) {
		buf[125] = 0;
		buf[126] = atoi(tag->value);
		count++;
	}
	buf[127] = 0xFF; /* default to unknown genre */
	if ((tag = av_metadata_get(s->metadata, "genre", NULL, 0))) {
		for (i = 0; i <= ID3v1_GENRE_MAX; i++) {
			if (!strcasecmp(tag->value, ff_id3v1_genre_str[i])) {
				buf[127] = i;
				count++;
				break;
			}
		}
	}
	return count;
}

/**
 * Write an ID3v2.4 header at beginning of stream
 */
cdx_int32 Mp3WriteHeader(Mp3MuxContext *s)
{
	AVMetadataTag *t = NULL;
#if FS_WRITER
	ByteIOContext *pb = s->fs_writer_info.mp_fs_writer;
#else
	ByteIOContext *pb = s->stream_writer;
#endif

	cdx_int32 i, ret;
	int totlen = 0;
	int64_t size_pos, cur_pos;

	writeBe32Stream(pb, MKBETAG('I', 'D', '3', 0x04)); /* ID3v2.4 */
	writeByteStream(pb, 0);
	writeByteStream(pb, 0); /* flags */

	/* reserve space for size */
#if FS_WRITER
	size_pos = pb->fsTell(pb);
#else
	size_pos = CdxStreamTell(pb);
#endif
	writeBe32Stream(pb, 0);

	while ((t = av_metadata_get(s->metadata, "", t, CDX_AV_METADATA_IGNORE_SUFFIX))) {
		unsigned int tag = 0;

		if (t->key[0] == 'T' && strlen(t->key) == 4) {
			int i;
			for (i = 0; *ff_id3v2_tags[i]; i++)
				if (AV_RB32(t->key) == AV_RB32(ff_id3v2_tags[i])) {
					int len = strlen(t->value);
					tag = AV_RB32(t->key);
					totlen += len + ID3v2_HEADER_SIZE + 2;
					id3v2_put_ttag(s, t->value, len + 1, tag);
					break;
				}
		}

		if (!tag) { /* unknown tag, write as TXXX frame */
			int   len = strlen(t->key), len1 = strlen(t->value);
			char *buf = malloc(len + len1 + 2);
			if (!buf)
				return -ENOMEM;
			tag = MKBETAG('T', 'X', 'X', 'X');
			strcpy(buf,           t->key);
			strcpy(buf + len + 1, t->value);
			id3v2_put_ttag(s, buf, len + len1 + 2, tag);
			totlen += len + len1 + ID3v2_HEADER_SIZE + 3;
			free(buf);
		}
	}

#if FS_WRITER
	cur_pos = pb->fsTell(pb);
	pb->fsSeek(pb, size_pos, SEEK_SET);
#else
	cur_pos = CdxStreamTell(pb);
	CdxStreamSeek(pb, size_pos, SEEK_SET);
#endif

	id3v2_put_size(s, totlen);

#if FS_WRITER
	pb->fsSeek(pb, cur_pos, SEEK_SET);
#else
	CdxStreamSeek(pb, cur_pos, SEEK_SET);
#endif

	return 0;
}

cdx_int32 Mp3WriteTrailer(Mp3MuxContext *s)
{
#if FS_WRITER
	ByteIOContext *pb = s->fs_writer_info.mp_fs_writer;
#else
	ByteIOContext *pb = s->stream_writer;
#endif

	unsigned char buf[ID3v1_TAG_SIZE];

	/* write the id3v1 tag */
	if (id3v1_create_tag(s, buf) > 0) {
		writeBufferStream(pb, (cdx_int8 *)buf, ID3v1_TAG_SIZE);
	}
	return 0;
}
