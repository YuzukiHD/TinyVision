#ifndef __AWNN_H__
#define __AWNN_H__

#include "awnn_det.h"
#include "awnn_info.h"
#include "awnn_lib.h"
#include "cross_utils.h"

/**
 * @brief version code
 * 1.0.0 support awnnlib and detect network
 * 1.0.1 awnn_lib:v1.0.1:fix int8 quantize, support dump io;info:v1.0.1: read info from nbinfo
 * 1.0.2 info:v1.0.2: add attr weak for openssl/md5
 * 1.0.3 awnn_lib:v1.0.2: add interface set input/get output
 * 1.0.4 awnn_lib:v1.0.3: fix bug read platform
 * 1.0.5 info:v1.0.3: make some function static
 * 1.0.6 awnn_lib:1.0.4 fix efuse hex calc error
 */
#define AWNN_VERSION "AWNN_VERSION_1.0.6"

#endif
