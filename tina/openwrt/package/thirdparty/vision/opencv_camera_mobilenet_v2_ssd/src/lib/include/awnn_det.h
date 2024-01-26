#ifndef __AWNN_DET_H__
#define __AWNN_DET_H__

#ifdef __cplusplus
       extern "C" {
#endif

#include "awnn_lib.h"

typedef enum {
    AWNN_DET_POST_HUMANOID_1 = 1,   // 对应人形1.0版本
    AWNN_DET_POST_HUMANOID_2 = 2,   // 对应人形2.0版本
    AWNN_DET_POST_HUMANOID_3 = 3,   // 对应人形3.0版本
    AWNN_DET_POST_FACE_1 = 4        // 对应人脸1.0版本
} AWNN_DET_POST_TYPE;

typedef struct
{
    AWNN_DET_POST_TYPE type;        // 检测后处理类型，根据模型版本分为
    int width;                      // 输入图像宽度
    int height;                     // 输入图像高度
    float thresh;                   // 检测后处理阈值
} Awnn_Post_t;

typedef struct
{
    int xmin; // 左侧坐标
    int ymin; // 顶部坐标
    int xmax; // 右侧坐标
    int ymax; // 底部坐标
    int label; // 标签：人形（脸）:0，非人形（脸）>0。
    float score; // 概率值
    int landmark_x[5]; // 五官坐标x，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
    int landmark_y[5]; // 五官坐标y，分别对应左眼、右眼、鼻子、嘴左侧，嘴右侧
} BBoxRect_t;

typedef struct
{
    int valid_cnt;                  // 检测结果数量
    BBoxRect_t boxes[100];          // 检测结果目标信息
} Awnn_Result_t;

// AW人形、人脸检测模型后处理接口
void awnn_det_post(Awnn_Context_t *context, Awnn_Post_t *post, Awnn_Result_t *res);

#ifdef __cplusplus
}
#endif

#endif