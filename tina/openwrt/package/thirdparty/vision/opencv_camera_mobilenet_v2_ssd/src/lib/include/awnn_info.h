#ifndef __AWNN_INFO_H__
#define __AWNN_INFO_H__

#ifdef __cplusplus
       extern "C" {
#endif

typedef struct awnn_info {
    const char *name;              // 模型名称
    const char md5[33];            // 模型文件MD5值
    int width;                     // 模型输入宽
    int height;                    // 模型输入高
    unsigned int mem_size;         // 模型占用内存
    float thresh;                  // 模型后处理推荐阈值
} awnn_info_t;

// 输入nbg路径，返回awnn_info_t结构体。包含库名称，网络输入图像宽高，内存占用大小，阈值信息等
awnn_info_t *awnn_get_info(char *nbg);

#ifdef __cplusplus
}
#endif

#endif
