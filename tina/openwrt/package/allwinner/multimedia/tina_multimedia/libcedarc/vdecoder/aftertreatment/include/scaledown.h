/*
 * =====================================================================================
 *
 *       Filename:  scaledown.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年07月17日 16时33分44秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *        Company:
 *
 * =====================================================================================
 */
#include "sc_interface.h"
#include "veInterface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDParam {
    int buf_size;
} SDParam;
/*
typedef struct PictureData {
	int nWidth;
	int nHeight;
	unsigned int phyYBufAddr;
	unsigned int phyCBufAddr;
	int ePixelFormat;
} PictureData;
*/


typedef void* SDInstant;

SDInstant SDCreate(struct VeOpsS* veops, void* veself, struct ScMemOpsS* memops);
SDInstant SDCreate2(void);
int SDPrePare(SDInstant pself, SDParam* param);
int SDProcess(SDInstant pself, VideoPicture* input, VideoPicture* output);
void SDDestory(SDInstant pself);

#ifdef __cplusplus
}
#endif
